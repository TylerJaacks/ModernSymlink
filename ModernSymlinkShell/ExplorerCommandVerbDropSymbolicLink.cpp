#include "pch.h"

#include "ExplorerCommands.h"

CExplorerCommandVerbDropSymbolicLink::CExplorerCommandVerbDropSymbolicLink(CExplorerCommandVerb& theParent) :
	CExplorerSubCommandVerb(theParent, Target::Directory,
		L"Symbolic Link",
		L"Create a symbolic link to the picked source here",
		__uuidof(CExplorerCommandVerbDropSymbolicLink))
{
}

HRESULT CExplorerCommandVerbDropSymbolicLink::OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	UNREFERENCED_PARAMETER(pBindContext);

	return DropLink(pShellItemArray, LinkKind::SymbolicLink, LinkKind::DirectorySymbolicLink);
}
