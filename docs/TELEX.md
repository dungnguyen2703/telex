# TELEX — Input rule specification

This is the single source of truth for both the engine and the tests. Any
argument about "what UniKey actually does" is settled by editing this file
first, then the code.

Goal: match UniKey's default behaviour (Telex input, Unicode output, classic
tone placement, syllable validity checking).

## 1. Transform keys

**Tone keys** — they apply to the whole syllable and may be typed at any
position:

| Key | Tone | Example |
| --- | --- | --- |
| `s` | acute (sắc) | `as` → á |
| `f` | grave (huyền) | `af` → à |
| `r` | hook (hỏi) | `ar` → ả |
| `x` | tilde (ngã) | `ax` → ã |
| `j` | dot below (nặng) | `aj` → ạ |
| `z` | remove tone | `asz` → a |

**Letter keys** — they modify a vowel of the syllable being typed:

| Key | Result | Example |
| --- | --- | --- |
| `a` after `a` | â | `aa` → â |
| `e` after `e` | ê | `ee` → ê |
| `o` after `o` | ô | `oo` → ô |
| `w` after `a` | ă | `aw` → ă |
| `w` after `o` | ơ | `ow` → ơ |
| `w` after `u` | ư | `uw` → ư |
| `w` after `uo` | ươ (both change) | `duongw` → dương |
| `w` with no vowel yet | ư | `w` → ư |
| `d` after `d` | đ | `dd` → đ |

## 2. Free positioning

Tone keys apply to the **whole syllable** and may be typed anywhere after the
vowel:

- `vieetj` → việt, `vietj` → viẹt, `vieejt` → việt
- `toans` → toán, `hoacj` → hoạc

`w` also works at a distance, since w is never a Vietnamese letter itself: it
applies to the nucleus of the syllable being typed.

- `duongwf` → dường, `dduowngf` → dường (both orders give the same result)
- `w` tries the `uo` → `ươ` cluster before single vowels, and never touches the
  `u` of a `qu` onset (`quowr` → quở, not quưở).

The doubling keys work at a distance too, because most people type the whole
word and add the marks afterwards:

- `vanas` → vấn, `quanaf` → quần, `thayas` → thấy, `tienes` → tiến
- `dandf` → đàn, `danhsd` → đánh. đ is always the onset, so only the first
  letter can become one.

What keeps this from running wild is the validity check: in `ngoeor` the second
`o` would have to make the nucleus `ôe`, which no Vietnamese syllable has, so the
key stays a plain letter and the word comes out as ngoẻo. A letter that already
carries a diacritic is final — `ô` never counts as an `o` still waiting for one.

## 3. Retyping to undo

Retyping the key that was just applied restores the original letters and emits
that key literally:

| Keys | Output |
| --- | --- |
| `a` `s` `s` | as |
| `a` `a` `a` | aa |
| `a` `w` `w` | aw |
| `d` `d` `d` | dd |
| `o` `o` `o` | oo (so `boong` is typed `booong`) |

Typing a **different** tone key replaces the tone and emits nothing:
`asf` → à, `asx` → ã.

`z` removes the tone. If the syllable carries no tone, `z` is emitted literally.

Because the second key is consumed by the undo, an English word with a doubled
tone letter loses one of them: `password` → pasword, `miss` → mis. UniKey
behaves the same way; the exclusion list is the answer.

## 4. Syllable structure (used for validity checking)

```
syllable = [onset] + [nucleus] + [coda] + [tone]
```

- **Onsets**: empty, b, c, ch, d, đ, g, gh, gi, h, k, kh, l, m, n, ng, ngh, nh,
  p, ph, qu, r, s, t, th, tr, v, x
- **Codas**: empty, c, ch, m, n, ng, nh, p, t
- **Nuclei**: a closed set, fully enumerated in `engine/tables.h`. Covers single
  vowels (a ă â e ê i o ô ơ u ư y), diphthongs (ai ao au ay âu ây eo êu ia iê iu
  oa oă oe oi ôi ơi ua uâ uê ui uô uơ uy ưa ưi ươ ưu) and triphthongs (iêu yêu
  oai oay oeo uao uây uôi uya uyê ươi ươu uyu).
- Extra constraints: codas `c`/`ch`/`p`/`t` only combine with the acute or dot
  tone (no `bàc`, no `bảt`). `ch`/`nh` only follow front vowels (a, ê, i, y, …).

This is a **structural** check, not a dictionary lookup. `bươn`, `khoét` and
`nghiễng` are all valid even if they are not real words.

## 5. Tone placement (classic style)

Evaluate in order, stop at the first rule that matches:

1. If the onset is `qu` or `gi` **and** another vowel follows it, that `u`/`i`
   belongs to the onset and does not count as a nucleus vowel.
   (`quy` → **quý**, `gia` → **già**)
