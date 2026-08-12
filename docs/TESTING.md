# TESTING — The test contract both implementations obey

Rule: **never type a few words by hand and call it working**. Every rule in
[TELEX.md](TELEX.md) needs at least one automated case, and the platform layer is
verified by machine, not by eye.

Three tiers, cheapest first. **Tier 1 is shared and is specified here** — the
same required coverage and the same corpus file for both builds. Tiers 2 and 3
are inherently platform-specific and are specified per platform:

| Tier | Windows | macOS |
| --- | --- | --- |
| 1 — engine | this file | this file |
| 2 — end to end | [windows/docs/TESTING.md](../windows/docs/TESTING.md) | [macos/docs/TESTING.md](../macos/docs/TESTING.md) |
| 3 — manual | [windows/docs/TESTING.md](../windows/docs/TESTING.md) | [macos/docs/TESTING.md](../macos/docs/TESTING.md) |

## Why tier 1 is shared

The two implementations share no code, so nothing but a test suite can keep them
behaving the same. Tier 1 is that guarantee: **an implementation is a faithful
telex only once it passes every case below and the whole corpus.** Both builds
read the same [corpus.txt](corpus.txt); it is not copied per platform, precisely
so the two cannot quietly drift apart.

A behaviour difference between the two builds is a bug in one of them, never a
platform difference. If the rules themselves need to change, change
[TELEX.md](TELEX.md) first, then both engines.

## Tier 1 — Engine tests

Run against the pure engine: no OS, no keyboard. This catches 99% of bugs and
finishes in under a second, so it runs after **every** code change.

Case format: a key sequence in, an expected string out.

```
CHECK("tieengs vieejt", "tiếng việt")
CHECK("hello",          "hello")
```

### The harness

A case drives the engine exactly the way the platform layer does, keeping its own
model of what is on screen: on `PassThrough` append the character, on `Replace`
drop `backspaces` characters from the end and append `insert`. `\b` in the key
string means Backspace. Comparing that model against the expected string is what
makes the `backspaces` count testable at all — an implementation that always
rewrites the whole word passes a naive harness and fails this one.

Non-letters are fed to the engine as ordinary keys; the engine itself treats them
as word boundaries.

### Required coverage

Counts are the assertions in the Windows build, which is the reference. A second
implementation is expected to reach every group, not a particular number.

| Group | Covers | Cases |
| --- | --- | --- |
| Spec examples | the full table in TELEX.md §10 | 27 |
| Tones | 5 tones × 12 single vowels (60), changing tone mid-word, `z` removing it and `z` as a literal | 66 |
| Letter transforms | `aa ee oo aw ow uw dd`, standalone `w`, `w` after a consonant, the `uo` + `w` cluster in every typing order (6), and the `qu` onset that `w` must not touch | 26 |
| Retype to undo | tones, letters, `z`, standalone `w`, and carrying on normally after an undo | 18 |
| Free positioning | marks typed before, inside and **after** the coda (`vanas`, `khongo`, `muonos`, `congoj`, `nguyenej`), the validity check stopping it running wild, and the adjacency rule (`vieete`) | 35 |
| đ | both typing orders, at a distance, capitalised, and the English words it eats | 21 |
| Backspace then continue | deleting mid-word and carrying on, putting a mark **back** after a backspace, marks applying to what is left | 23 |
| Backspace across words | adopting a word from history over a space, punctuation or digits; text we never typed | 15 |
| Tone placement | all 5 rules from TELEX.md §5, ≥ 3 each | 25 |
| English words | 24 that must survive verbatim, plus the 5 known manglings (`test`, `win`, `wrong`, `password`, `miss`) | 29 |
| Capitalisation | `Aa`, `DD`, `dD`, `Dd`, `Ows`, whole words, all caps | 9 |
| Backspace | mid-word, over a diacritic, over `đ`, on an empty word | 8 |
| Word boundaries | space, punctuation, digits, tab | 7 |
| Revert to literal | a transformed word that stops being Vietnamese, and the next word starting clean | 6 |
| Syllable validity | the structural checker directly, **including strict vs lenient** | 14 |
| Corpus round-trip | every syllable in [corpus.txt](corpus.txt) | 333 |
| | **Total** | **662** |

The last two groups are unit tests of the layers underneath the state machine
rather than key sequences. The validity group is the only place the strict mode
is exercised at all (see [DESIGN.md](DESIGN.md)); everything else goes through
the engine, which only ever uses lenient.

**Corpus test.** Beyond individual cases, [corpus.txt](corpus.txt) holds ≥ 300
real Vietnamese syllables (one per line) covering every nucleus in the table, all
checked in one pass. Whenever a bug is found in the wild, add a line here
*before* fixing the code — and it then guards both builds at once.

**Round-trip test.** For every syllable in the corpus: derive its Telex spelling
from the syllable itself, type that back, and require the original syllable. This
catches placement bugs that hand-written cases routinely miss. Deriving the
spelling is itself part of the test — it is the same base-letter/mark/keys split
that adopting a word from history uses (TELEX.md §8).

### What a fresh implementation fails first

In order of how often they were the actual bug, not how hard they look:

1. **Marks typed after the whole word.** `khongo` → không, `muonos` → muốn,
   `congoj` → cộng. This is how most people really type and it is the group that
   grew the most during testing.
2. **State surviving between words.** `vay roi` → delete back to `vay` → `a` →
   vây. Nothing else in tier 1 depends on state outliving a word boundary, so an
   implementation that never built the history passes everything else.
3. **Putting a mark back after a backspace.** `tieengs` ⌫ `gs` → tiếng, not
   tiêng. Adjacent retype undoes, non-adjacent applies.
4. **Strict validity used in the input path.** Looks correct on finished words,
   falls apart while typing.

Get these four green before starting any platform work.

### Known gaps in the reference build

Not blocking, but they are gaps, and both builds should close them together:

- Capitalisation has 9 cases where the original target was 10.
- The đ group has 21 where the target was 22.
- Word boundaries has 7 where the target was 8.

## Tier 2 — End to end

Covers what unit tests cannot reach: whether the interception layer actually sees
keys, whether it recurses, whether the right number of backspaces goes out,
whether the process is still alive after sustained typing.

The mechanism differs per platform, but three requirements do not:

1. The test **injects real keys** into a real text control of a real running
   build, and reads the text back. Calling the engine directly is tier 1, not
   tier 2.
2. Because the tests inject keys, the interception layer must **not** filter on
   the platform's generic "injected event" flag — only on our own signature
   (see [DESIGN.md](DESIGN.md), *Output*). This constraint exists to keep the app
   testable and may not be traded away.
3. The test carries a **watchdog** that kills it, and any surviving telex
   process, after two minutes. Injecting global input can wedge if something
   steals the foreground, and a test that hangs is worse than one that fails.

Scenarios each platform must cover, at minimum: correct Vietnamese output;
toggle off and on again; exclusion applied and then removed *without restarting*;
backspace mid-word; a burst of ~200 characters with nothing lost or reordered;
sustained typing without the interception being dropped; and a clean exit that
leaves no icon behind.

## Tier 3 — Manual checklist

Run only before declaring a platform done. In each application: type
`tieengs vieejt`, type a sentence containing `đ`, hit Backspace mid-word, toggle
off and on. The application list is per platform.

## Bug workflow

1. Write a failing test that reproduces the bug **first**; confirm it fails.
2. Fix the code.
3. Re-run everything; confirm no other case regressed.
4. If the bug is that TELEX.md describes the wrong behaviour, fix TELEX.md first,
   then the code — **in both implementations**, or record explicitly why one is
   not affected.

No case may be marked "skip for now", in either build.
