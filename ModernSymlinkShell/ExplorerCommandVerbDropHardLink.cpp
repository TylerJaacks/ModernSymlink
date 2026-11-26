#include "pch.h"

#include "ExplorerCommands.h"

CExplorerCommandVerbDropHardLink::CExplorerCommandVerbDropHardLink(CExplorerCommandVerb& theParent) :
	CExplorerSubCommandVerb(theParent, Target::Directory,
		L"Hard Link",
		L"Create a hard link to the picked source file here",
		__uuidof(CExplorerCommandVerbDropHardLink))
{
}

HRESULT CExplorerCommandVerbDropHardLink::OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	UNREFERENCED_PARAMETER(pBindContext);

	// files only
	return DropLink(pShellItemArray, LinkKind::HardLink, std::nullopt);
}
