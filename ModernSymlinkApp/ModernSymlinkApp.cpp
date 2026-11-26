// ModernSymlinkApp.cpp : the package's elevated link helper.
//
// This executable has no user interface and is not meant to be run by hand.
// It is hidden from Start (AppListEntry="none" in the package manifest) and
// exists only so the shell extension can create links that need rights
// Explorer's unprivileged surrogate does not have. The extension launches it
// with the "runas" verb, which shows one UAC prompt:
//
//     ModernSymlinkApp.exe <switch> "<link>" "<source>"
//
// with one of the switches in c_linkKinds (Links.h): --symlink, --symlink-dir,
// --hardlink, --junction. The process creates the link and exits with the
// Win32 error code (0 = ok). It runs as administrator, so it does exactly this
// and nothing else, and only with absolute paths. Any other command line,
// including none at all, exits at once with ERROR_INVALID_PARAMETER and shows
// nothing.

#include "framework.h"

#include <shellapi.h>

#include "..\ModernSymlinkShell\Links.h"

static bool IsAbsolutePath(PCWSTR path)
{
    // "C:\..." or "\\server\share\..."
    const size_t length = wcslen(path);

    const bool drive = length >= 3 && iswalpha(path[0]) && path[1] == L':' && path[2] == L'\\';
    const bool unc = length >= 3 && path[0] == L'\\' && path[1] == L'\\';

    if (!drive && !unc)
    {
        return false;
    }

    // no "." or ".." segments anywhere
    for (PCWSTR segment = path; *segment; )
    {
        PCWSTR end = wcschr(segment, L'\\');
        const size_t segmentLength = end ? static_cast<size_t>(end - segment) : wcslen(segment);

        if ((segmentLength == 1 && segment[0] == L'.') ||
            (segmentLength == 2 && segment[0] == L'.' && segment[1] == L'.'))
        {
            return false;
        }

        if (!end)
        {
            break;
        }

        segment = end + 1;
    }

    return true;
}

// Carries out the command line and returns the process exit code.
static int RunHelper()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (!argv)
    {
        return static_cast<int>(GetLastError());
    }

    int exitCode = ERROR_INVALID_PARAMETER;

    if (argc == 4)
    {
        if (const LinkKindInfo* info = FindLinkKindBySwitch(argv[1]))
        {
            PCWSTR link = argv[2];
            PCWSTR source = argv[3];

            if (!IsAbsolutePath(link) || !IsAbsolutePath(source))
            {
                exitCode = ERROR_BAD_PATHNAME;
            }
            else
            {
                exitCode = CreateLink(info->kind, link, source) ? ERROR_SUCCESS : static_cast<int>(GetLastError());
            }
        }
    }

    LocalFree(argv);

    return exitCode;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // no window, no message loop: do the one job (or nothing) and exit
    return RunHelper();
}
