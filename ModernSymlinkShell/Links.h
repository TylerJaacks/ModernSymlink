#pragma once

// Link creation shared by the shell extension and its elevated helper
// (ModernSymlinkApp.exe includes this header directly). Keeping the link calls,
// the helper's command-line switches and the "elevation fixes this" errors in
// one table means the two executables cannot disagree about any of them.

#include <windows.h>
#include <winioctl.h>

#include <string>
#include <vector>

enum class LinkKind
{
	SymbolicLink,            // to a file
	DirectorySymbolicLink,   // to a folder
	HardLink,                // files only
	Junction                 // folders only, local or mapped drive targets
};

struct LinkKindInfo
{
	LinkKind kind;

	// command-line switch of the elevated helper: <switch> "<link>" "<source>"
	PCWSTR helperSwitch;

	// the failure that running elevated can fix; anything else (name taken,
	// cross-volume, FAT, ...) fails the same way elevated
	DWORD errorFixedByElevation;
};

inline constexpr LinkKindInfo c_linkKinds[] =
{
	// symbolic links need SeCreateSymbolicLinkPrivilege unless Developer Mode is on
	{ LinkKind::SymbolicLink,          L"--symlink",     ERROR_PRIVILEGE_NOT_HELD },
	{ LinkKind::DirectorySymbolicLink, L"--symlink-dir", ERROR_PRIVILEGE_NOT_HELD },

	// hard links and junctions need no privilege, only write access to the target folder
	{ LinkKind::HardLink,              L"--hardlink",    ERROR_ACCESS_DENIED },
	{ LinkKind::Junction,              L"--junction",    ERROR_ACCESS_DENIED },
};

inline const LinkKindInfo& GetLinkKindInfo(LinkKind kind)
{
	for (const LinkKindInfo& info : c_linkKinds)
	{
		if (info.kind == kind)
		{
			return info;
		}
	}

	return c_linkKinds[0];   // unreachable: every LinkKind has a row
}

inline const LinkKindInfo* FindLinkKindBySwitch(PCWSTR helperSwitch)
{
	for (const LinkKindInfo& info : c_linkKinds)
	{
		if (wcscmp(info.helperSwitch, helperSwitch) == 0)
		{
			return &info;
		}
	}

	return nullptr;
}

// ---------------------------------------------------------------------------
// Junctions. Windows has no Win32 call for them: a junction is an empty
// directory carrying an IO_REPARSE_TAG_MOUNT_POINT reparse point whose data
// holds the target twice, as the NT path ("\??\C:\target") the file system
// follows and as the plain path ("C:\target") tools display. This is what
// "mklink /J" does. Junctions always resolve on the local machine, so the
// target must be on a local or mapped drive letter, not a UNC path.
// ---------------------------------------------------------------------------

// Mount-point layout of the kernel's REPARSE_DATA_BUFFER (ntifs.h), which
// user-mode headers do not define.
struct MountPointReparseBuffer
{
	DWORD ReparseTag;
	WORD ReparseDataLength;
	WORD Reserved;
	WORD SubstituteNameOffset;
	WORD SubstituteNameLength;
	WORD PrintNameOffset;
	WORD PrintNameLength;
	WCHAR PathBuffer[1];
};

// size of the fields before the tag-specific data (ReparseTag, ReparseDataLength, Reserved)
inline constexpr DWORD c_cbReparseHeader = sizeof(DWORD) + 2 * sizeof(WORD);

