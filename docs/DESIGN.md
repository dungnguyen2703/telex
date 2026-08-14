# DESIGN — The contract both implementations obey

telex is built twice, independently:

| | Windows | macOS |
| --- | --- | --- |
| Sources | [`windows/`](../windows/) | [`macos/`](../macos/) |
| Language | C++17, plain Win32 | Swift |
| Design notes | [windows/docs/DESIGN.md](../windows/docs/DESIGN.md) | [macos/docs/DESIGN.md](../macos/docs/DESIGN.md) |

**No code is shared between them.** Not a header, not a helper. The Windows
build works because it was fixed against a real keyboard in real applications,
and refactoring it to fit a second platform would put every one of those fixes
at risk for no user-visible gain.

What *is* shared is this directory: the input rules, the architecture, and the
tier 1 test suite. A change here is a change to both products. This file is the
architecture half; [TELEX.md](TELEX.md) is the input rules and
[TESTING.md](TESTING.md) is the test contract.

All documentation, code, comments, and commit messages are in English. The only
Vietnamese in the repo is the README and the Vietnamese text data itself.

## Fixed for both platforms

| Item | Choice | Why |
| --- | --- | --- |
| Input method | Telex only | No VNI, no VIQR, no selector |
| Output encoding | Precomposed Unicode only | No TCVN3, no VNI-Windows, no VIQR |
| Tone placement | Classic (hòa, thủy) | Matches UniKey's default; not configurable |
| Persisted state | None. Always starts ON | No config file, no settings window |
| Toggle | One global chord, fixed | Not rebindable |
| Per-application opt-out | A plain text file, one name per line | The only file the user ever edits |

Neither build uses the platform's "correct" text-input framework (TSF on
Windows, an Input Method Kit component on macOS). Both intercept keys globally
and synthesise replacement text instead. The correct frameworks require running
inside other processes, registration, and a far larger surface than this scope
justifies — and interception is what UniKey does by default too.

## The layering rule

The single most important boundary in both implementations: **the engine must
not know that an operating system exists.** No OS headers, no OS types, no I/O,
no globals. That is what makes the entire input logic testable without a real
keyboard, and it is what makes a second implementation possible at all.

```
engine   <- pure language. Key in -> edit commands out. Testable offline.
             tables      vowel/tone -> precomposed character, both cases
             syllable    onset/nucleus/coda split, validity, tone placement
             telex       the input state machine
app      <- the platform layer, as thin as possible, almost no logic
             entry       process lifetime, event loop, startup/teardown
             hook/tap    global key interception, filtering, swallowing
             sender      emits backspaces + characters
             indicator   the tray / menu bar icon and its menu
             icon        draws the two icons at runtime, no binary assets
             exclusion   reads the list, resolves the foreground application
```

If a rule about *what Vietnamese text should come out* ever ends up in the app
layer, it is in the wrong place. The app layer decides only: is this key ours to
look at, and what do I do with the answer.

## The engine contract

The engine is effectively a pure function: **state + key in → edit commands
out**. It never sends anything itself and knows nothing about how text reaches
the screen.

```
Action  = PassThrough | Replace

Result  = { action, backspaces, insert }

Letter  = { base, tone, upper, src }
            base   toneless lowercase base ('a', 'ă', 'đ', 'b', ...)
            tone   only meaningful on vowels
            upper  case of this letter
            src    the raw keys that produced this letter

Engine
    OnKey(ch)      -> Result
    OnBackspace()  -> Result
    EndWord(ch)
    Reset()
    Display()      -> the word as it currently looks on screen
    Raw()          -> the literal keys pressed for this word
```

Four points that a second implementation gets wrong if it is not told:

1. **`OnKey` takes a character, not a key code, and the case is already
   applied.** Resolving Shift and Caps Lock into an actual character is the app
   layer's job. The engine never sees modifiers.
