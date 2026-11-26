#include "pch.h"

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void*)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}

	return TRUE;
}