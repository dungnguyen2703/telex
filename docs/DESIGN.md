# DESIGN — Technical architecture

This document is for whoever writes the code. The README describes *what* we
build; this describes *how*.

All documentation, code, comments, and commit messages are in English. The only
Vietnamese in the repo is the README and the Vietnamese text data itself.

## Foundational decisions

| Item | Choice | Why |
| --- | --- | --- |
| Language | C++17, plain Win32 | No runtime, single `.exe`, no GC/JIT pause inside the keyboard hook |
| Toolchain | MSVC (`cl.exe`) from the VS 18 Community install already on the machine | Nothing new to install |
| Build | `build.bat` invoking `vcvars64.bat` then `cl` | No CMake (not installed) |
| Key interception | `WH_KEYBOARD_LL` (low-level hook) + `SendInput` | Same as UniKey's default mode; far simpler than TSF |
| Output | `SendInput` with `KEYEVENTF_UNICODE` | Independent of keyboard layout |
| Persisted state | None. Always starts ON | No config file |

We do not use TSF (Text Services Framework). TSF is the "correct" approach but
requires a COM server, per-architecture DLL registration, and running inside
other processes — far too heavy for this scope.

## Layering

The single most important boundary in the project: **the engine must never
`#include <windows.h>`**. That is what makes the entire input logic testable
without a real keyboard.

```
src/
  engine/            <- pure C++, no Win32, no I/O. Testable offline.
    telex.h/.cpp        state machine: key in -> text edit commands out
    syllable.h/.cpp     syllable parsing, validity, tone placement
    tables.h            onsets / nuclei / codas / tone mark tables
  app/               <- Windows layer, as thin as possible, almost no logic
    main.cpp            WinMain, message loop, startup/teardown
    hook.cpp            WH_KEYBOARD_LL, key filtering, key swallowing
    sender.cpp          emits backspaces + Unicode chars via SendInput
    tray.cpp            Shell_NotifyIcon, context menu
    icon.cpp            draws the ON/OFF icons with GDI, no binary assets
    exclusion.cpp       reads exclude.txt, resolves foreground process name
tests/
  engine_tests.cpp   <- runs against the engine, no Windows needed
  e2e_test.cpp       <- real test: inject keys into an EDIT control, read back
docs/
```

## Engine interface

The engine is effectively a pure function: **state + key in → edit commands
out**. It never sends anything itself and knows nothing about `SendInput`.

```cpp
enum class Action { PassThrough, Swallow, Replace };

struct Result {
    Action  action;
    int     backspaces;      // backspaces to send first
    std::u16string insert;   // text to type afterwards
};

class Engine {
public:
    Result OnKey(char32_t ch, bool shift, bool caps);
    Result OnBackspace();
    void   Reset();          // on focus loss, mouse click, navigation keys, ...
private:
    std::u16string buffer_;  // exactly what is currently displayed for this word
    std::u16string raw_;     // exactly the keys the user pressed for this word
};
```