2. **The word is a list of `Letter`s, not a string.** Each letter carries its
   base, its tone, its case and the raw keys that produced it, side by side.
   Keeping the rendered form and the original keystrokes together per letter is
   what makes retype-to-undo, revert-to-literal, and backspace over a `đ` all
   work. A plain "displayed string + raw string" pair is not enough.
3. **There is no `Swallow` action.** A key is either passed through untouched or
   replaced by `backspaces` + `insert`. Nothing is ever eaten silently.
4. **`backspaces` is the number of trailing characters that actually changed**,
   never the whole word. Typing `hoas` needs 2 backspaces (`oa` → `òa`), not 4.
   Getting this wrong is invisible in a text editor and destroys text in a
   browser address bar.

### Engine state that is easy to miss

Beyond the letters and the raw keys, the state machine carries three flags and a
history buffer. All four came out of bugs found in real use, and all four have
to exist in any implementation:

| State | Meaning |
| --- | --- |
| `history` | The last **128** characters the engine itself put on screen, ended words included, so backspacing back into them can rebuild that word (TELEX.md §8). Dropped the moment we can no longer be sure what is in front of the caret. |
| `dead` | The word was reverted to literal text; every remaining key of it passes straight through (TELEX.md §6). |
| `edited` | Backspace was used in this word; never rewrite the whole word again (TELEX.md §6, §8). Adopting a word back out of `history` counts as edited too. |
| `lastTransform` | The key that applied the transform currently on screen, or none. Retyping a key only undoes it when it comes **straight after** (TELEX.md §3), so that putting a mark back after a backspace is not read as removing it. Cleared by any plain letter, any backspace, and every word boundary. |

`Reset()` clears all four. `EndWord(ch)` clears the last three but **appends to
`history`** — the word as displayed, followed by `ch` as the separator, or a
space when `ch` is 0 because the platform layer could not say which character
ended the word.

### Syllable validity has two modes and the engine uses only one

The syllable checker answers two different questions — "is this a complete
syllable?" (strict) and "could this still become one?" (lenient). **Every call
in the input path uses lenient.** Strict exists, and is unit-tested, but nothing
in the running engine calls it.

This is the single easiest way to build a second implementation that looks
correct on finished words and falls apart while someone is actually typing. See
[TELEX.md](TELEX.md) §4.

## Flow of a single keystroke

The app layer runs exactly this. The order matters; so does the difference
between the three ways a word can end.

```
a key arrives
        |
   1. Is this input we injected ourselves?      -> pass it on, do NOT process
        |                                          (breaks the recursion)
   2. Is it the toggle chord?                   -> toggle, swallow it, done
        |
   3. Has the excluded state changed since the
      last key we looked at?                    -> Engine.Reset()
        |
   4. Currently OFF, or foreground app excluded? -> pass it on, don't touch it
        |
   5. Ctrl / Alt / Meta held?                   -> Engine.Reset(), pass it on
        |
   6. Backspace?          -> Engine.OnBackspace(), pass the key through
   7. Space?              -> Engine.EndWord(' '),  pass the key through
   8. Navigation, Enter, Tab, Escape?
                          -> Engine.Reset(),       pass the key through
   9. Any other non-letter (digit, punctuation, function key)?
                          -> Engine.EndWord(0),    pass the key through
        |
  10. A letter: resolve Shift + Caps Lock into a character
        |
      Engine.OnKey(ch)
        |
      PassThrough -> pass it on, the key types itself as usual
      Replace     -> swallow the key, emit backspaces + insert
```

Steps 3, 7, 8 and 9 are the ones that get skipped by mistake:

- **Step 3.** Keys typed while we were standing aside never reached the engine,
  so what it remembers about the text is no longer true — in *both* directions,
  entering and leaving an excluded application.
