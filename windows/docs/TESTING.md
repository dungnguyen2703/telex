# TESTING — The Windows build

Tier 1 (the engine, the corpus, the round-trip test) is shared and specified in
[docs/TESTING.md](../../docs/TESTING.md). This file covers only what is specific
to Windows: how tier 2 is wired, and the tier 3 checklist.

## Running the tests

From the `windows/` directory:

```
build.bat        rem  build build\telex.exe
build.bat engine rem  tier 1 only
build.bat test   rem  tier 1 then tier 2
```

`build.bat test` prints pass/fail counts and exits non-zero if anything failed.
No case may be marked "skip for now". Tier 1 is **662 assertions**; that number
is the reference the macOS build is measured against.

## Tier 1 wiring

`tests/engine_tests.cpp` links the engine only — no Win32 — and takes the path to
the corpus as its argument. `build.bat` passes `..\docs\corpus.txt`, the file
shared with the macOS build; the hard-coded fallback inside the test is only used
when it is run by hand with no argument.

It compiles and runs on any platform with a C++17 compiler; that portability is
deliberate and worth keeping, because it is what lets the engine be checked
without a Windows machine.

## Tier 2 — Real end-to-end tests (`tests/e2e_test.cpp`)

Covers what unit tests cannot reach: whether the hook actually intercepts keys,
whether it recurses, whether the right number of backspaces goes out.

Mechanism: the test program creates its own window with an `EDIT` control,
brings it to the foreground, injects keys with `SendInput`, then reads the
control back with `WM_GETTEXT` and compares.

This is why the hook must **not** skip input based on the `LLKHF_INJECTED` flag
(see [DESIGN.md](DESIGN.md), pitfall 1) — doing so would make automated testing
impossible.

Required scenarios:

1. Type `tieengs vieejt` → the EDIT control contains `tiếng việt`.
2. Press `Alt + Z`, type again → the control contains `tieengs vieejt` (now off).
3. Press `Alt + Z` again → Vietnamese output resumes (toggle works both ways).
4. Add the test program's own exe name to `exclude.txt` → output is literal even
   though the state is ON.
5. Remove it from `exclude.txt` → Vietnamese output resumes **without
   restarting the app**.
6. Type `hoas` then Backspace twice → exactly the right characters remain,
   nothing extra, nothing missing.
6b. Type `vay roi`, delete back to `vay`, press `a` → vây. The word from before
   the space has to be picked up again.
6c. Type `tieengs`, delete the g, type `gs` → tiếng. Putting a mark back must not
   be read as removing it.
7. Inject 200 characters as fast as possible → every character still correct
   (catches ordering races between real and injected keys).
8. Type continuously for several seconds, then type one more word → the hook is
   still alive (Windows has not dropped it for exceeding the timeout).
9. Ctrl+A to select everything, then type a new word → the new word composes
   from scratch (correct diacritics), not as a continuation of the word it
   replaced. Exercises the Ctrl-chord reset path (hook.cpp) together with a
   real selection.
10. Type a word, Shift+Home to select it, Backspace to delete the selection in
    one go, then type another word → the new word composes correctly. telex
    never learns "how much" text a selection-backspace removed; it only has to
    not be confused afterwards, because Home is a boundary key and already
    resets it independently of Shift.
11. Half-type a word in one window, switch the *foreground window* to a second,
    unrelated window (as Alt+Tab or a taskbar click would — not a mouse click
    inside the first window), type something else there, then switch back →
    typing continues correctly in both windows, and the second window's typing
    never bleeds into the first. Exercises the `WinEventHook` /
    `EVENT_SYSTEM_FOREGROUND` reset path (main.cpp), the one path not covered
    by scenarios 4-10.
12. Post `WM_CLOSE` → the process exits on its own, meaning the message loop was
    healthy and the teardown path (unhook, remove the tray icon) ran.

The test program carries a watchdog that kills it, and any surviving telex.exe,
after two minutes. Injecting global input can wedge if something steals the
foreground, and a test that hangs is worse than one that fails.

## Tier 3 — Manual checklist

Run only before declaring the build done. In each app: type `tieengs vieejt`,
type a sentence containing `đ`, hit Backspace mid-word, press `Alt + Z` twice.

- [ ] Notepad
- [ ] Chrome / Edge address bar (autocomplete interferes here)
- [ ] A Google search box inside a page
- [ ] VS Code (Electron)
- [ ] Discord or Slack (Electron, rich text input)
- [ ] Microsoft Word
- [ ] PowerShell / cmd
- [ ] The File Explorer rename box
- [ ] The Start Menu search box
- [ ] An app running elevated → confirm it does not work (known limitation,
      documented in the README)
- [ ] Right-click the tray icon, check *Start with Windows* → a `telex` value
      appears under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
      pointing at the running exe; uncheck it → the value is gone; sign out and
      back in (or reboot) with it checked → telex is running afterwards

## Leak check

Run for 30 minutes of continuous typing; confirm memory and handle counts are
flat.
