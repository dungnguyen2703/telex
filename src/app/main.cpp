// Entry point: hidden window, message loop, lifetime of the hooks and the tray
// icon. See docs/DESIGN.md.
#include "app.h"

namespace app {
namespace {

bool g_enabled = true;
HWND g_hwnd = nullptr;
HWINEVENTHOOK g_winEvent = nullptr;
UINT g_taskbarCreated = 0;

void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    // The window changed: the half-typed word is gone, and a different process
    // may or may not be excluded.
    ResetEngine();
    RefreshExclusion();
}

void Shutdown(HWND hwnd) {
    if (g_winEvent) {
        UnhookWinEvent(g_winEvent);
        g_winEvent = nullptr;
    }
    KillTimer(hwnd, kTimerId);
    RemoveHooks();
    DestroyTrayIcon();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case kTrayMessage:
            if (LOWORD(lParam) == WM_LBUTTONUP) {
                ToggleEnabled();
            } else if (LOWORD(lParam) == WM_RBUTTONUP) {
                ShowTrayMenu(hwnd);
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == kMenuExclusions) OpenExclusionFile();
            if (LOWORD(wParam) == kMenuExit) DestroyWindow(hwnd);
            return 0;

        case kRefreshTrayMessage:
            UpdateTrayIcon();
            return 0;

        case WM_TIMER:
            // Picks up edits to exclude.txt without needing a window switch.
            if (wParam == kTimerId) RefreshExclusion();
            return 0;

        case WM_QUERYENDSESSION:
            Shutdown(hwnd);
            return TRUE;

        case WM_DESTROY:
            Shutdown(hwnd);
            PostQuitMessage(0);
            return 0;

        default:
            if (msg == g_taskbarCreated && g_taskbarCreated != 0) {
                // Explorer restarted and forgot about us.
                CreateTrayIcon(hwnd);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

bool IsEnabled() { return g_enabled; }

void SetEnabled(bool on) {
    if (g_enabled == on) return;
    g_enabled = on;
    ResetEngine();
    // This runs inside the keyboard hook. Shell_NotifyIcon talks to Explorer and
    // can block for long enough that Windows drops the next few keystrokes, so
    // the icon update is queued instead of done here.
    if (g_hwnd) PostMessageW(g_hwnd, kRefreshTrayMessage, 0, 0);
}

void ToggleEnabled() { SetEnabled(!g_enabled); }

}  // namespace app

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, app::kMutexName);
    if (mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        // A second instance would hook the keyboard twice and double every key.
        MessageBoxW(nullptr, L"telex is already running.", L"telex",
                    MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = app::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = app::kWindowClass;
    if (!RegisterClassExW(&wc)) return 1;

    // A real (never shown) window rather than a message-only one: message-only
    // windows do not receive the TaskbarCreated broadcast.
    HWND hwnd = CreateWindowExW(0, app::kWindowClass, L"telex", 0, 0, 0, 0, 0,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    app::g_hwnd = hwnd;

    app::g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    if (!app::CreateTrayIcon(hwnd)) return 1;
    if (!app::InstallHooks()) {
        MessageBoxW(hwnd, L"Could not install the keyboard hook.", L"telex",
                    MB_OK | MB_ICONERROR);
        app::DestroyTrayIcon();
        return 1;
    }

    app::g_winEvent = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                      nullptr, app::WinEventProc, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    SetTimer(hwnd, app::kTimerId, app::kTimerPeriodMs, nullptr);
    app::RefreshExclusion();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
