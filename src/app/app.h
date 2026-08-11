// Shared declarations for the Windows layer. All the input logic lives in
// src/engine; everything here is plumbing.
#pragma once

#include <windows.h>

#include <string>

namespace app {

// Stamped on every input event we inject so the hook can ignore its own output.
// Deliberately not LLKHF_INJECTED: the e2e tests inject keys too and those must
// be processed like real ones (docs/DESIGN.md, pitfall 1).
constexpr ULONG_PTR kSignature = 0x54454C58;  // 'TELX'

constexpr UINT kTrayMessage = WM_APP + 1;
// Posted by the hook so the tray icon is refreshed from the message loop and
// never from inside the hook callback (docs/DESIGN.md, pitfall 2).
constexpr UINT kRefreshTrayMessage = WM_APP + 2;
constexpr UINT kTimerId = 1;
constexpr UINT kTimerPeriodMs = 1000;

constexpr wchar_t kMutexName[] = L"Local\\telex-single-instance";
constexpr wchar_t kWindowClass[] = L"TelexHiddenWindow";

constexpr UINT kMenuExclusions = 100;
constexpr UINT kMenuExit = 101;

// --- state (main.cpp) -------------------------------------------------------
bool IsEnabled();
void SetEnabled(bool on);
void ToggleEnabled();

// --- hook.cpp --------------------------------------------------------------
bool InstallHooks();
void RemoveHooks();
void ResetEngine();

// --- sender.cpp ------------------------------------------------------------
void SendEdit(int backspaces, const std::u16string& insert);
// Injects a dummy Ctrl press so that releasing Alt does not open the menu bar
// of whatever window is focused (docs/DESIGN.md, pitfall 4c).
void CancelAltMenu();

// --- tray.cpp --------------------------------------------------------------
bool CreateTrayIcon(HWND owner);
void DestroyTrayIcon();
void UpdateTrayIcon();
void ShowTrayMenu(HWND owner);

// --- exclusion.cpp ---------------------------------------------------------
// Recomputes "is the foreground window excluded", reloading exclude.txt if it
// changed. Never called from inside the keyboard hook.
void RefreshExclusion();
bool IsExcluded();
void OpenExclusionFile();
std::wstring ExclusionPath();

}  // namespace app
