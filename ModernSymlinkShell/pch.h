// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and most code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#define STRICT_TYPED_ITEMIDS
#define NOMINMAX

// Windows Header Files
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>

#include <new>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ShellHelpers.h"

#endif //PCH_H
