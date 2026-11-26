// CExplorerCommandVerb is the single entry this DLL adds to the context menu. Its shape
// depends on whether a drop is possible for the current right-click (see ExplorerCommands.h):
// "Pick Link Source" when it is not, the "Drop As..." submenu when it is.

#include "pch.h"

#include "Dll.h"
#include "ExplorerCommands.h"
#include "ShellResource.h"

static constexpr WCHAR c_szDropAsDisplayName[] = L"Drop As...";
static constexpr WCHAR c_szDropAsToolTip[] = L"Create a link to the picked source in this folder";

// indices into _subCommands
static constexpr size_t c_iPickLinkSource = 0;
static constexpr size_t c_iDropSymbolicLink = 1;
static constexpr size_t c_iDropHardLink = 2;
static constexpr size_t c_iDropJunction = 3;
static constexpr size_t c_iCancel = 4;

// the Drop As submenu for each kind of picked source, in display order
static constexpr size_t c_fileSubCommands[] = { c_iDropHardLink, c_iDropSymbolicLink, c_iCancel };
static constexpr size_t c_folderSubCommands[] = { c_iDropSymbolicLink, c_iDropJunction, c_iCancel };

CExplorerCommandVerb::CExplorerCommandVerb() :
	_cRef(1),
	_pUnknownSite(nullptr),
	_modeResolved(false),
	_mode(MenuState::NothingPicked),
	_pLastItems(nullptr),
	_siteItemsResolved(false),
	_pSiteItems(nullptr),
	_subCommands{}
{
	_subCommands[c_iPickLinkSource] = new (std::nothrow) CExplorerCommandVerbPickLinkSource(*this);
	_subCommands[c_iDropSymbolicLink] = new (std::nothrow) CExplorerCommandVerbDropSymbolicLink(*this);
	_subCommands[c_iDropHardLink] = new (std::nothrow) CExplorerCommandVerbDropHardLink(*this);
	_subCommands[c_iDropJunction] = new (std::nothrow) CExplorerCommandVerbDropJunction(*this);
	_subCommands[c_iCancel] = new (std::nothrow) CExplorerCommandVerbCancel(*this);

	DllAddRef();
}

CExplorerCommandVerb::~CExplorerCommandVerb()
{
	for (IExplorerCommand*& pSubCommand : _subCommands)
	{
		SafeRelease(&pSubCommand);
	}

	SafeRelease(&_pLastItems);
	SafeRelease(&_pSiteItems);
	SafeRelease(&_pUnknownSite);

	DllRelease();
}

// IUnknown
IFACEMETHODIMP CExplorerCommandVerb::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qiTab[] = {
		QITABENT(CExplorerCommandVerb, IExplorerCommand),
		QITABENT(CExplorerCommandVerb, IObjectWithSite),
		{ .piid = nullptr }
	};

	return QISearch(this, qiTab, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CExplorerCommandVerb::AddRef()
{
	return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) CExplorerCommandVerb::Release()
{
	const long cRef = InterlockedDecrement(&_cRef);

	if (!cRef)
	{
		delete this;
	}

	return cRef;
}

// ---------------------------------------------------------------------------
// Items. Explorer passes the right-clicked items to most methods, nothing for
// a background click, and nothing at all to GetFlags / EnumSubCommands. The
// site (the folder view hosting the menu, the same route Windows Terminal's
// "Open in Terminal" uses) fills the gaps: the current selection if there is
// one, otherwise the folder being viewed.
// ---------------------------------------------------------------------------
static IFolderView* GetFolderViewFromSite(IUnknown* pSite)
{
	if (!pSite)
	{
		return nullptr;
	}

	IFolderView* pFolderView = nullptr;

	// the view hosting the context menu...
	if (SUCCEEDED(IUnknown_QueryService(pSite, SID_SFolderView, IID_PPV_ARGS(&pFolderView))))
	{
		return pFolderView;
	}

	// ...or, failing that, the active view of the top-level browser
	IShellBrowser* pShellBrowser = nullptr;

	if (SUCCEEDED(IUnknown_QueryService(pSite, SID_STopLevelBrowser, IID_PPV_ARGS(&pShellBrowser))))
	{
		IShellView* pShellView = nullptr;

		if (SUCCEEDED(pShellBrowser->QueryActiveShellView(&pShellView)))
		{
			pShellView->QueryInterface(IID_PPV_ARGS(&pFolderView));

			pShellView->Release();
		}

		pShellBrowser->Release();
	}

	return pFolderView;
}

static IShellItemArray* CreateItemArrayFromSite(IUnknown* pSite)
{
	IFolderView* pFolderView = GetFolderViewFromSite(pSite);

	if (!pFolderView)
	{
		return nullptr;
	}

	IShellItemArray* pItems = nullptr;

	// the selection, if any
	DWORD count = 0;

	if (SUCCEEDED(pFolderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&pItems))) && pItems)
	{
		if (FAILED(pItems->GetCount(&count)) || count == 0)
		{
			SafeRelease(&pItems);
		}
	}

	// otherwise the folder being viewed (background click)
	if (!pItems)
	{
		IShellItem* pFolder = nullptr;

		if (FAILED(pFolderView->GetFolder(IID_PPV_ARGS(&pFolder))))
		{
			// older views only answer IPersistFolder2; go through the pidl
			IPersistFolder2* pPersistFolder = nullptr;

			if (SUCCEEDED(pFolderView->GetFolder(IID_PPV_ARGS(&pPersistFolder))))
			{
				PIDLIST_ABSOLUTE pidl = nullptr;

				if (SUCCEEDED(pPersistFolder->GetCurFolder(&pidl)))
				{
					SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&pFolder));

					ILFree(pidl);
				}

				pPersistFolder->Release();
			}
		}

		if (pFolder)
		{
			SHCreateShellItemArrayFromShellItem(pFolder, IID_PPV_ARGS(&pItems));

			pFolder->Release();
		}
	}

	pFolderView->Release();

	return pItems;
}

