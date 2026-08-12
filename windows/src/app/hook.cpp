// Low-level keyboard and mouse hooks. Everything here runs on the message-loop
// thread and must stay fast: Windows silently unhooks callbacks that take too
// long (docs/DESIGN.md, pitfall 2).
#include "app.h"

#include "../engine/telex.h"

namespace app {
namespace {

telex::Engine g_engine;
HHOOK g_keyboardHook = nullptr;
HHOOK g_mouseHook = nullptr;
bool g_swallowNextZUp = false;

bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

// Word boundaries that are not characters: anything that moves the caret or
// edits elsewhere invalidates what we think is on screen.
bool IsBoundaryKey(DWORD vk) {
    switch (vk) {
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
        case VK_DELETE: case VK_INSERT: case VK_RETURN: case VK_TAB:
        case VK_ESCAPE:
            return true;
        default:
            return false;
    }
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION) return CallNextHookEx(nullptr, code, wParam, lParam);

    const KBDLLHOOKSTRUCT* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (key->dwExtraInfo == kSignature) {
        return CallNextHookEx(nullptr, code, wParam, lParam);  // our own output
    }

    const bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    const bool ctrl = KeyDown(VK_CONTROL);
    const bool alt = (key->flags & LLKHF_ALTDOWN) != 0 || KeyDown(VK_MENU);
    const bool win = KeyDown(VK_LWIN) || KeyDown(VK_RWIN);

    // Alt+Z toggles. Both halves of the chord are swallowed so no application
    // ever sees a stray Z.
    if (key->vkCode == 'Z' && alt && !ctrl && !win) {
        if (down) {
            ToggleEnabled();
            CancelAltMenu();
            g_swallowNextZUp = true;
            return 1;
        }
        if (up && g_swallowNextZUp) {
            g_swallowNextZUp = false;
            return 1;
        }
    }
    if (key->vkCode == 'Z' && up && g_swallowNextZUp) {
        g_swallowNextZUp = false;  // Alt was released first
        return 1;
    }

    if (!down) return CallNextHookEx(nullptr, code, wParam, lParam);

    // Keys typed while we were standing aside never reached the engine, so what
    // it remembers about the text is no longer true.
    static bool wasExcluded = false;
    const bool excluded = IsExcluded();
    if (excluded != wasExcluded) {
        wasExcluded = excluded;
        g_engine.Reset();
    }
    if (!IsEnabled() || excluded) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (ctrl || alt || win) {
        g_engine.Reset();
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (key->vkCode == VK_BACK) {
        g_engine.OnBackspace();
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (key->vkCode == VK_SPACE) {
        g_engine.EndWord(u' ');
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (IsBoundaryKey(key->vkCode)) {
        // The caret is going somewhere we cannot follow; forget the text too.
        g_engine.Reset();
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (key->vkCode < 'A' || key->vkCode > 'Z') {
        // Digits, punctuation, function keys: they end the word, but the caret
        // stays put, so backspacing back into what we typed still works. Which
        // character it was does not matter - anything that is not a letter ends
        // a word just the same.
        g_engine.EndWord(0);
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const bool shift = KeyDown(VK_SHIFT);
    const bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    const bool upper = (shift != caps);
    const char16_t ch = static_cast<char16_t>(
        upper ? key->vkCode : (key->vkCode - 'A' + 'a'));

    const telex::Result r = g_engine.OnKey(ch);
    if (r.action == telex::Action::PassThrough) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    SendEdit(r.backspaces, r.insert);
    return 1;
}

LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION &&
        (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
         wParam == WM_MBUTTONDOWN)) {
        // The caret moved somewhere we cannot see; forget the current word.
        g_engine.Reset();
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

bool InstallHooks() {
    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, nullptr, 0);
    if (!g_keyboardHook) return false;
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, nullptr, 0);
    return true;  // the mouse hook is a nicety, not a requirement
}

void RemoveHooks() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
}

void ResetEngine() { g_engine.Reset(); }

}  // namespace app
