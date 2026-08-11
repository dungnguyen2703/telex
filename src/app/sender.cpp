// Emits the edits the engine asked for. One SendInput call for the whole batch:
// Chrome and Electron reorder events that arrive in separate calls
// (docs/DESIGN.md, pitfall 3).
#include "app.h"

#include <vector>

namespace app {
namespace {

void AddKey(std::vector<INPUT>& out, WORD vk, bool keyUp) {
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    in.ki.dwExtraInfo = kSignature;
    out.push_back(in);
}

void AddUnicode(std::vector<INPUT>& out, char16_t ch, bool keyUp) {
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = static_cast<WORD>(ch);
    in.ki.dwFlags = KEYEVENTF_UNICODE | (keyUp ? KEYEVENTF_KEYUP : 0);
    in.ki.dwExtraInfo = kSignature;
    out.push_back(in);
}

}  // namespace

// Swallowing the Z of Alt+Z leaves the target application seeing Alt pressed and
// released on its own, which puts it into menu mode - a modal loop that stops it
// processing anything else. A stray Ctrl tap tells Windows the Alt chord was
// consumed by another key.
void CancelAltMenu() {
    std::vector<INPUT> events;
    AddKey(events, VK_CONTROL, false);
    AddKey(events, VK_CONTROL, true);
    SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
}

void SendEdit(int backspaces, const std::u16string& insert) {
    if (backspaces <= 0 && insert.empty()) return;

    std::vector<INPUT> events;
    events.reserve(static_cast<size_t>(backspaces) * 2 + insert.size() * 2);
    for (int i = 0; i < backspaces; ++i) {
        AddKey(events, VK_BACK, false);
        AddKey(events, VK_BACK, true);
    }
    for (char16_t ch : insert) {
        AddUnicode(events, ch, false);
        AddUnicode(events, ch, true);
    }
    SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
}

}  // namespace app
