# PLAN — Build order and definition of done

Follow this order. Each step's tests must be green before starting the next —
don't build the Windows layer first and come back to fix the engine later.

## Step 0 — Build skeleton

- `build.bat` invokes VS 18's `vcvars64.bat` then `cl`, with two modes:
  `build.bat` (produces `telex.exe`) and `build.bat test` (compiles and runs the
  tests).
- Done when: an empty `main.cpp` compiles into a runnable exe.

## Step 1 — Data tables (`engine/tables.h`)

- Onset, nucleus and coda tables per TELEX.md §4.
- Mapping from (vowel, tone) to the precomposed Unicode character, upper and
  lower case.
- Done when: table lookups are tested for all 12 vowels × 6 tones × 2 cases.

## Step 2 — Syllable parsing and validation (`engine/syllable.*`)

- Split a string into onset / nucleus / coda.
- Structural validity, including the `qu`/`gi` rule and the tone restriction on
  codas `c ch p t`.
- Tone placement following the 5 rules in TELEX.md §5.
- Done when: the placement function alone has ≥ 40 cases covering all 5 rules.

## Step 3 — Input state machine (`engine/telex.*`)

- `OnKey`, `OnBackspace`, `Reset` per the interface in DESIGN.
- Maintain `buffer_` and `raw_`, the retype-to-undo rule, the revert-to-literal
  rule.
- Compute `backspaces` as the number of trailing characters that actually
  changed, never the whole word.
- Done when: **all of tier 1 in TESTING.md is green**, corpus and round-trip
  included.

From here on, no engine change ships without re-running tier 1.

## Step 4 — Character output (`app/sender.cpp`)

- Batch every backspace and character into a **single** `SendInput` call.
- Stamp `dwExtraInfo = kSignature` on every event sent.
- Done when: typing into Notepad produces the right sequence.

## Step 5 — Keyboard hook (`app/hook.cpp`)

- `WH_KEYBOARD_LL`, handling both `WM_KEYDOWN` and `WM_SYSKEYDOWN`.
- Skip input carrying our own signature.
- Translate key codes to characters accounting for Shift and Caps Lock.
- Reset at the word boundaries listed in TELEX.md §7.
- Done when: tier 2 scenarios 1, 6, 7 and 8 pass.

## Step 6 — `Alt + Z` toggle

- Handled in the hook; swallow both keydown and keyup, and deal with the Alt
  menu-bar pitfall.
- Done when: tier 2 scenarios 2 and 3 pass.

## Step 7 — Tray icon (`app/tray.cpp`)

- Two icons embedded via `.rc`, tooltip, left click toggles, two-item menu.
- Re-register the icon on the `TaskbarCreated` message (Explorer restarts).
- Done when, manually: toggling by hotkey updates the icon, and exiting removes
  the icon immediately.

## Step 8 — Exclusion list (`app/exclusion.cpp`)

- Read `exclude.txt`, `SetWinEventHook` for foreground changes, precompute the
  exclusion flag.
- Create a commented template file the first time the user opens it.
- Done when: tier 2 scenarios 4 and 5 pass.

## Step 9 — Final pass

- Run all three tiers of TESTING.md; tier 3 fully ticked.
- Leak check: run for 30 minutes of continuous typing, confirm memory and handle
  counts are flat.
- Re-read the README: everything under "Có" works, and nothing under "Không có"
  exists in the code.

## Definition of done

1. `build.bat test` is 100% green with no skipped cases.
2. The manual checklist is fully ticked.
3. No feature exists that the README does not describe.
4. Known limitations (elevated windows, `Alt + Z` swallowed globally, `test` →
   `tét`) are stated plainly in the README, not hidden.

## Scope note

If at any point "while we're here, let's also add…" comes up — stop and re-read
*Out of scope* in DESIGN.md. Feature creep is the biggest risk to this project,
bigger than any technical bug.