- **Steps 7 and 9 are `EndWord`, step 8 is `Reset`.** These are not the same
  thing (TELEX.md §7). `EndWord` starts a fresh word but *remembers* the text, so
  backspacing back into it picks it up again — that is what makes `vay roi`,
  delete back to `vay`, press `a` produce vây. `Reset` forgets everything.
  Collapsing steps 7 and 9 into `Reset` costs 14 test cases and a feature nobody
  will report as a bug, they will just quietly think the app is dumb.
- **Step 6 never swallows the Backspace key.** The engine updates its own state
  and the real Backspace does the real deletion. Do not compensate for it.

`Engine.Reset()` is also called on: mouse click, foreground window change, and
every toggle of the ON/OFF state.

## Output

Text goes out as **backspaces followed by characters, in one single batch** —
one call to whatever the platform's injection API is, never a call per key.
Applications that process input asynchronously (anything Chromium-based, which
is most of them) reorder events that arrive in separate calls, and the user sees
scrambled letters.

Every event injected must carry a **signature** that the interception layer
checks first and skips. Without it, our own output comes straight back in and
recurses.

The signature is `0x54454C58` (`'TELX'`) on both platforms. Skip *only* by this
signature, never by the platform's generic "this event was injected" flag — the
end-to-end tests inject keys too, and those must be processed like real ones.
Filtering on the generic flag makes the app untestable.

## Exclusion list

Shared semantics; the file location and what counts as an "application name" are
per-platform.

- One application per line. Blank lines and lines starting with `#` are ignored.
- Anything after the application name on a line is a free-form description and
  is ignored. The template file ships fully commented out and uses this.
- Comparison is case-insensitive, and against the application's identity only —
  never a full path.
- A missing file means an empty list, not an error.
- The list is **hot-reloaded**. Saving the file takes effect at once; no restart.
  Polling is what makes this true even when the foreground never changes.
- Reading the file and resolving the foreground application are both slow enough
  to matter and **must not run on the thread that handles input**. That work
  publishes a single boolean; the input path only ever reads that flag.
- Exclusion does not change the ON/OFF state. Leaving the application resumes
  typing immediately.

## Status indicator

- Two icons: ON (white **V** on a red rounded square) and OFF (the same shape in
  grey), drawn in code at startup so the build needs no binary assets.
- The **application icon** — what Finder or Explorer shows for the file itself —
  is the one exception, because the shell reads it out of the file without ever
  running the program. It is the same artwork, and both builds generate it from
  the same drawing code at build time rather than checking in a picture.
- The tooltip / title states the current mode and the toggle chord.
- Clicking the icon toggles. The menu has exactly two items: open the exclusion
  list, and quit.
- The icon updates the moment the state changes, including via the hotkey.
- Whatever updates the icon must not run on the input thread — talking to the
  shell can block long enough to lose keystrokes.

## Out of scope

To prevent creep, in both implementations: no run-at-startup, no auto-update, no
log files, no settings window, no UI localisation, no configurable input method,
no macros or abbreviations, no spell check, no clipboard conversion, no
rebindable hotkey.

Adding anything on this list is a scope violation. If "while we're here, let's
also add…" comes up, re-read this section. Feature creep is a bigger risk to
this project than any technical bug.

The one sanctioned exception is a platform *requirement* rather than a feature:
macOS cannot intercept keys at all without the user granting Accessibility
permission, so that build has to ask for it. See
[macos/docs/DESIGN.md](../macos/docs/DESIGN.md).

**Run-at-startup exception:** by explicit request, both builds are getting a
"launch at login" toggle in their tray/menu-bar menu, in deviation from the
list above. It does not touch persisted *behaviour* state (still always starts
ON, no config file) — only whether the OS launches the app at logon. Windows is
done first (`HKCU\...\Run`, see [windows/docs/DESIGN.md](../windows/docs/DESIGN.md));
macOS (`SMAppService` / a login item) is planned but not implemented yet — see
[macos/docs/DESIGN.md](../macos/docs/DESIGN.md) once it lands.
