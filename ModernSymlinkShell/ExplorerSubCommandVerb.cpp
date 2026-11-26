#include "pch.h"

#include "Dll.h"
#include "ExplorerCommands.h"
#include "LinkUtilities.h"

CExplorerSubCommandVerb::CExplorerSubCommandVerb(CExplorerCommandVerb& theParent, Target target, PCWSTR pszTitle, PCWSTR pszToolTip, REFGUID canonicalName) :
	Parent(theParent),
	_cRef(1),
	_target(target),
	_pszTitle(pszTitle),
	_pszToolTip(pszToolTip),
	_canonicalName(canonicalName)
{
	DllAddRef();
}

CExplorerSubCommandVerb::~CExplorerSubCommandVerb()
{
	DllRelease();
}

// IUnknown
IFACEMETHODIMP CExplorerSubCommandVerb::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qiTab[] = {
		QITABENT(CExplorerSubCommandVerb, IExplorerCommand),
		{ .piid = nullptr }
	};

	return QISearch(this, qiTab, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CExplorerSubCommandVerb::AddRef()
{
	return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) CExplorerSubCommandVerb::Release()
{
	const long cRef = InterlockedDecrement(&_cRef);

	if (!cRef)
	{
		delete this;
	}

	return cRef;
}

// IExplorerCommand
IFACEMETHODIMP CExplorerSubCommandVerb::GetTitle(IShellItemArray* pShellItemArray, LPWSTR* ppszName)
{
	UNREFERENCED_PARAMETER(pShellItemArray);

	return SHStrDup(_pszTitle, ppszName);
}

IFACEMETHODIMP CExplorerSubCommandVerb::GetIcon(IShellItemArray* pShellItemArray, LPWSTR* ppszIcon)
{
	UNREFERENCED_PARAMETER(pShellItemArray);

	*ppszIcon = nullptr;

	return E_NOTIMPL;
}

IFACEMETHODIMP CExplorerSubCommandVerb::GetToolTip(IShellItemArray* pShellItemArray, LPWSTR* ppszInfotip)
{
	UNREFERENCED_PARAMETER(pShellItemArray);

	return SHStrDup(_pszToolTip, ppszInfotip);
}

IFACEMETHODIMP CExplorerSubCommandVerb::GetCanonicalName(GUID* pGuidCommandName)
{
	*pGuidCommandName = _canonicalName;

	return S_OK;
}

IFACEMETHODIMP CExplorerSubCommandVerb::GetState(IShellItemArray* pShellItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState)
{
	UNREFERENCED_PARAMETER(fOkToBeSlow);

	// null for a background click: the current folder is the target
	IShellItemArray* pItems = Parent.EffectiveItems(pShellItemArray);

	bool applicable = true;

	switch (_target)
	{
	case Target::FilesystemItem:
		applicable = IsFilesystemItem(pItems);
		break;

	case Target::Directory:
		applicable = IsSingleDirectoryTarget(pItems);
		break;

	case Target::Any:
		break;
	}

	*pCmdState = applicable ? ECS_ENABLED : ECS_HIDDEN;

	return S_OK;
}

IFACEMETHODIMP CExplorerSubCommandVerb::GetFlags(EXPCMDFLAGS* pFlags)
{
	*pFlags = ECF_DEFAULT;

	return S_OK;
}

IFACEMETHODIMP CExplorerSubCommandVerb::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
	// leaf verbs have no sub-commands
	*ppEnum = nullptr;

	return E_NOTIMPL;
}

IFACEMETHODIMP CExplorerSubCommandVerb::Invoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	IShellItemArray* pItems = Parent.EffectiveItems(pShellItemArray);

	if (!pItems)
	{
		// nothing to act on (no items and no site folder)
		return E_INVALIDARG;
	}

	return OnInvoke(pItems, pBindContext);
}

HRESULT CExplorerSubCommandVerb::DropLink(IShellItemArray* pShellItemArray, std::optional<LinkKind> kindForFile, std::optional<LinkKind> kindForFolder)
{
	const std::optional<State>& state = Parent.GetLinkState();

	if (!state.has_value())
	{
		return E_UNEXPECTED;
	}

	const std::optional<LinkKind> kind = state->m_isFolder ? kindForFolder : kindForFile;

	if (!kind.has_value())
	{
		// the verb is not offered for this kind of source
		return E_UNEXPECTED;
	}

	// the new link lives in the right-clicked folder under the source's own name
	std::wstring linkPath = GetFirstItemPath(pShellItemArray);

	if (!linkPath.empty() && linkPath.back() != L'\\')
	{
		linkPath += L'\\';
	}

	linkPath += PathFindFileNameW(state->m_linkSource.c_str());

	const HRESULT hr = CreateLinkWithElevation(Parent.GetWndHandle(), *kind, linkPath, state->m_linkSource);

	if (SUCCEEDED(hr))
	{
		// show the new item without a refresh, and forget the pick
		SHChangeNotify(state->m_isFolder ? SHCNE_MKDIR : SHCNE_CREATE, SHCNF_PATHW, linkPath.c_str(), nullptr);

		StateStore::Clear();
	}

	return hr;
}
