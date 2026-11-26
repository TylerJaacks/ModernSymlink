#include "pch.h"
#include "MenuState.h"

std::wstring GetFirstItemPath(IShellItemArray* pShellItemArray)
{
	std::wstring path;

	IShellItem* pShellItem = nullptr;

	if (SUCCEEDED(GetItemAt(pShellItemArray, 0, IID_PPV_ARGS(&pShellItem))))
	{
		PWSTR pszPath = nullptr;

		if (SUCCEEDED(pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
		{
			path = pszPath;

			CoTaskMemFree(pszPath);
		}

		pShellItem->Release();
	}

	return path;
}

bool IsFolderItem(IShellItemArray* pShellItemArray)
{
	IShellItem* pShellItem = nullptr;

	if (FAILED(GetItemAt(pShellItemArray, 0, IID_PPV_ARGS(&pShellItem))))
	{
		return false;
	}

	// SFGAO_FOLDER alone is also set on zip files and other "browsable" files
	// (those carry SFGAO_STREAM as well) and on virtual folders such as
	// "This PC" or a library (those lack SFGAO_FILESYSTEM). A real directory
	// is a filesystem folder that is not a stream.
	SFGAOF attributes = 0;

	const bool isFolder =
		SUCCEEDED(pShellItem->GetAttributes(SFGAO_FOLDER | SFGAO_STREAM | SFGAO_FILESYSTEM, &attributes)) &&
		(attributes & SFGAO_FOLDER) &&
		(attributes & SFGAO_FILESYSTEM) &&
		!(attributes & SFGAO_STREAM);

	pShellItem->Release();

	return isFolder;
}

bool IsFilesystemItem(IShellItemArray* pShellItemArray)
{
	return !GetFirstItemPath(pShellItemArray).empty();
}

bool IsSingleDirectoryTarget(IShellItemArray* pShellItemArray)
{
	DWORD count = 0;

	if (!pShellItemArray || FAILED(pShellItemArray->GetCount(&count)) || count != 1)
	{
		return false;
	}

	return IsFolderItem(pShellItemArray);
}
