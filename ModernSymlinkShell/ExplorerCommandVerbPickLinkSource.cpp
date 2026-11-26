#include "pch.h"

#include "ExplorerCommands.h"

CExplorerCommandVerbPickLinkSource::CExplorerCommandVerbPickLinkSource(CExplorerCommandVerb& theParent) :
	CExplorerSubCommandVerb(theParent, Target::FilesystemItem,
		L"Pick Link Source",
		L"Remember the selected item as the source for a link",
		__uuidof(CExplorerCommandVerbPickLinkSource))
{
}

HRESULT CExplorerCommandVerbPickLinkSource::OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	UNREFERENCED_PARAMETER(pBindContext);

	return StateStore::Save(State{ .m_linkSource = GetFirstItemPath(pShellItemArray), .m_isFolder = IsFolderItem(pShellItemArray) });
}
