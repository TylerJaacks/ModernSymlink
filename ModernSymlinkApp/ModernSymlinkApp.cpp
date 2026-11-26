// ModernSymlinkApp.cpp : the package's app entry and elevated link helper.
//
// Launched from Start (no arguments), it shows one dialog explaining that
// Modern Symlink works from File Explorer's right-click menu, then exits.
// The Microsoft Store requires a Start entry unless the app has the
// HeadlessAppBypass waiver.
//
// Otherwise it exists so the shell extension can create links that need
// rights Explorer's unprivileged surrogate does not have. The extension
// launches it with the "runas" verb, which shows one UAC prompt:
//
//     ModernSymlinkApp.exe <switch> "<link>" "<source>"
//
// with one of the switches in c_linkKinds (Links.h): --symlink, --symlink-dir,
// --hardlink, --junction. The process creates the link and exits with the
// Win32 error code (0 = ok). It may run as administrator, so it does exactly
// this and nothing else, and only with absolute paths. Any other command line
// exits at once with ERROR_INVALID_PARAMETER and shows nothing.

#include "framework.h"

#include <commctrl.h>
#include <shellapi.h>

#include "Resource.h"
#include "..\ModernSymlinkShell\Links.h"

// TaskDialog lives in version 6 of the common controls
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// The Start entry: says where the app's features are, then exits.
static int ShowWelcome(HINSTANCE hInstance)
{
    // sharp text on high-DPI displays
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    TASKDIALOGCONFIG config = { .cbSize = sizeof(config) };

    config.hInstance = hInstance;
    config.dwFlags = TDF_SIZE_TO_CONTENT;   // one line per step, no awkward wrapping
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = L"Modern Symlink";
    config.pszMainIcon = MAKEINTRESOURCEW(IDI_MODERNSYMLINKAPP);
    config.pszMainInstruction = L"Modern Symlink is installed";
    config.pszContent =
        L"It works from File Explorer's right-click menu:\n\n"
        L"1.  Right-click a file or folder and choose Pick Link Source.\n"
        L"2.  Right-click the destination folder, or empty space in it, and choose Drop As.";

    TaskDialogIndirect(&config, nullptr, nullptr, nullptr);

    return ERROR_SUCCESS;
}

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
static int Run(HINSTANCE hInstance)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (!argv)
    {
        return static_cast<int>(GetLastError());
    }

    int exitCode = ERROR_INVALID_PARAMETER;

    if (argc == 1)
    {
        // opened from Start
        exitCode = ShowWelcome(hInstance);
    }
    else if (argc == 4)
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
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // no main window, no message loop: one dialog or one link, then exit
    return Run(hInstance);
}
