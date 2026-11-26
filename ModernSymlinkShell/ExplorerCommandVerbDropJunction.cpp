#include "pch.h"

#include "ExplorerCommands.h"

CExplorerCommandVerbDropJunction::CExplorerCommandVerbDropJunction(CExplorerCommandVerb& theParent) :
	CExplorerSubCommandVerb(theParent, Target::Directory,
		L"Junction",
		L"Create a directory junction to the picked source folder here",
		__uuidof(CExplorerCommandVerbDropJunction))
{
}

HRESULT CExplorerCommandVerbDropJunction::OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	UNREFERENCED_PARAMETER(pBindContext);

	// folders only
	return DropLink(pShellItemArray, std::nullopt, LinkKind::Junction);
}