// Creates the directory "link" as a junction to the existing directory "target".
// Win32 style: returns FALSE and sets the last error on failure, e.g.
//   ERROR_ALREADY_EXISTS     link already exists
//   ERROR_DIRECTORY          target is not a directory
//   ERROR_NOT_SUPPORTED      target is a UNC path, or the volume cannot hold junctions (FAT)
//   ERROR_ACCESS_DENIED      no write access to the link's parent folder
// A half-made link directory is removed again on failure.
inline BOOL CreateDirectoryJunction(PCWSTR link, PCWSTR target)
{
	// full, normalized target path
	const DWORD cchFull = GetFullPathNameW(target, 0, nullptr, nullptr);

	if (cchFull == 0)
	{
		return FALSE;
	}

	std::wstring full(cchFull, L'\0');

	if (GetFullPathNameW(target, cchFull, full.data(), nullptr) == 0)
	{
		return FALSE;
	}

	full.resize(wcslen(full.c_str()));

	// "\\server\share" and "\\?\" forms cannot be junction targets
	if (full.size() >= 2 && full[0] == L'\\' && full[1] == L'\\')
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	// keep the trailing backslash only for a drive root ("C:\")
	if (full.size() > 3 && full.back() == L'\\')
	{
		full.pop_back();
	}

	const DWORD attributes = GetFileAttributesW(full.c_str());

	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		return FALSE;
	}

	if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		SetLastError(ERROR_DIRECTORY);
		return FALSE;
	}

	// reparse data: substitute name and print name, each NUL-terminated
	const std::wstring substituteName = L"\\??\\" + full;
	const std::wstring& printName = full;

	const size_t cbSubstitute = substituteName.size() * sizeof(WCHAR);
	const size_t cbPrint = printName.size() * sizeof(WCHAR);
	const size_t cbData = 4 * sizeof(WORD) + cbSubstitute + sizeof(WCHAR) + cbPrint + sizeof(WCHAR);

	if (c_cbReparseHeader + cbData > MAXIMUM_REPARSE_DATA_BUFFER_SIZE)
	{
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return FALSE;
	}

	std::vector<BYTE> buffer(c_cbReparseHeader + cbData, 0);

	auto* reparse = reinterpret_cast<MountPointReparseBuffer*>(buffer.data());

	reparse->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	reparse->ReparseDataLength = static_cast<WORD>(cbData);
	reparse->SubstituteNameOffset = 0;
	reparse->SubstituteNameLength = static_cast<WORD>(cbSubstitute);
	reparse->PrintNameOffset = static_cast<WORD>(cbSubstitute + sizeof(WCHAR));
	reparse->PrintNameLength = static_cast<WORD>(cbPrint);

	BYTE* paths = reinterpret_cast<BYTE*>(reparse->PathBuffer);

	memcpy(paths, substituteName.c_str(), cbSubstitute);
	memcpy(paths + reparse->PrintNameOffset, printName.c_str(), cbPrint);

	// the junction is an empty directory...
	if (!CreateDirectoryW(link, nullptr))
	{
		return FALSE;
	}

	// ...with the reparse point written onto it
	HANDLE hLink = CreateFileW(link, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	DWORD error = ERROR_SUCCESS;

	if (hLink == INVALID_HANDLE_VALUE)
	{
		error = GetLastError();
	}
	else
	{
		DWORD cbReturned = 0;

		if (!DeviceIoControl(hLink, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()),
			nullptr, 0, &cbReturned, nullptr))
		{
			// ERROR_INVALID_FUNCTION on FAT: report it as unsupported
			error = GetLastError();
			error = (error == ERROR_INVALID_FUNCTION) ? ERROR_NOT_SUPPORTED : error;
		}

		CloseHandle(hLink);
	}

	if (error != ERROR_SUCCESS)
	{
		RemoveDirectoryW(link);
		SetLastError(error);

		return FALSE;
	}

	return TRUE;
}

// ---------------------------------------------------------------------------

// Creates a link of the given kind at "link" pointing at "source".
// Win32 style: returns FALSE and sets the last error on failure.
inline BOOL CreateLink(LinkKind kind, PCWSTR link, PCWSTR source)
{
	// harmless when the caller holds the privilege; lets an unprivileged
	// caller create symbolic links when Developer Mode is on
	constexpr DWORD c_symbolicLinkFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

	switch (kind)
	{
	case LinkKind::SymbolicLink:
		return CreateSymbolicLinkW(link, source, c_symbolicLinkFlags) != FALSE;

	case LinkKind::DirectorySymbolicLink:
		return CreateSymbolicLinkW(link, source, c_symbolicLinkFlags | SYMBOLIC_LINK_FLAG_DIRECTORY) != FALSE;

	case LinkKind::HardLink:
		return CreateHardLinkW(link, source, nullptr);

	case LinkKind::Junction:
		return CreateDirectoryJunction(link, source);
	}

	SetLastError(ERROR_INVALID_PARAMETER);

	return FALSE;
}
