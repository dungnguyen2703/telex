# DESIGN — The Windows build

The rules that apply to both builds are in [docs/DESIGN.md](../../docs/DESIGN.md)
and the input rules in [docs/TELEX.md](../../docs/TELEX.md). This file is only
the Win32 half.

**This build is done and works. Treat it as frozen.** Everything below,
especially the pitfall list, came out of testing against real applications with a
real keyboard. Changing this code to accommodate the macOS port, or to share
anything with it, is not allowed — the macOS build is a separate implementation
for exactly that reason.

## Choices

| Item | Choice | Why |
| --- | --- | --- |
| Language | C++17, plain Win32 | No runtime, single `.exe`, no GC/JIT pause inside the keyboard hook |
| Toolchain | MSVC (`cl.exe`) from the VS 18 Community install already on the machine | Nothing new to install |
| Build | `build.bat` invoking `vcvars64.bat` then `cl` | No CMake (not installed) |
| Key interception | `WH_KEYBOARD_LL` (low-level hook) + `SendInput` | Same as UniKey's default mode; far simpler than TSF |
| Output | `SendInput` with `KEYEVENTF_UNICODE` | Independent of keyboard layout |
| Toggle | `Alt + Z` | Swallowed globally |
| Exclusion identity | Executable file name (`code.exe`) | Never the full path |
| Exclusion file | `exclude.txt`, next to the `.exe` | |

We do not use TSF (Text Services Framework). TSF is the "correct" approach but
requires a COM server, per-architecture DLL registration, and running inside
other processes — far too heavy for this scope.

## Layout

Paths are relative to `windows/`.

```
src/
  engine/            <- pure C++, no Win32, no I/O. Testable offline.
    telex.h/.cpp        state machine: key in -> text edit commands out
    syllable.h/.cpp     syllable parsing, validity, tone placement
    tables.h/.cpp       onsets / nuclei / codas / tone mark tables
  app/               <- Windows layer, as thin as possible, almost no logic
    app.h               shared declarations for the Windows layer
    main.cpp            WinMain, message loop, startup/teardown
    hook.cpp            WH_KEYBOARD_LL, key filtering, key swallowing
    sender.cpp          emits backspaces + Unicode chars via SendInput
    tray.cpp            Shell_NotifyIcon, context menu
    icon.cpp/.h         draws the ON/OFF icons with GDI, no binary assets
    exclusion.cpp       reads exclude.txt, resolves foreground process name
tests/
  engine_tests.cpp   <- tier 1, runs against the engine, no Windows needed
  e2e_test.cpp       <- tier 2, injects keys into an EDIT control, reads back
build.bat
telex.rc             <- one line: the application icon resource
telex.ico            <- generated, not hand-drawn; see below
build/telex.exe      <- build output, and the committed download
```

The syllable corpus is not here: it lives in [docs/corpus.txt](../../docs/corpus.txt)
because both builds check themselves against the same one.

`build/` is the output directory and is ignored, with exactly one exception:
`build/telex.exe` is committed, because that is the file the README links to for
download. Two consequences to be aware of:

- **Every local build overwrites it**, so `git status` will show it modified
  after you build. That is expected. Commit it when you actually mean to ship a
  new download; otherwise `git checkout -- build/telex.exe` to put the shipped
  one back.
- The other build artefacts next to it (`engine_tests.exe`, `e2e_test.exe`,
  `*.obj`, `*.pdb`) stay ignored.

The engine signature is `Result OnKey(char16_t ch)` — the case is already
resolved by `hook.cpp`. `Action` has two values, `PassThrough` and `Replace`.

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
   It applies to the whole thread, not just the callback: anything slow running
   in the message loop delays hook delivery just as badly. Watching `exclude.txt`
   used to sit on a one-second timer here, and because the file can live in a
   cloud-synced folder, a single slow `GetFileAttributesEx` was enough to lose a
   keystroke — which the user sees as a word that randomly failed to take its
   tone mark. It now runs on its own thread.
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

Shared semantics are in [docs/DESIGN.md](../../docs/DESIGN.md). Windows
specifics:

- `exclude.txt` sits next to the `.exe`. One program name per line, compared
  case-insensitively against the file name only (`code.exe`), never the path.
- The name is cut after `.exe` rather than at the first space, because plenty of
  programs ship as `League of Legends.exe`. Everything after that is a
  description and is ignored.
- A dedicated worker thread owns the list. It polls once a second, and the
  foreground WinEvent signals it to look again immediately. Polling is what makes
  "save the file and it applies" true even when the foreground never changes. No
  directory watcher, no restart required.
- The worker publishes a single `std::atomic<bool>`; the hook only reads that
  flag. Nothing that touches the disk or another process may run on the thread
  that owns the hook.
- Opening the list from the menu creates a commented template first if the file
  is missing, then `ShellExecute`s it.

## System tray

- Two icons drawn with GDI at startup, so the build needs no binary assets. The
  tooltip states the current mode.
- The **application** icon that Explorer shows for `telex.exe` cannot be drawn at
  runtime — the shell reads it from the file. It lives in `telex.ico`, linked in
  through `telex.rc` by an `rc.exe` step in `build.bat`. The file is generated by
  `macos/Tools/makeicon.swift` from the same artwork, so both platforms show the
  same icon; it is committed because the Windows machine has no Swift toolchain.
  Do not hand-edit it.
- Left click toggles ON/OFF. Right click opens a two-item menu: *Open exclusion
  list* and *Exit*.
- Re-register the icon on the `TaskbarCreated` broadcast — Explorer restarts and
  forgets about us. This is why the app owns a real (never shown) window rather
  than a message-only one: message-only windows do not receive that broadcast.
