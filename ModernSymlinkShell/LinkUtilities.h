#pragma once
#include "pch.h"
#include "Links.h"

// Creates a link of the given kind. If that fails with the one error running
// elevated can fix (see c_linkKinds), retries through the package's elevated
// helper, which shows one UAC prompt. Returns S_OK, the failure as an HRESULT,
// or ERROR_CANCELLED if the user declined the prompt.
HRESULT CreateLinkWithElevation(HWND hwndOwner, LinkKind kind, const std::wstring& linkPath, const std::wstring& source);
