#include "telex.h"

#include <algorithm>

#include "syllable.h"
#include "tables.h"

namespace telex {
namespace {

constexpr char16_t kABreve = 0x0103;  // ă
constexpr char16_t kACirc  = 0x00E2;  // â
constexpr char16_t kECirc  = 0x00EA;  // ê
constexpr char16_t kOCirc  = 0x00F4;  // ô
constexpr char16_t kOHorn  = 0x01A1;  // ơ
constexpr char16_t kUHorn  = 0x01B0;  // ư

bool IsAsciiLetter(char16_t c) {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

bool IsToneKey(char16_t low) {
    return low == u's' || low == u'f' || low == u'r' || low == u'x' ||
           low == u'j' || low == u'z';
}

uint8_t ToneOfKey(char16_t low) {
    switch (low) {
        case u's': return kAcute;
        case u'f': return kGrave;
        case u'r': return kHook;
        case u'x': return kTilde;
        case u'j': return kDot;
        default:   return kNoTone;  // 'z' removes the tone
    }
}

// Letters before the nucleus belong to the onset and must not be transformed:
// the u of "qu" is not a vowel we may turn into ư ("quowr" is quở, not quưở).
size_t NucleusStart(const std::u16string& bases) {
    Syllable s;
    if (!ParseSyllable(bases, /*strict=*/false, s)) return 0;
    return static_cast<size_t>(s.nucleusStart);
}

size_t CommonPrefix(const std::u16string& a, const std::u16string& b) {
    const size_t n = std::min(a.size(), b.size());
    size_t i = 0;
    while (i < n && a[i] == b[i]) ++i;
    return i;
}

}  // namespace

namespace {

constexpr size_t kMaxHistory = 128;

// The Telex keys that produce a letter, used to rebuild a word we are about to
// carry on editing.
std::u16string KeysForBase(char16_t base, bool upper) {
    std::u16string keys;
    switch (base) {
        case kABreve: keys = u"aw"; break;
        case kACirc:  keys = u"aa"; break;
        case kECirc:  keys = u"ee"; break;
        case kOCirc:  keys = u"oo"; break;
        case kOHorn:  keys = u"ow"; break;
        case kUHorn:  keys = u"uw"; break;
        case kDLower: keys = u"dd"; break;
        default:      keys.push_back(base); break;
    }
    if (upper && !keys.empty()) {
        keys[0] = static_cast<char16_t>(keys[0] - u'a' + u'A');
    }
    return keys;
}

}  // namespace

void Engine::Reset() {
    letters_.clear();
    raw_.clear();
    history_.clear();
    dead_ = false;
    edited_ = false;
    lastTransform_ = 0;
}

void Engine::EndWord(char16_t ch) {
    history_ += Display();
    history_.push_back(ch != 0 && !IsLetter(ch) ? ch : u' ');
    if (history_.size() > kMaxHistory) {
        history_.erase(0, history_.size() - kMaxHistory);
    }
    letters_.clear();
    raw_.clear();
    dead_ = false;
    edited_ = false;
    lastTransform_ = 0;
}

// Backspace has reached text we typed earlier: adopt the word it ends with, so
// the user can go on marking it up ("vay" + a is vây, even after a space and a
// few other words were typed in between).
void Engine::TakeWordFromHistory() {
    size_t start = history_.size();
    while (start > 0 && IsLetter(history_[start - 1])) --start;

    for (size_t i = start; i < history_.size(); ++i) {
        char16_t base = 0;
        uint8_t tone = kNoTone;
        bool upper = false;
        if (!DecomposeLetter(history_[i], base, tone, upper)) continue;
        Letter letter;
        letter.base = base;
        letter.tone = tone;
        letter.upper = upper;
        letter.src = KeysForBase(base, upper);
        letters_.push_back(letter);
    }
    history_.erase(start);
    RebuildRaw();
}

std::u16string Engine::Display() const {
    std::u16string out;
    out.reserve(letters_.size());
    for (const Letter& l : letters_) out.push_back(ComposeLetter(l.base, l.tone, l.upper));
    return out;
}

std::u16string Engine::Bases() const {
    std::u16string out;
    out.reserve(letters_.size());
    for (const Letter& l : letters_) out.push_back(l.base);
    return out;
}

uint8_t Engine::CurrentTone() const {
    for (const Letter& l : letters_) {
        if (l.tone != kNoTone) return l.tone;
    }
    return kNoTone;
}

void Engine::ClearTones() {
    for (Letter& l : letters_) l.tone = kNoTone;
}

bool Engine::HasVowel() const {
    for (const Letter& l : letters_) {
        if (IsVowelBase(l.base)) return true;
    }
    return false;
}

// The tone mark can move as the syllable grows: "hoà" becomes "hoàn", but
// "hòa" stays "hòa". Recompute the position after every change.
void Engine::RepositionTone() {
    const uint8_t tone = CurrentTone();
    if (tone == kNoTone) return;
    const std::u16string bases = Bases();
    Syllable s;
    if (!ParseSyllable(bases, false, s)) return;
    const int pos = TonePosition(bases, s);
    if (pos < 0) return;
    ClearTones();
    letters_[static_cast<size_t>(pos)].tone = tone;
}

void Engine::RebuildRaw() {
    raw_.clear();
    for (const Letter& l : letters_) raw_ += l.src;
}

void Engine::PushPlain(char16_t ch) {
    Letter l;
    l.base = ToLowerAscii(ch);
    l.tone = kNoTone;
    l.upper = IsUpperAscii(ch);
    l.src.push_back(ch);
    letters_.push_back(l);
}

// Undo a transform: put back the raw keys that produced this letter.
void Engine::ExpandLetter(size_t index) {
    const std::u16string src = letters_[index].src;
    const uint8_t tone = letters_[index].tone;
    std::vector<Letter> replacement;
    replacement.reserve(src.size());
    for (char16_t c : src) {
        Letter l;
        l.base = ToLowerAscii(c);
        l.upper = IsUpperAscii(c);
        l.src.push_back(c);
        replacement.push_back(l);
    }
    if (!replacement.empty()) replacement.front().tone = tone;
    letters_.erase(letters_.begin() + static_cast<long>(index));
    letters_.insert(letters_.begin() + static_cast<long>(index),
                    replacement.begin(), replacement.end());
}

// A word that has been transformed but no longer looks like Vietnamese goes
// back to exactly what was typed, and stops being processed (TELEX.md 6).
void Engine::MaybeRevert() {
    // Once the user has deleted something, what is on screen is text they have
    // seen and kept. Rewriting it - turning a đ they already accepted back into
    // "dd" - is far more confusing than leaving an odd-looking word alone.
    if (edited_) return;
    if (Display() == raw_) return;
    if (IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) return;

    const std::u16string raw = raw_;
    letters_.clear();
    for (char16_t c : raw) PushPlain(c);
    raw_ = raw;
    dead_ = true;
}

Result Engine::Diff(const std::u16string& before, char16_t typed) const {
    const std::u16string after = Display();
    if (after.size() == before.size() + 1 &&
        after.compare(0, before.size(), before) == 0 && after.back() == typed) {
        return Result{Action::PassThrough, 0, {}};
    }
    const size_t common = CommonPrefix(before, after);
    Result r;
    r.action = Action::Replace;
    r.backspaces = static_cast<int>(before.size() - common);
    r.insert = after.substr(common);
    return r;
}

Result Engine::OnKey(char16_t ch) {
    if (!IsAsciiLetter(ch)) {
        EndWord(ch);
        return Result{Action::PassThrough, 0, {}};
    }

    const std::u16string before = Display();

    if (dead_) {
        PushPlain(ch);
        raw_.push_back(ch);
        return Diff(before, ch);
    }

    const char16_t low = ToLowerAscii(ch);
    const char16_t previousTransform = lastTransform_;
    lastTransform_ = 0;

    bool handled = false;
    if (IsToneKey(low)) {
        handled = TryTone(ch, previousTransform);
    } else if (low == u'd') {
        handled = TryDd(ch, previousTransform);
    } else if (low == u'w') {
        handled = TryW(ch, previousTransform);
    } else if (low == u'a' || low == u'e' || low == u'o') {
        handled = TryCircumflex(ch, previousTransform);
    }

    if (!handled) {
        PushPlain(ch);
        raw_.push_back(ch);
        RepositionTone();
        MaybeRevert();
    }
    return Diff(before, ch);
}

Result Engine::OnBackspace() {
    if (letters_.empty()) {
        if (history_.empty()) return Result{Action::PassThrough, 0, {}};
        history_.pop_back();  // the character the application is deleting
        TakeWordFromHistory();
        dead_ = false;
        edited_ = true;
        lastTransform_ = 0;
        return Result{Action::PassThrough, 0, {}};
    }
    letters_.pop_back();
    RebuildRaw();
    edited_ = true;
    lastTransform_ = 0;
    // Deliberately no tone repositioning here: the character the user deleted is
    // gone from the screen already and we never rewrite what is left (TELEX.md 8).
    return Result{Action::PassThrough, 0, {}};
}

bool Engine::TryTone(char16_t ch, char16_t previousTransform) {
    const char16_t low = ToLowerAscii(ch);
    const uint8_t tone = ToneOfKey(low);
    const std::u16string bases = Bases();
    if (bases.empty()) return false;

    Syllable s;
    if (!ParseSyllable(bases, /*strict=*/false, s)) return false;
    const int pos = TonePosition(bases, s);
    if (pos < 0) return false;

    const uint8_t current = CurrentTone();
    if (low == u'z') {
        if (current == kNoTone) return false;  // nothing to remove, type a literal z
        ClearTones();
        RebuildRaw();
        return true;
    }
    if (current == tone) {
        if (previousTransform == low) {
            // Retyped straight away: undo, and put the tone key back as a plain
            // letter, which is what the user originally typed ("ass" -> "as").
            ClearTones();
            PushPlain(ch);
            RebuildRaw();
            return true;
        }
        // The mark is already there and this key is not an undo - most likely the
        // user deleted a letter and is putting the mark back. Swallow the key and
        // leave the word alone; pressing it once more does undo it.
        lastTransform_ = low;
        return true;
    }
    if (!ToneAllowed(bases, s, tone)) return false;

    ClearTones();
    letters_[static_cast<size_t>(pos)].tone = tone;
    raw_.push_back(ch);
    lastTransform_ = low;
    return true;
}

bool Engine::TryDd(char16_t ch, char16_t previousTransform) {
    if (letters_.empty()) return false;
    Letter& last = letters_.back();
    if (last.base == u'd' && last.tone == kNoTone) {
        const std::vector<Letter> saved = letters_;
        last.base = kDLower;
        last.upper = last.upper || IsUpperAscii(ch);
        last.src.push_back(ch);
        if (!IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) {
            letters_ = saved;  // "ađ" is not a syllable; that d is just a d
            return false;
        }
        raw_.push_back(ch);
        lastTransform_ = u'd';
        return true;
    }
    if (last.base == kDLower && previousTransform == u'd') {
        ExpandLetter(letters_.size() - 1);
        RebuildRaw();
        RepositionTone();
        return true;
    }

    // Free position, like the other transform keys: "dandf" is đàn. đ is always
    // the onset, so the first letter is the only candidate.
    if (letters_[0].base == u'd' && letters_[0].tone == kNoTone) {
        const std::vector<Letter> saved = letters_;
        letters_[0].base = kDLower;
        letters_[0].upper = letters_[0].upper || IsUpperAscii(ch);
        letters_[0].src.push_back(ch);
        if (!IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) {
            letters_ = saved;
            return false;
        }
        raw_.push_back(ch);
        lastTransform_ = u'd';
        return true;
    }
    return false;
}

bool Engine::TryCircumflex(char16_t ch, char16_t previousTransform) {
    const char16_t low = ToLowerAscii(ch);
    const char16_t circumflex = (low == u'a') ? kACirc : (low == u'e') ? kECirc : kOCirc;
    const size_t firstVowel = NucleusStart(Bases());

    // Free position: people type the doubled key after the whole word, as in
    // "vanas" for vấn. Cases like "ngoeor" (ngoẻo), where reaching back would
    // produce the impossible nucleus "ôe", are stopped by the validity check.
    for (size_t i = letters_.size(); i-- > firstVowel;) {
        if (letters_[i].base == low) {
            const std::vector<Letter> saved = letters_;
            letters_[i].base = circumflex;
            letters_[i].src.push_back(ch);
            RepositionTone();
            if (!IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) {
                letters_ = saved;
                RepositionTone();
                continue;  // an earlier vowel may still be the right target
            }
            raw_.push_back(ch);
            lastTransform_ = low;
            return true;
        }
        if (letters_[i].base == circumflex && previousTransform == low) {
            ExpandLetter(i);
            RebuildRaw();
            RepositionTone();
            return true;
        }
    }
    return false;
}

bool Engine::TryW(char16_t ch, char16_t previousTransform) {
    const size_t firstVowel = NucleusStart(Bases());

    // The "uo" cluster is handled first: in "duongw" the w turns both letters
    // into "ươ" at once.
    for (size_t i = letters_.size(); i-- > firstVowel + 1;) {
        const char16_t first = letters_[i - 1].base;
        const char16_t second = letters_[i].base;
        const bool firstIsU = (first == u'u' || first == kUHorn);
        const bool secondIsO = (second == u'o' || second == kOHorn);
        if (!firstIsU || !secondIsO) continue;

        if (first == kUHorn && second == kOHorn) {  // already ươ
            if (previousTransform != u'w') break;   // not an undo, leave it alone
            ExpandLetter(i);
            ExpandLetter(i - 1);
            RebuildRaw();
            RepositionTone();
            return true;
        }
        const std::vector<Letter> saved = letters_;
        letters_[i - 1].base = kUHorn;
        letters_[i].base = kOHorn;
        letters_[i].src.push_back(ch);
        RepositionTone();
        if (IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) {
            raw_.push_back(ch);
            lastTransform_ = u'w';
            return true;
        }
        letters_ = saved;
        break;
    }

    for (size_t i = letters_.size(); i-- > firstVowel;) {
        const char16_t base = letters_[i].base;
        char16_t horned = 0;
        if (base == u'a') horned = kABreve;
        else if (base == u'o') horned = kOHorn;
        else if (base == u'u') horned = kUHorn;

        if (horned != 0) {
            const std::vector<Letter> saved = letters_;
            letters_[i].base = horned;
            letters_[i].src.push_back(ch);
            RepositionTone();
            if (!IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) {
                letters_ = saved;
                RepositionTone();
                continue;  // an earlier vowel may still be the right target: in
                           // "mua" the a cannot take a horn but the u can
            }
            raw_.push_back(ch);
            lastTransform_ = u'w';
            return true;
        }
        if (base == kABreve || base == kOHorn || base == kUHorn) {
            if (previousTransform != u'w') continue;
            ExpandLetter(i);
            RebuildRaw();
            RepositionTone();
            return true;
        }
    }

    // A lone w with no vowel to attach to becomes ư ("w" -> ư, "tw" -> tư).
    if (!HasVowel()) {
        Letter l;
        l.base = kUHorn;
        l.upper = IsUpperAscii(ch);
        l.src.push_back(ch);
        letters_.push_back(l);
        if (!IsValidSyllable(Bases(), CurrentTone(), /*strict=*/false)) {
            letters_.pop_back();
            return false;
        }
        raw_.push_back(ch);
        lastTransform_ = u'w';
        return true;
    }
    return false;
}

}  // namespace telex
