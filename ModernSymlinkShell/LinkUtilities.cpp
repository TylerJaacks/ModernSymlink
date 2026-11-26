#include "pch.h"

#include "LinkUtilities.h"

#include <appmodel.h>

// the elevated helper is the package's own app executable
static constexpr PCWSTR c_szHelperRelativePath = L"\\ModernSymlinkApp\\ModernSymlinkApp.exe";

static std::wstring GetHelperPath()
{
	// the surrogate hosting this DLL carries the package identity, so the
	// package root is known
	UINT32 cch = 0;

	if (GetCurrentPackagePath(&cch, nullptr) != ERROR_INSUFFICIENT_BUFFER || cch == 0)
	{
		return {};
	}

	std::wstring root(cch, L'\0');

	if (GetCurrentPackagePath(&cch, root.data()) != ERROR_SUCCESS)
	{
		return {};
	}

	root.resize(wcslen(root.c_str()));

	return root + c_szHelperRelativePath;
}

// Launches the helper elevated with the given parameters, waits for it and
// returns its exit code (a Win32 error) as an HRESULT. hrNoHelper is returned
// when there is no helper to launch (not running from the package).
static HRESULT RunElevatedHelper(HWND hwndOwner, const std::wstring& parameters, HRESULT hrNoHelper)
{
	const std::wstring helper = GetHelperPath();

	if (helper.empty() || !PathFileExistsW(helper.c_str()))
	{
		return hrNoHelper;
	}

	SHELLEXECUTEINFOW sei = { .cbSize = sizeof(sei) };

	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
	sei.hwnd = hwndOwner;
	sei.lpVerb = L"runas";
	sei.lpFile = helper.c_str();
	sei.lpParameters = parameters.c_str();
	sei.nShow = SW_HIDE;

	if (!ShellExecuteExW(&sei))
	{
		// ERROR_CANCELLED when the user declines the UAC prompt
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if (!sei.hProcess)
	{
		return E_UNEXPECTED;
	}

	WaitForSingleObject(sei.hProcess, INFINITE);

	DWORD exitCode = ERROR_GEN_FAILURE;

	if (!GetExitCodeProcess(sei.hProcess, &exitCode))
	{
		exitCode = GetLastError();
	}

	CloseHandle(sei.hProcess);

	return HRESULT_FROM_WIN32(exitCode);
}

HRESULT CreateLinkWithElevation(HWND hwndOwner, LinkKind kind, const std::wstring& linkPath, const std::wstring& source)
{
	if (CreateLink(kind, linkPath.c_str(), source.c_str()))
	{
		return S_OK;
	}

	const DWORD error = GetLastError();
	const LinkKindInfo& info = GetLinkKindInfo(kind);

	if (error != info.errorFixedByElevation)
	{
		return HRESULT_FROM_WIN32(error);
	}

	const std::wstring parameters = std::wstring(info.helperSwitch) + L" \"" + linkPath + L"\" \"" + source + L"\"";

	return RunElevatedHelper(hwndOwner, parameters, HRESULT_FROM_WIN32(error));
}
