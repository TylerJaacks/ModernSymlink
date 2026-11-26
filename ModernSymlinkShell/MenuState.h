#pragma once
#include "pch.h"
#include "State.h"

// Shape of the "Drop As..." submenu, decided by what was picked.
enum class MenuState
{
	NothingPicked,   // no drop possible
	ExpandedFile,    // a file is picked
	ExpandedFolder   // a folder is picked
};

inline MenuState MenuStateFor(const std::optional<State>& state)
{
	if (!state.has_value())
	{
		return MenuState::NothingPicked;
	}

	return state->m_isFolder ? MenuState::ExpandedFolder : MenuState::ExpandedFile;
}

inline bool IsExpanded(MenuState state)
{
	return state != MenuState::NothingPicked;
}

// ---------------------------------------------------------------------------
// Questions about the right-clicked items. All accept a null array.
// ---------------------------------------------------------------------------

// Filesystem path of the first item; empty if there is none or it is virtual
// (e.g. "This PC").
std::wstring GetFirstItemPath(IShellItemArray* pShellItemArray);

// The first item is a real directory: not a zip file, not a virtual folder.
bool IsFolderItem(IShellItemArray* pShellItemArray);

// The first item exists on a filesystem, i.e. a link could point at it.
bool IsFilesystemItem(IShellItemArray* pShellItemArray);

// Exactly one item, and it is a real directory: something a link can be dropped into.
bool IsSingleDirectoryTarget(IShellItemArray* pShellItemArray);
