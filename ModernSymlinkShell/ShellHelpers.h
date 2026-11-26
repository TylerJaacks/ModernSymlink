#pragma once

// The few COM and shell helpers this DLL uses, kept from the Windows SDK
// ExplorerCommandVerb sample's ShellHelpers.h.

#include <windows.h>
#include <shobjidl_core.h>

#pragma comment(lib, "shlwapi.lib")

// Gets item i of an array as the requested interface. Fails cleanly on a null array.
inline HRESULT GetItemAt(IShellItemArray* pShellItemArray, DWORD i, REFIID riid, void** ppv)
{
	*ppv = nullptr;

	IShellItem* pShellItem = nullptr;

	HRESULT hr = pShellItemArray ? pShellItemArray->GetItemAt(i, &pShellItem) : E_NOINTERFACE;

	if (SUCCEEDED(hr))
	{
		hr = pShellItem->QueryInterface(riid, ppv);

		pShellItem->Release();
	}

	return hr;
}

template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = nullptr;
	}
}

template <class T> HRESULT SetInterface(T** ppT, IUnknown* pUnknown)
{
	SafeRelease(ppT);

	return pUnknown ? pUnknown->QueryInterface(ppT) : E_NOINTERFACE;
}
