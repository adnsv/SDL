/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
/*
  Screen keyboard and text input backend
  for GDK platforms.
*/

#include "SDL_windowstextinput.h"
#include "SDL_build_config.h"
#include "SDL_internal.h"

#ifdef SDL_WINDOWS_TEXTINPUT

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <ShlObj.h>
#include <Windows.h>
#include <initguid.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlwapi.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../../events/SDL_keyboard_c.h"
#include "../windows/SDL_windowsvideo.h"

DEFINE_GUID(CLSID_UIHostNoLaunch, 0x4CE576FA, 0x83DC, 0x4f88, 0x95, 0x1C, 0x9D, 0x07, 0x82, 0xB4, 0xE3, 0x76);
DEFINE_GUID(IID_ITipInvocation, 0x37c994e7, 0x432b, 0x4834, 0xa2, 0xf7, 0xdc, 0xe1, 0xf1, 0x3b, 0x83, 0x4b);

typedef interface ITipInvocation ITipInvocation;
typedef struct ITipInvocationVtbl
{
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE *QueryInterface)
    (ITipInvocation *This, REFIID riid, void **ppvObject);
    ULONG(STDMETHODCALLTYPE *AddRef)
    (ITipInvocation *This);
    ULONG(STDMETHODCALLTYPE *Release)
    (ITipInvocation *This);
    HRESULT(STDMETHODCALLTYPE *Toggle)
    (ITipInvocation *This, HWND window);
    END_INTERFACE
} ITipInvocationVtbl;
interface ITipInvocation { CONST_VTBL ITipInvocationVtbl *lpVtbl; };

static bool LaunchTabTip()
{
    DWORD pid = 0;
    STARTUPINFOW su = { sizeof(STARTUPINFOW) };
    su.dwFlags = STARTF_USESHOWWINDOW;
    su.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION stat = {};
    const wchar_t *path =
        L"%CommonProgramW6432%\\microsoft shared\\ink\\TabTIP.EXE";
    wchar_t buf[512];
    SetEnvironmentVariableW(L"__compat_layer", L"RunAsInvoker");
    ExpandEnvironmentStringsW(path, buf, 512);

    if (CreateProcessW(0, buf, 0, 0, 1, 0, 0, 0, &su, &stat)) {
        pid = stat.dwProcessId;
    }
    CloseHandle(stat.hProcess);
    CloseHandle(stat.hThread);
    return pid != 0;
}

static bool LaunchTabTip2()
{
    WCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES | CSIDL_FLAG_CREATE, NULL, 0, szPath))) {
        if (wcscat_s(szPath, MAX_PATH, L"\\Common Files\\microsoft shared\\ink\\TabTip.exe") != 0)
            return false;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        // Create the process with the CREATE_SUSPENDED flag
        if (CreateProcessW(szPath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
            // Resume the process
            ResumeThread(pi.hThread);

            // Wait for the process to become idle, with a timeout
            DWORD result = WaitForInputIdle(pi.hProcess, 4000); // 4 second timeout

            CloseHandle(pi.hThread);

            if (result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT) {
                // Process is idle or timeout occurred, assume it's ready
                CloseHandle(pi.hProcess);
                return TRUE;
            } else {
                // WaitForInputIdle failed
                TerminateProcess(pi.hProcess, 0);
                CloseHandle(pi.hProcess);
                return FALSE;
            }
        }
    }
    return FALSE;
}

static bool GetKeyboardRect(RECT *r)
{
    IFrameworkInputPane *inputPane = NULL;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(&CLSID_FrameworkInputPane, NULL, CLSCTX_INPROC_SERVER, &IID_IFrameworkInputPane, (LPVOID *)&inputPane);
        if (SUCCEEDED(hr)) {
            hr = inputPane->lpVtbl->Location(inputPane, r);
            if (!SUCCEEDED(hr)) {
            }
            inputPane->lpVtbl->Release(inputPane);
        }
    }
    CoUninitialize();
    return SUCCEEDED(hr);
}

static bool IsKeyboardVisible() {
    RECT r;
    bool rect_ok = GetKeyboardRect(&r);
    return rect_ok && (r.right > r.left) && (r.bottom > r.top);
}

static bool IsKeyboardVisible_1()
{
    HWND hwnd = FindWindowW(L"IPTip_Main_Window", NULL);
    if (hwnd) {
        LONG style = GetWindowLongW(hwnd, GWL_STYLE);
        bool visible = (style & WS_VISIBLE) != 0;
        bool disabled = (style & WS_DISABLED) != 0;
        if (!visible)
            return false;
        return !disabled;
    }
    return false;
}

static bool IsKeyboardVisible_2()
{
    return false;

    LPCWSTR afw_cls = L"ApplicationFrameWindow";
    LPCWSTR core_cls = L"Windows.UI.Core.CoreWindow";
    LPCWSTR core_title1 = L"Microsoft Text Input Application";
    LPCWSTR core_title2 = L"Windows Input Experience";

    HWND wnd = NULL;

    HWND afw = FindWindowExW(NULL, NULL, afw_cls, NULL);
    if (!afw)
        return false;

    do {
        wnd = FindWindowExW(afw, NULL, core_cls, core_title1);
        if (wnd)
            break;

        wnd = FindWindowExW(afw, NULL, core_cls, core_title2);
        if (wnd)
            break;

    } while (afw = FindWindowExW(NULL, afw, afw_cls, NULL));

    if (!afw || !wnd)
        return false;

    LONG style = GetWindowLongW(afw, GWL_STYLE);
    bool visible1 = (style & WS_VISIBLE) != 0;
    bool disabled1 = (style & WS_DISABLED) != 0;

    style = GetWindowLongW(wnd, GWL_STYLE);
    bool visible2 = (style & WS_VISIBLE) != 0;
    bool disabled2 = (style & WS_DISABLED) != 0;
    if (!visible2)
        return false;
    return !disabled2;
}

static void ToggleKeyboardVisibility()
{
    ITipInvocation *pTipInvocation = NULL;
    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        printf("Failed to initialize COM\n");
        return;
    }

    // Create an instance of the ITipInvocation interface
    hr = CoCreateInstance(&CLSID_UIHostNoLaunch, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                          &IID_ITipInvocation, (void **)&pTipInvocation);

    if (hr == REGDB_E_CLASSNOTREG) {
        if (LaunchTabTip()) {
            hr = CoCreateInstance(&CLSID_UIHostNoLaunch, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                                  &IID_ITipInvocation, (void **)&pTipInvocation);
        } else {
            printf("Failed to launch TabTip service\n");
        }
    }

    if (SUCCEEDED(hr)) {
        // Show the on-screen keyboard
        hr = pTipInvocation->lpVtbl->Toggle(pTipInvocation, GetDesktopWindow());
        if (FAILED(hr)) {
            printf("Failed to show on-screen keyboard\n");
        }
        // Release the interface
        pTipInvocation->lpVtbl->Release(pTipInvocation);
    } else {
        printf("Failed to create ITipInvocation instance\n");
    }

    // Uninitialize COM
    CoUninitialize();
}

bool WIN_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return true;
}

void WIN_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    if (!IsKeyboardVisible())
        ToggleKeyboardVisibility();
}

void WIN_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (IsKeyboardVisible())
        ToggleKeyboardVisibility();
}

bool WIN_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    return IsKeyboardVisible();
}

#ifdef __cplusplus
}
#endif

#endif
