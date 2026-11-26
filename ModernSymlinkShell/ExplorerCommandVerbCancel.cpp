#include "pch.h"

#include "ExplorerCommands.h"

CExplorerCommandVerbCancel::CExplorerCommandVerbCancel(CExplorerCommandVerb& theParent) :
	CExplorerSubCommandVerb(theParent, Target::Any,
		L"Cancel Link Creation",
		L"Forget the picked link source",
		__uuidof(CExplorerCommandVerbCancel))
{
}

HRESULT CExplorerCommandVerbCancel::OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	UNREFERENCED_PARAMETER(pShellItemArray);
	UNREFERENCED_PARAMETER(pBindContext);

	StateStore::Clear();

	return S_OK;
}