IShellItemArray* CExplorerCommandVerb::EffectiveItems(IShellItemArray* pShellItemArray)
{
	if (pShellItemArray)
	{
		// remember the items Explorer gave us for the calls that get none
		if (pShellItemArray != _pLastItems)
		{
			SafeRelease(&_pLastItems);

			_pLastItems = pShellItemArray;
			_pLastItems->AddRef();
		}

		return pShellItemArray;
	}

	if (_pLastItems)
	{
		return _pLastItems;
	}

	if (!_siteItemsResolved)
	{
		_pSiteItems = CreateItemArrayFromSite(_pUnknownSite);
		_siteItemsResolved = true;
	}

	return _pSiteItems;
}

// ---------------------------------------------------------------------------
// Shape of the entry
// ---------------------------------------------------------------------------
MenuState CExplorerCommandVerb::ResolveMode()
{
	if (!_modeResolved)
	{
		_linkState = StateStore::Load();
		_mode = MenuStateFor(_linkState);
		_modeResolved = true;
	}

	return _mode;
}

bool CExplorerCommandVerb::ShowsDropMenu(IShellItemArray* pItems)
{
	// a drop needs a picked source and a single real directory to drop into
	return IsExpanded(ResolveMode()) && IsSingleDirectoryTarget(pItems);
}

IExplorerCommand* CExplorerCommandVerb::PickVerbUnlessDropping(IShellItemArray* pItems)
{
	return ShowsDropMenu(pItems) ? nullptr : _subCommands[c_iPickLinkSource];
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// Icon location string ("<this DLL's path>,-<resource id>") of an icon
// embedded in this DLL, which Explorer loads itself.
static HRESULT GetModuleIconLocation(int resourceId, LPWSTR* ppszIcon)
{
	*ppszIcon = nullptr;

	WCHAR modulePath[MAX_PATH];

	const DWORD cch = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, ARRAYSIZE(modulePath));

	if (cch == 0 || cch == ARRAYSIZE(modulePath))
	{
		return cch == 0 ? HRESULT_FROM_WIN32(GetLastError()) : HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	}

	WCHAR location[MAX_PATH + 16];

	const HRESULT hr = StringCchPrintfW(location, ARRAYSIZE(location), L"%s,-%d", modulePath, resourceId);

	return SUCCEEDED(hr) ? SHStrDupW(location, ppszIcon) : hr;
}

// IExplorerCommand
IFACEMETHODIMP CExplorerCommandVerb::GetTitle(IShellItemArray* pShellItemArray, LPWSTR* ppszName)
{
	IShellItemArray* pItems = EffectiveItems(pShellItemArray);

	if (IExplorerCommand* pPick = PickVerbUnlessDropping(pItems))
	{
		return pPick->GetTitle(pItems, ppszName);
	}

	return SHStrDup(c_szDropAsDisplayName, ppszName);
}