Keeping `buffer_` (what's on screen) and `raw_` (the original keystrokes) side
by side is what enables two behaviours: retyping a tone key to undo it, and
reverting to literal text when the syllable turns out invalid. The detailed
rules live in [TELEX.md](TELEX.md).

`backspaces` is always the number of trailing characters that **actually
changed**, never the whole word. Typing `hoas` needs 2 backspaces (`oa` → `òa`),
not 4.

## Flow of a single keystroke

```
user presses a key
        |
   LowLevelKeyboardProc  (hook.cpp)
        |
   1. Is this input we injected ourselves?  (dwExtraInfo == kSignature)
        |-- yes --> CallNextHookEx immediately, do NOT process  (breaks recursion)
   2. Is it Alt+Z?              --> toggle ON/OFF, swallow the key, done
   3. Currently OFF?            --> return, don't touch it
   4. Foreground app excluded?  --> return, don't touch it
   5. Ctrl / Alt / Win held?    --> Engine.Reset(), return
   6. Navigation / Enter / Tab / punctuation? --> Engine.Reset(), return
        |
   7. Engine.OnKey(...)
        |
   PassThrough --> CallNextHookEx (the key types itself as usual)
   Swallow     --> return 1 (eat it, emit nothing)
   Replace     --> eat the original key, then sender.cpp emits N backspaces + new text
```

## Win32 pitfalls that must be handled (missing one breaks the app)

1. **Hook recursion.** Our own `SendInput` comes back through the hook. Stamp
   every `INPUT` we send with `dwExtraInfo = kSignature` (`0x54454C58`) and skip
   those at the top of the hook. Skip *only* by this signature — never by the
   `LLKHF_INJECTED` flag, because the automated tests also inject keys and those
   must be processed like real ones.
2. **Low-level hook timeout.** Windows silently unhooks us if the callback
   exceeds `LowLevelHooksTimeout`. No heavy allocation, no I/O, no locks inside
   the hook. Reading `exclude.txt` and resolving process names happens outside
   the hook (driven by a WinEvent hook on foreground change and a one-second
   timer); the hook only reads a precomputed flag.
   The same applies to anything the hook triggers: `Shell_NotifyIcon` talks to
   Explorer and blocks long enough to lose the next few keystrokes, so toggling
   only flips a flag and posts `kRefreshTrayMessage` for the message loop to
   handle. This was a real bug caught by the tier 2 tests, not a theoretical one.
3. **Backspace/character ordering.** Send everything in **one** `SendInput` call
   with a single array, never several calls. Chrome and Electron apps mis-order
   the events otherwise.
4. **Alt+Z is swallowed globally.** Detect it in the hook and return 1 so the app
   below never sees it. Three easily-missed details: (a) Alt+key arrives as
   `WM_SYSKEYDOWN`, not `WM_KEYDOWN` — handle both; (b) swallow the matching
   `WM_SYSKEYUP` for `Z` too, so no app sees half a chord; (c) with the Z
   swallowed, the focused window sees Alt pressed and released on its own and
   enters **menu mode** — a modal loop that freezes it until Escape. Injecting a
   dummy `VK_CONTROL` tap right after the toggle tells Windows the Alt chord was
   consumed. Not optional: without it the e2e test hangs, and so does Notepad.
5. **Elevated windows.** A non-elevated process cannot hook input to elevated
   windows. That is a Windows limitation — document it in the README's known
   limitations, do not try to work around it.
6. **Focus changes.** Register `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`: on
   every window switch, `Engine.Reset()` and recompute the exclusion flag. Mouse
   clicks must also `Reset()` (a lightweight mouse hook listening only for
   `WM_LBUTTONDOWN`).
7. **Caps Lock and Shift.** Derive the actual character from
   `GetKeyState(VK_CAPITAL)` plus shift state — never guess from the key code.
8. **Case preservation.** `Đ`, `Ư`, `Ơ` and friends must follow the case of the
   original keystroke.
9. **Single instance.** Named `CreateMutex`; exit immediately if already running,
   otherwise two hooks produce doubled characters.
10. **Teardown.** `UnhookWindowsHookEx` + `Shell_NotifyIcon(NIM_DELETE)` on exit,
    including on `WM_QUERYENDSESSION`, so no ghost icon is left in the tray.

## Exclusion list

- `exclude.txt` sits next to the `.exe`. One program name per line. Blank lines
  and lines starting with `#` are ignored. Case-insensitive comparison against
  the file name only (`code.exe`), never the full path.
- A missing file means an empty list — not an error.
- Reloaded when its modification time changes, checked on every window switch
  and on a one-second timer. The timer is what makes "save the file and it
  applies" true even when the foreground never changes. No directory watcher, no
  restart required.
- Whether the current window is excluded is computed **once** per foreground
  change and stored in an `std::atomic<bool>`; the hook only reads that flag.
- Exclusion does not change the ON/OFF state. Leaving the app resumes typing
  immediately.

## System tray

- Two icons: ON (white **V** on a red rounded square) and OFF (the same shape in
  grey). Both are drawn with GDI at startup, so the build needs no binary
  assets. The tooltip states the current mode.
- Left click toggles ON/OFF. Right click opens a two-item menu: *Open exclusion
  list* (`ShellExecute` on `exclude.txt`, creating a commented template if
  missing) and *Exit*.
- The icon updates the moment the state changes, including via the hotkey.

## Out of scope

To prevent creep: no run-at-startup, no auto-update, no log files, no settings
window, no UI localisation, no configurable input method. Adding anything on
this list is a scope violation.
