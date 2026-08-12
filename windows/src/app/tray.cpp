// The tray icon and its two-item menu. The artwork itself lives in icon.cpp.
#include "app.h"

#include <shellapi.h>

#include "icon.h"

namespace app {
namespace {

NOTIFYICONDATAW g_data = {};
HWND g_owner = nullptr;
HICON g_iconOn = nullptr;
HICON g_iconOff = nullptr;

void FillTooltip() {
    const wchar_t* text = IsEnabled() ? L"telex: ON  (Alt+Z to turn off)"
                                      : L"telex: OFF  (Alt+Z to turn on)";
    wcscpy_s(g_data.szTip, text);
}

}  // namespace

bool CreateTrayIcon(HWND owner) {
    g_owner = owner;
    if (!g_iconOn) g_iconOn = MakeTrayIcon(true);
    if (!g_iconOff) g_iconOff = MakeTrayIcon(false);

    g_data = {};
    g_data.cbSize = sizeof g_data;
    g_data.hWnd = owner;
    g_data.uID = 1;
    g_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_data.uCallbackMessage = kTrayMessage;
    g_data.hIcon = IsEnabled() ? g_iconOn : g_iconOff;
    FillTooltip();
    return Shell_NotifyIconW(NIM_ADD, &g_data) != FALSE;
}

void UpdateTrayIcon() {
    if (!g_owner) return;
    g_data.hIcon = IsEnabled() ? g_iconOn : g_iconOff;
    FillTooltip();
    Shell_NotifyIconW(NIM_MODIFY, &g_data);
}

void DestroyTrayIcon() {
    if (!g_owner) return;
    Shell_NotifyIconW(NIM_DELETE, &g_data);
    if (g_iconOn) DestroyIcon(g_iconOn);
    if (g_iconOff) DestroyIcon(g_iconOff);
    g_iconOn = g_iconOff = nullptr;
    g_owner = nullptr;
}

void ShowTrayMenu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kMenuExclusions, L"Open exclusion list");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    // Required so the menu closes when the user clicks elsewhere.
    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, owner,
                   nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

}  // namespace app
