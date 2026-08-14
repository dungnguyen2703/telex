# TESTING — The macOS build

Tier 1 (the engine, the corpus, the round-trip test) is shared and specified in
[docs/TESTING.md](../../docs/TESTING.md). This file covers only what is specific
to macOS.

## Running the tests

From the `macos/` directory:

```
./build.sh          # build build/telex.app
./build.sh engine   # tier 1 only
./build.sh test     # tier 1 then tier 2
```

`./build.sh test` prints pass/fail counts and exits non-zero if anything failed.
No case may be marked "skip for now".

## Tier 1 wiring

`swift test` runs the `TelexEngineTests` target, which links `TelexEngine` alone
— no AppKit. The corpus is read from `../docs/corpus.txt`, the same file the
Windows build checks itself against.

**The reference is 662 assertions.** That is what the Windows build reports, and
the macOS build is not finished until it reports the same. A group that comes out
short means a rule was dropped in the port, not that the platform differs.

Tier 1 needs no permissions and no signing. It runs on any Mac with the Swift
toolchain, and it is the entire test story until the app layer exists.

## Accessibility, and why tier 2 is not fully automatic

Event taps require Accessibility permission, and that applies to **two** separate
processes:

1. `build/telex.app` — the app under test.
2. The process running the tier 2 test, which injects keys with `CGEvent.post`.
   When run from a terminal, that is the terminal application itself (Terminal,
   iTerm, or the IDE hosting the shell), not the test binary.

Neither can be granted from a script. Both are granted by hand in
**System Settings → Privacy & Security → Accessibility**.

This means **tier 2 cannot run on a clean CI machine**, and that is a fact about
macOS rather than a gap in the tests. `./build.sh test` detects the missing
permission with `AXIsProcessTrusted()` and stops with an explanation instead of
reporting phantom failures.

**After every rebuild the signature changes and the permission must be granted
again.** The quickest loop is to remove the stale entry with the `−` button and
re-add the freshly built `build/telex.app` with `+`.

## Tier 2 — Real end-to-end tests

Mechanism: the test creates its own window with an `NSTextField`, brings it to
the front, injects keys with `CGEvent.post(tap: .cgSessionEventTap)`, then reads
`stringValue` back and compares. It launches the real `telex.app` first and
terminates it at the end.

As on Windows, the tap must **not** filter on anything except our own signature
(see [DESIGN.md](DESIGN.md), pitfall 1) — the test's own injected keys have to be
processed like real ones.

The test carries a watchdog that kills it, and any surviving telex process, after
two minutes. Injecting global input can wedge if something steals the front-most
position, and a test that hangs is worse than one that fails.

Required scenarios — the same nine the Windows build covers, so a behaviour
difference shows up as a failure on one platform only:

1. Type `tieengs vieejt` → the field contains `tiếng việt`.
2. Press `Control + Space`, type again → the field contains `tieengs vieejt`
   (now off), and no stray space anywhere.
3. Press `Control + Space` again → Vietnamese output resumes.
4. Add the test's own bundle identifier to `exclude.txt` → output is literal even
   though the state is ON.
5. Remove it → Vietnamese output resumes **without restarting the app**.
6. Type `hoas` then Backspace twice → exactly `h` remains.
6b. Type `vay roi`, delete back to `vay`, press `a` → vây.
6c. Type `tieengs`, delete the `g`, type `gs` → tiếng.
7. Inject 200 characters back to back → every character still correct.
   Unlike the Windows harness, which can use a zero delay, this one paces keys
   at 3 ms. `CGEvent.post` is asynchronous through the window server, so with no
   delay at all the test's next key can overtake telex's correction for the
   previous one. 3 ms is still around thirty times faster than a fast typist,
   and the scenario still does what it is for: catching ordering races.
8. Type continuously for several seconds, then type one more word → the tap is
   still alive, i.e. a `.tapDisabledByTimeout` was caught and re-enabled.
9. Cmd+A to select everything, then type a new word → the new word composes
   from scratch, not as a continuation of the word it replaced. Exercises the
   Command-chord reset path (`EventTap.swift`) together with a real selection.
10. Type a word, select it, Delete to remove the selection in one go, then type
    another word → the new word composes correctly. telex never learns "how
    much" a selection-delete removed; it only has to not be confused
    afterwards, because the key that made the selection was already a
    boundary key or a Command chord, either of which resets it on its own.
11. Half-type a word in one window, switch to a second window (a real click,
    not just `makeKeyAndOrderFront`), type something else there, then switch
    back → typing continues correctly in both windows, and the second
    window's typing never bleeds into the first.
    **Platform note:** unlike Windows, whose foreground-window detection
    covers a window switch on its own, macOS's `didActivateApplicationNotification`
    (`main.swift`) only fires on a change of *application* — switching between
    two windows of the same app does not trigger it. What actually covers this
    case is the mouseDown reset path (`EventTap.swift`), the same one a real
    user's click hits when they switch windows. A window switch with **no**
    click at all (e.g. a trackpad gesture or Mission Control with the pointer
    never touching either window) is not covered by any reset path and is a
    known gap — rare in practice, since composing text and then switching
    windows without touching the mouse or a Cmd-chord is unusual, but worth
    knowing about rather than assuming it is handled.
12. Terminate the app → it exits on its own and the menu bar icon disappears.

## Tier 3 — Manual checklist

Run only before declaring the build done. In each app: type `tieengs vieejt`,
type a sentence containing `đ`, hit Backspace mid-word, press `Control + Space`
twice.

- [ ] TextEdit
- [ ] Safari address bar (autocomplete interferes here)
- [ ] A Google search box inside a page
- [ ] Chrome (Chromium event handling, the ordering risk)
- [ ] VS Code (Electron)
- [ ] Slack or Discord (Electron, rich text input)
- [ ] Notes
- [ ] Pages or Microsoft Word
- [ ] Terminal / iTerm
- [ ] Spotlight
- [ ] Finder rename box
- [ ] A password field → confirm it does **not** work (Secure Event Input, known
      limitation, documented in the README)

## Leak check

Run for 30 minutes of continuous typing; confirm memory is flat in Activity
Monitor and the tap has not been dropped.
