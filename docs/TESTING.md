# TESTING — How we test so nothing slips through

Rule: **never type a few words by hand and call it working**. Every rule in
[TELEX.md](TELEX.md) needs at least one automated case, and the Windows layer is
verified by machine, not by eye.

Three tiers, cheapest first:

## Tier 1 — Engine unit tests (`tests/engine_tests.cpp`)

Run against the pure engine: no Windows, no keyboard. This catches 99% of bugs
and finishes in under a second, so it runs after **every** code change.

Case format: a key sequence in, an expected string out.

```cpp
CHECK("tieengs vieejt", u"tiếng việt");
CHECK("hello",          u"hello");
```

Required coverage:

| Group | Minimum cases |
| --- | --- |
| 5 tones × every single vowel (a ă â e ê i o ô ơ u ư y) | 60 |
| Letter transforms `aa ee oo aw ow uw dd` | 3 each |
| The `uo` + `w` → ươ cluster in every typing order (`uow`, `uwow`, `uongw`) | 6 |
| Retype-to-undo: tones, letters, `z`, standalone `w` | 10 |
| Changing tone mid-word (`asf`, `axj`) | 5 |
| Free positioning: tone before and after the coda | 10 |
| All 5 tone-placement rules from TELEX.md §5, ≥ 3 each | 15 |
| The `qu` and `gi` exceptions | 6 |
| English words that must survive verbatim (hello, sport, email, world, string…) | 20 |
| Capitalisation and Caps Lock | 10 |
| Backspace mid-word, over a diacritic, over `đ` | 8 |
| Backspace, then carry on typing and marking the word | 12 |
| Backspace back across a space into an earlier word | 14 |
| đ in both typing orders, plus what English words it eats | 22 |
| Word boundaries: space, punctuation, digits | 8 |
| The full example table in TELEX.md §10 | all |

**Corpus test.** Beyond individual cases, `tests/corpus.txt` holds ≥ 300 real
Vietnamese syllables (one per line, `telex → expected`) covering every nucleus
in the table, all checked in one pass. Whenever a bug is found in the wild, add
a line here *before* fixing the code.

**Round-trip test.** For every syllable in the corpus: derive its Telex spelling
from the syllable itself, type that back, and require the original syllable. This
catches placement bugs that hand-written cases routinely miss.

## Tier 2 — Real end-to-end tests (`tests/e2e_test.cpp`)

Covers what unit tests cannot reach: whether the hook actually intercepts keys,
whether it recurses, whether the right number of backspaces goes out.

Mechanism: the test program creates its own window with an `EDIT` control,
brings it to the foreground, injects keys with `SendInput`, then reads the
control back with `WM_GETTEXT` and compares.

This is why the hook must **not** skip input based on the `LLKHF_INJECTED` flag
(see DESIGN pitfall 1) — doing so would make automated testing impossible.

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
9. Post `WM_CLOSE` → the process exits on its own, meaning the message loop was
   healthy and the teardown path (unhook, remove the tray icon) ran.

The test program carries a watchdog that kills it, and any surviving telex.exe,
after two minutes. Injecting global input can wedge if something steals the
foreground, and a test that hangs is worse than one that fails.

## Tier 3 — Manual checklist

Run only before declaring the project done. In each app: type `tieengs vieejt`,
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

## Running the tests

`build.bat test` compiles and runs tier 1 then tier 2, prints pass/fail counts,
and exits non-zero if anything failed. No case may be marked "skip for now".

## Bug workflow

1. Write a failing test that reproduces the bug **first**; confirm it fails.
2. Fix the code.
3. Re-run everything; confirm no other case regressed.
4. If the bug is that TELEX.md describes the wrong behaviour, fix TELEX.md first,
   then the code.
