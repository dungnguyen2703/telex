# PLAN — Build order and definition of done (macOS)

Same discipline as the Windows build did in
[windows/docs/PLAN.md](../../windows/docs/PLAN.md): each step's tests are green
before the next one starts. The engine is finished before a single line of AppKit
is written.

The Windows sources are the behavioural reference, not a template to translate
mechanically. Read [docs/TELEX.md](../../docs/TELEX.md) and
[docs/DESIGN.md](../../docs/DESIGN.md) first — they were corrected against the
Windows code specifically so this port could be built from them.

## Step 0 — Skeleton

- `Package.swift` with three targets: `TelexEngine`, `TelexApp`,
  `TelexEngineTests`.
- `build.sh` with three modes: default, `engine`, `test`.
- Done when: `swift build` succeeds and `./build.sh engine` runs zero tests
  without failing.

## Step 1 — Tables (`Sources/TelexEngine/Tables.swift`)

- Onset, nucleus and coda tables per TELEX.md §4.
- (vowel, tone) → precomposed character, and the reverse decomposition, both
  cases.
- Done when: lookups are tested for all 12 vowels × 6 tones × 2 cases, and
  compose/decompose round-trip.

## Step 2 — Syllable (`Sources/TelexEngine/Syllable.swift`)

- Split into onset / nucleus / coda.
- **Both validity modes**, lenient and strict, per TELEX.md §4 — and remember
  that the engine will only ever call lenient.
- Tone placement, the 5 rules of TELEX.md §5 in order.
- Done when: the 14 validity cases pass, including the strict/lenient contrasts
  (`nghieng`, `q`).

## Step 3 — Engine (`Sources/TelexEngine/Engine.swift`)

- `onKey`, `onBackspace`, `endWord`, `reset`, `display`, `raw` per the engine
  contract in [docs/DESIGN.md](../../docs/DESIGN.md).
- The letter list carrying base/tone/upper/src, plus `history` (128), `dead`,
  `edited` and `lastTransform`.
- `backspaces` = trailing characters that actually changed, never the whole word.
- Done when: **all of tier 1 is green — 662 assertions**, corpus and round-trip
  included.

From here on, no engine change ships without re-running tier 1.

## Step 4 — Output (`Sources/TelexApp/Sender.swift`)

- One `CGEventSource`; post every backspace and character through it in order.
- Unicode string set on both key down and key up.
- Signature stamped on every event.
- Done when: a scratch harness types into TextEdit correctly.

## Step 5 — Event tap (`Sources/TelexApp/EventTap.swift`)

- `CGEvent.tapCreate`, the `@convention(c)` callback with state via `refcon`.
- Skip our own signature; handle `.tapDisabledByTimeout` and
  `.tapDisabledByUserInput` by re-enabling.
- Character from `keyboardGetUnicodeString`; the flow of
  [docs/DESIGN.md](../../docs/DESIGN.md) exactly, including `endWord` for space
  and punctuation versus `reset` for navigation.
- Mouse down resets.
- Done when: tier 2 scenarios 1, 6, 7 and 8 pass.

## Step 6 — Permission gate and `Control + Space`

- `AXIsProcessTrustedWithOptions` at startup, alert, deep link to System
  Settings, poll until granted.
- Toggle detected by key code, both down and up swallowed.
- Done when: tier 2 scenarios 2 and 3 pass and no stray space is ever typed.

## Step 7 — Status item (`StatusItem.swift`, `Icon.swift`)

- Two icons drawn at runtime, tooltip, click toggles, two-item menu.
- Updates marshalled to the main queue.
- Done when, manually: toggling by hotkey updates the icon, and quitting removes
  it immediately.

## Step 8 — Exclusion (`Exclusion.swift`)

- `~/Library/Application Support/telex/exclude.txt`, bundle identifiers, worker
  thread with a one-second poll, `didActivateApplicationNotification` for an
  immediate re-check, one atomic flag read by the tap.
- Commented template created on first open.
- Done when: tier 2 scenarios 4 and 5 pass.

## Step 9 — Packaging (`build.sh`)

- Assemble `build/telex.app`: `Contents/MacOS/telex`, `Contents/Info.plist` with
  `LSUIElement`, ad-hoc `codesign`.
- Done when: double-clicking the app puts the icon in the menu bar and typing
  works after granting Accessibility.

## Step 10 — Final pass

- All three tiers; tier 3 fully ticked.
- Leak check: 30 minutes of continuous typing, memory flat.
- Re-read the README: everything under "Có" works on macOS too, and nothing under
  "Không có" exists in the code.

## Definition of done

1. `./build.sh test` is 100% green with no skipped cases, and tier 1 reports the
   same **662** as the Windows build.
2. The manual checklist is fully ticked.
3. No feature exists that the README does not describe.
4. Known limitations (Secure Event Input, `Control + Space` swallowed globally,
   Gatekeeper, re-granting after rebuild) are stated plainly in the README.

## Scope note

The *Out of scope* list is shared and lives in
[docs/DESIGN.md](../../docs/DESIGN.md). The Accessibility permission flow is its
one sanctioned exception, because without it the app cannot run at all. Nothing
else gets added on the grounds that "macOS users expect it".
