#pragma once
#include "pch.h"
#include "Links.h"
#include "MenuState.h"
#include "StateStore.h"

class CExplorerCommandVerb;

// ---------------------------------------------------------------------------
// Shared implementation of the leaf verbs. A verb passes its title, tooltip,
// canonical GUID and target requirement to the constructor and implements
// OnInvoke; everything else is here.
// ---------------------------------------------------------------------------
class CExplorerSubCommandVerb : public IExplorerCommand
{
public:
	// What the right-clicked items must be for the verb to show. Enforced by
	// GetState (which always receives the items), so it holds whatever order
	// Explorer calls the IExplorerCommand methods in.
	enum class Target
	{
		Any,            // always shown (Cancel)
		FilesystemItem, // the item has a filesystem path (Pick Link Source)
		Directory       // exactly one item, and it is a real directory (the drop verbs)
	};

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
	IFACEMETHODIMP_(ULONG) AddRef() override;
	IFACEMETHODIMP_(ULONG) Release() override;

	// IExplorerCommand
	IFACEMETHODIMP GetTitle(IShellItemArray* pShellItemArray, LPWSTR* ppszName) override;
	IFACEMETHODIMP GetIcon(IShellItemArray* pShellItemArray, LPWSTR* ppszIcon) override;
	IFACEMETHODIMP GetToolTip(IShellItemArray* pShellItemArray, LPWSTR* ppszInfotip) override;
	IFACEMETHODIMP GetCanonicalName(GUID* pGuidCommandName) override;
	IFACEMETHODIMP GetState(IShellItemArray* pShellItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState) override;
	IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override;
	IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override;

	// Explorer passes no item array for a background click. Invoke resolves
	// the items (the current folder, via the parent's site) and hands them to
	// OnInvoke, so the verbs never see a null array.
	IFACEMETHODIMP Invoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext) override;

protected:
	CExplorerSubCommandVerb(CExplorerCommandVerb& theParent, Target target, PCWSTR pszTitle, PCWSTR pszToolTip, REFGUID canonicalName);
	virtual ~CExplorerSubCommandVerb();

	// The verb's action; pShellItemArray is never null here.
	virtual HRESULT OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext) = 0;

	// For the drop verbs: creates a link to the picked source inside the
	// right-clicked folder, named after the source. kindForFile / kindForFolder
	// say which link to make for each kind of source (none: not offered). The
	// pick is forgotten only on success, so a failed drop can be retried.
	HRESULT DropLink(IShellItemArray* pShellItemArray, std::optional<LinkKind> kindForFile, std::optional<LinkKind> kindForFolder);

	CExplorerCommandVerb& Parent;

private:
	long _cRef;
	const Target _target;
	const PCWSTR _pszTitle;
	const PCWSTR _pszToolTip;
	const GUID _canonicalName;
};

// ---------------------------------------------------------------------------
// The leaf verbs: a constructor (strings, GUID, target) and OnInvoke each.
// ---------------------------------------------------------------------------
#define DECLARE_SUB_VERB(ClassName, Uuid)                                                       \
	class __declspec(uuid(Uuid)) ClassName final : public CExplorerSubCommandVerb               \
	{                                                                                           \
	public:                                                                                     \
		explicit ClassName(CExplorerCommandVerb& theParent);                                    \
	protected:                                                                                  \
		HRESULT OnInvoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext) override;    \
	}

// the entry itself while no drop is possible
DECLARE_SUB_VERB(CExplorerCommandVerbPickLinkSource,   "ADFFF516-F33D-43CE-83D0-9928AFEC2807");

// the "Drop As..." submenu
DECLARE_SUB_VERB(CExplorerCommandVerbDropSymbolicLink, "F334ADC8-C35C-499D-83C0-21C42F32EBFD");   // file and folder sources
DECLARE_SUB_VERB(CExplorerCommandVerbDropHardLink,     "EA3E0785-6A66-40E2-85ED-F9F835276288");   // file sources
DECLARE_SUB_VERB(CExplorerCommandVerbDropJunction,     "F1B0C8A3-9602-445F-99FF-D631A329ADE5");   // folder sources
DECLARE_SUB_VERB(CExplorerCommandVerbCancel,           "18E09B7A-23EE-425F-B102-E86FC2DE9FF4");   // last item of both

#undef DECLARE_SUB_VERB

// ---------------------------------------------------------------------------
// Enumerator handed to Explorer from CExplorerCommandVerb::EnumSubCommands.
// Holds its own references to the commands it enumerates.
// ---------------------------------------------------------------------------
class CEnumExplorerCommand final : public IEnumExplorerCommand
{
public:
	CEnumExplorerCommand(IExplorerCommand* const* ppCommands, ULONG cCommands, ULONG index = 0);

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
	IFACEMETHODIMP_(ULONG) AddRef() override;
	IFACEMETHODIMP_(ULONG) Release() override;