2. If the nucleus contains `ê` or `ơ`, the tone goes there.
   (`tieengs` → **tiếng**, `nguowif` → **người**)
3. If there is exactly one vowel, the tone goes on it. (`banj` → **bạn**)
4. If there is a coda, the tone goes on the **last** vowel.
   (`muoons` → **muốn**, `hoafn` → **hoàn**)
5. Otherwise (open syllable), the tone goes on the **second-to-last** vowel.
   (`hoaf` → **hòa**, `thuyr` → **thủy**, `cuar` → **của**, `mias` → **mía**)

Rule 5 is what separates "classic" from "modern" placement. We use classic
(**hòa**, **thủy**), matching UniKey's default. There is no option to change it.

## 6. Reverting invalid syllables

The engine keeps two parallel strings: `buffer_` (displayed) and `raw_` (the
literal keys pressed).

- **If no transform has been applied yet** (`buffer_ == raw_`), skip all checks
  and pass every key straight through. This is the path almost all English text
  takes, so it must be cheap.
- **When applying a transform** would produce an invalid syllable, don't apply
  it — emit the key literally. (`s` at word start → `s`; `hells` → `hells`)
- **When appending a plain letter** to a word that already has a transform makes
  the syllable invalid, **revert the whole word to `raw_`** (send backspaces,
  retype the original keys) and mark the word "dead": every remaining key of the
  word passes straight through.
  (`hello`: `hel` has an invalid coda → revert to `hel`, then `l`, `o` pass
  through → **hello**)

Known and accepted consequence: `test` → **tét**, because `tét` is a valid
syllable. UniKey behaves the same way. That is exactly why the exclusion list
exists.

## 7. Word boundaries

There are two of them, and the difference matters.

**Ending a word** — start a fresh word but remember the text just typed, so that
backspacing back into it can pick it up again (section 8):

- Space
- Any non-letter character (punctuation, digits, symbols)

**Forgetting everything** — the caret has gone somewhere we cannot follow, so
anything we think we know about the text in front of it is worthless:

- Navigation keys: arrows, Home, End, PageUp/Down, Delete, Enter, Tab, Escape
- Any chord involving Ctrl / Alt / Win
- Mouse click
- Foreground window change
- Toggling with `Alt + Z`, or moving into or out of an excluded application

Guessing wrong here means rewriting text the user never touched, so when in
doubt, forget.

## 8. Backspace

Drop one character from the end of `buffer_` and the corresponding keys from the
end of `raw_`, then let the Backspace key through untouched (don't swallow it,
don't compensate). Never try to reconstruct a diacritic that was deleted. If
`buffer_` is empty, just pass through.

When `raw_` is longer than `buffer_` (e.g. `dd` → `đ`), remove all raw keys that
produced the deleted character, so subsequent typing does not desynchronise.

**Backspacing past the start of a word adopts the previous one.** Someone types
`vay`, a space, a few more words, then deletes back to `vay` and presses `a`
expecting vây. That only works if the engine takes the word back, so it keeps the
last 128 characters it has itself put on screen and rebuilds the word from them -
splitting each character back into its base letter, its mark and the Telex keys
that would produce it. The moment we are no longer sure what sits in front of the
caret (section 7), that memory is dropped.

**Backspace also disables reverting for the rest of the word** (section 6). Once
the user has deleted something, whatever is left is text they have looked at and
kept; turning a đ they already accepted back into `dd` under their hands is far
worse than leaving an odd-looking word alone. Marks still apply as usual:
`đường` ⌫⌫⌫ `ngf` → đừng.

## 9. Capitalisation

Transform keys preserve the case of the target vowel: `Aa` → Â, `DD` → Đ,
`dD` → Đ, `Ows` → Ớ. Tone keys never change case.

## 10. Reference examples (tests read directly from this table)

| Keys | Output |
| --- | --- |
| `tieengs vieejt` | tiếng việt |
| `Tieengs Vieejt` | Tiếng Việt |
| `xin chaof` | xin chào |
| `ddaji hocj quoocs gia` | đại học quốc gia |
| `nguwowif` / `nguoiwf` | người |
| `khoong cos gif` | không có gì |
| `thuyr tinh` | thủy tinh |
| `hoaf binhf` | hòa bình |
| `quays` | quáy |
| `quys` | quý |
| `giaf` | già |
| `cuar` | của |
| `mias` | mía |
| `ruowuj` | rượu |
| `nghieengf` | nghiềng |
| `bows` | bớ |
| `aas` | ấ |
| `w` | ư |
| `ww` | w |
| `booong` | boong |
| `hello` | hello |
| `sport` | sport |
| `email` | email |
| `test` | tét *(known, accepted)* |
| `asz` | a |
| `assf` | asf |
