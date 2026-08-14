# PLAN — Build order and definition of done (Windows)

**Status: shipped.** Every step below is done and all three tiers are green.
This file is kept as the record of how the build was put together and what
"done" was measured against — the macOS port follows its own plan in
[macos/docs/PLAN.md](../../macos/docs/PLAN.md).

**Post-ship addition (2026-08-14):** "Start with Windows" (`src/app/autostart.cpp`,
a tray menu checkbox backed by `HKCU\...\Run`) was added on explicit request,
as a deliberate deviation from the shared *Out of scope* list — see
[DESIGN.md](DESIGN.md). Same build, same tiers; no new step number since the
existing Step 7/9 loop (tray menu, done-when checked manually) already covers
how a new menu item gets verified.

The order mattered: each step's tests were green before the next one started.
Don't build the platform layer first and come back to fix the engine later.

## Step 0 — Build skeleton

- `build.bat` invokes VS 18's `vcvars64.bat` then `cl`, with two modes:
  `build.bat` (produces `telex.exe`) and `build.bat test` (compiles and runs the
  tests).
- Done when: an empty `main.cpp` compiles into a runnable exe.

## Step 1 — Data tables (`src/engine/tables.*`)

- Onset, nucleus and coda tables per [TELEX.md](../../docs/TELEX.md) §4.
- Mapping from (vowel, tone) to the precomposed Unicode character, upper and
  lower case.
- Done when: table lookups are tested for all 12 vowels × 6 tones × 2 cases.

## Step 2 — Syllable parsing and validation (`src/engine/syllable.*`)

- Split a string into onset / nucleus / coda.
- Structural validity, including the `qu`/`gi` rule and the tone restriction on
  codas `c ch p t`.
- Tone placement following the 5 rules in TELEX.md §5.
- Done when: the placement function alone has ≥ 40 cases covering all 5 rules.

## Step 3 — Input state machine (`src/engine/telex.*`)

- `OnKey`, `OnBackspace`, `EndWord`, `Reset` per the engine contract in
  [docs/DESIGN.md](../../docs/DESIGN.md).
- Maintain the letter list and the raw keys, the retype-to-undo rule, the
  revert-to-literal rule.
- Compute `backspaces` as the number of trailing characters that actually
  changed, never the whole word.
- Done when: **all of tier 1 in [docs/TESTING.md](../../docs/TESTING.md) is
  green**, corpus and round-trip included.

From here on, no engine change ships without re-running tier 1.

## Step 4 — Character output (`src/app/sender.cpp`)

- Batch every backspace and character into a **single** `SendInput` call.
- Stamp `dwExtraInfo = kSignature` on every event sent.
- Done when: typing into Notepad produces the right sequence.

## Step 5 — Keyboard hook (`src/app/hook.cpp`)

- `WH_KEYBOARD_LL`, handling both `WM_KEYDOWN` and `WM_SYSKEYDOWN`.
- Skip input carrying our own signature.
- Translate key codes to characters accounting for Shift and Caps Lock.
- Handle the word boundaries listed in TELEX.md §7 — note that space and
  punctuation end the word while navigation keys forget everything.
- Done when: tier 2 scenarios 1, 6, 7 and 8 pass.

## Step 6 — `Alt + Z` toggle

- Handled in the hook; swallow both keydown and keyup, and deal with the Alt
  menu-bar pitfall.
- Done when: tier 2 scenarios 2 and 3 pass.

## Step 7 — Tray icon (`src/app/tray.cpp`)

- Two icons drawn at runtime, tooltip, left click toggles, two-item menu.
- Re-register the icon on the `TaskbarCreated` message (Explorer restarts).
- Done when, manually: toggling by hotkey updates the icon, and exiting removes
  the icon immediately.

## Step 8 — Exclusion list (`src/app/exclusion.cpp`)

- Read `exclude.txt`, `SetWinEventHook` for foreground changes, precompute the
  exclusion flag on a worker thread.
- Create a commented template file the first time the user opens it.
- Done when: tier 2 scenarios 4 and 5 pass.

## Step 9 — Final pass

- Run all three tiers; tier 3 fully ticked.
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

The *Out of scope* list is shared and lives in
[docs/DESIGN.md](../../docs/DESIGN.md). Feature creep is the biggest risk to this
project, bigger than any technical bug.