	// IEnumExplorerCommand
	IFACEMETHODIMP Next(ULONG celt, IExplorerCommand** ppUICommand, ULONG* pceltFetched) override;
	IFACEMETHODIMP Skip(ULONG celt) override;
	IFACEMETHODIMP Reset() override;
	IFACEMETHODIMP Clone(IEnumExplorerCommand** ppEnum) override;

private:
	~CEnumExplorerCommand();

	long _cRef;
	std::vector<IExplorerCommand*> _commands;
	ULONG _index;
};

// ---------------------------------------------------------------------------
// The single entry this DLL adds to the context menu (its CLSID is what the
// package manifest registers for files, folders and the folder background).
// Following Link Shell Extension, "Pick Link Source" and the drop verbs are
// never visible at the same time:
//
//   no drop possible : it IS "Pick Link Source" (nothing picked, or the target
//                      is a file / multi-selection); forwards to that sub-verb.
//                      Hidden on virtual items such as "This PC".
//   drop possible    : a source is picked and the target is a single real
//                      directory (folder item or folder background): it is
//                      "Drop As..." with ECF_HASSUBCOMMANDS:
//       ExpandedFile   : Hard Link, Symbolic Link, Cancel Link Creation.
//       ExpandedFolder : Symbolic Link, Junction, Cancel Link Creation.
//
// Explorer passes no items for a background click and none at all to GetFlags;
// EffectiveItems() fills in from the items seen earlier or from the site.
//
// (The packaged context-menu schema only allows the item types "*", ".ext",
// "Directory" and "Directory\Background", so the desktop background and drive
// roots cannot be registered. A second registered verb would make Explorer
// wrap everything in a package flyout, which is why Cancel lives inside the
// submenu rather than beside it as in Link Shell Extension.)
// ---------------------------------------------------------------------------

// CLSID registered in Package.appxmanifest.
inline constexpr CLSID CLSID_ModernSymlinkVerb = { 0x7B3C1E52, 0x9A4D, 0x4F6B, { 0x8C, 0x2E, 0x5D, 0x1A, 0x0F, 0x3B, 0x9E, 0x71 } };

class CExplorerCommandVerb final : public IExplorerCommand, public IObjectWithSite
{
public:
	CExplorerCommandVerb();

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
	IFACEMETHODIMP_(ULONG) AddRef() override;
	IFACEMETHODIMP_(ULONG) Release() override;

	// IExplorerCommand
	IFACEMETHODIMP GetTitle(IShellItemArray* pShellItemArray, LPWSTR* ppszName) override;
	IFACEMETHODIMP GetIcon(IShellItemArray* pShellItemArray, LPWSTR* ppszIcon) override;
	IFACEMETHODIMP GetToolTip(IShellItemArray* pShellItemArray, LPWSTR* ppszInfotip) override;
	IFACEMETHODIMP GetCanonicalName(GUID* pGuidCommandName) override;
	IFACEMETHODIMP GetState(IShellItemArray* pShellItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState) override;
	IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override;
	IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override;
	IFACEMETHODIMP Invoke(IShellItemArray* pShellItemArray, IBindCtx* pBindContext) override;

	// IObjectWithSite
	IFACEMETHODIMP SetSite(IUnknown* pUnknownSite) override;
	IFACEMETHODIMP GetSite(REFIID riid, void** ppv) override;

	// The Explorer window hosting the menu (may be null), for UI such as UAC prompts.
	HWND GetWndHandle() const;

	// The State persisted by Pick Link Source, loaded once per right-click.
	// Empty when nothing has been picked yet.
	const std::optional<State>& GetLinkState() const;

	// The items a call is really about: the array Explorer passed, else the
	// one it passed earlier to this instance, else the site's selection or
	// (background click) the folder being viewed. Null only if none of those
	// are available.
	IShellItemArray* EffectiveItems(IShellItemArray* pShellItemArray);

private:
	~CExplorerCommandVerb();

	// Loads (once per right-click) the persisted State and the menu mode it implies.
	MenuState ResolveMode();

	// True when the entry is the "Drop As..." submenu for these items.
	bool ShowsDropMenu(IShellItemArray* pItems);

	// While no drop is possible the entry is "Pick Link Source": that sub-verb.
	// Null while the entry is "Drop As...".
	IExplorerCommand* PickVerbUnlessDropping(IShellItemArray* pItems);

	// Every leaf verb, created up front; see the c_i* indices in the .cpp.
	static constexpr size_t c_cSubCommands = 5;

	long _cRef;
	IUnknown* _pUnknownSite;
	bool _modeResolved;
	MenuState _mode;
	std::optional<State> _linkState;
	IShellItemArray* _pLastItems;
	bool _siteItemsResolved;
	IShellItemArray* _pSiteItems;
	IExplorerCommand* _subCommands[c_cSubCommands];
};