IFACEMETHODIMP CExplorerCommandVerb::GetIcon(IShellItemArray* pShellItemArray, LPWSTR* ppszIcon)
{
	UNREFERENCED_PARAMETER(pShellItemArray);

	// the ModernSymlink logo, in both shapes
	return GetModuleIconLocation(IDI_MODERNSYMLINK, ppszIcon);
}

IFACEMETHODIMP CExplorerCommandVerb::GetToolTip(IShellItemArray* pShellItemArray, LPWSTR* ppszInfotip)
{
	IShellItemArray* pItems = EffectiveItems(pShellItemArray);

	if (IExplorerCommand* pPick = PickVerbUnlessDropping(pItems))
	{
		return pPick->GetToolTip(pItems, ppszInfotip);
	}

	return SHStrDup(c_szDropAsToolTip, ppszInfotip);
}

IFACEMETHODIMP CExplorerCommandVerb::GetCanonicalName(GUID* pGuidCommandName)
{
	*pGuidCommandName = CLSID_ModernSymlinkVerb;

	return S_OK;
}

IFACEMETHODIMP CExplorerCommandVerb::GetState(IShellItemArray* pShellItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState)
{
	IShellItemArray* pItems = EffectiveItems(pShellItemArray);

	// GetState is called once per menu; re-read the persisted state here in
	// case Explorer reuses this instance for a later right-click
	_modeResolved = false;

	if (IExplorerCommand* pPick = PickVerbUnlessDropping(pItems))
	{
		// hidden on virtual items, otherwise available (re-pick is one click)
		return pPick->GetState(pItems, fOkToBeSlow, pCmdState);
	}

	*pCmdState = ECS_ENABLED;

	return S_OK;
}

IFACEMETHODIMP CExplorerCommandVerb::GetFlags(EXPCMDFLAGS* pFlags)
{
	*pFlags = ShowsDropMenu(EffectiveItems(nullptr)) ? ECF_HASSUBCOMMANDS : ECF_DEFAULT;

	return S_OK;
}

IFACEMETHODIMP CExplorerCommandVerb::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
	*ppEnum = nullptr;

	if (!ShowsDropMenu(EffectiveItems(nullptr)))
	{
		// "Pick Link Source" is a single item
		return E_NOTIMPL;
	}

	const std::span<const size_t> indices = (_mode == MenuState::ExpandedFolder)
		? std::span<const size_t>(c_folderSubCommands)
		: std::span<const size_t>(c_fileSubCommands);

	std::vector<IExplorerCommand*> visible;

	for (const size_t i : indices)
	{
		if (_subCommands[i])
		{
			visible.push_back(_subCommands[i]);
		}
	}

	*ppEnum = new (std::nothrow) CEnumExplorerCommand(visible.data(), static_cast<ULONG>(visible.size()));

	return *ppEnum ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP CExplorerCommandVerb::Invoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext)
{
	IShellItemArray* pItems = EffectiveItems(pShellItemArray);

	if (IExplorerCommand* pPick = PickVerbUnlessDropping(pItems))
	{
		return pPick->Invoke(pItems, pBindContext);
	}

	// the shell never invokes a verb that has sub-commands
	return E_NOTIMPL;
}

// IObjectWithSite
IFACEMETHODIMP CExplorerCommandVerb::SetSite(IUnknown* pUnknownSite)
{
	// a new site means whatever we derived from the old one is stale
	SafeRelease(&_pSiteItems);
	_siteItemsResolved = false;

	return SetInterface(&_pUnknownSite, pUnknownSite);
}

IFACEMETHODIMP CExplorerCommandVerb::GetSite(REFIID riid, void** ppv)
{
	*ppv = nullptr;

	return _pUnknownSite ? _pUnknownSite->QueryInterface(riid, ppv) : E_FAIL;
}

HWND CExplorerCommandVerb::GetWndHandle() const
{
	HWND hwnd = nullptr;

	if (_pUnknownSite)
	{
		IUnknown_GetWindow(_pUnknownSite, &hwnd);
	}

	return hwnd;
}

const std::optional<State>& CExplorerCommandVerb::GetLinkState() const
{
	return _linkState;
}

HRESULT CExplorerCommandVerb_CreateInstance(REFIID riid, void** ppv)
{
	*ppv = nullptr;

	CExplorerCommandVerb* pVerb = new (std::nothrow) CExplorerCommandVerb();

	if (!pVerb)
	{
		return E_OUTOFMEMORY;
	}

	const HRESULT hr = pVerb->QueryInterface(riid, ppv);

	pVerb->Release();

	return hr;
}
