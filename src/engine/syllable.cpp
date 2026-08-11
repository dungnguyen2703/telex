#include "syllable.h"

#include <algorithm>
#include <vector>

#include "tables.h"

namespace telex {
namespace {

const char16_t* const kOnsets[] = {
    u"ngh",
    u"ch", u"gh", u"gi", u"kh", u"ng", u"nh", u"ph", u"qu", u"th", u"tr",
    u"b", u"c", u"d", u"đ", u"g", u"h", u"k", u"l", u"m", u"n", u"p",
    u"r", u"s", u"t", u"v", u"x",
};

const char16_t* const kCodas[] = {
    u"ch", u"ng", u"nh", u"c", u"m", u"n", u"p", u"t",
};

// Every nucleus the language allows. Order is irrelevant.
const char16_t* const kNuclei[] = {
    // single
    u"a", u"ă", u"â", u"e", u"ê", u"i", u"o", u"ô",
    u"ơ", u"u", u"ư", u"y",
    // double
    u"ai", u"ao", u"au", u"ay", u"âu", u"ây", u"eo", u"êu",
    u"ia", u"iê", u"iu", u"oa", u"oă", u"oe", u"oi", u"ôi",
    u"ơi", u"ua", u"uâ", u"uê", u"ui", u"uô", u"uơ",
    u"uy", u"ưa", u"ưi", u"ươ", u"ưu", u"yê",
    // triple
    u"iêu", u"yêu", u"oai", u"oao", u"oay", u"oeo", u"uây",
    u"uôi", u"uya", u"uyê", u"uyu", u"ươi", u"ươu",
};

char16_t StripDiacritic(char16_t c) {
    switch (c) {
        case 0x0103:  // ă
        case 0x00E2:  // â
            return u'a';
        case 0x00EA:  // ê
            return u'e';
        case 0x00F4:  // ô
        case 0x01A1:  // ơ
            return u'o';
        case 0x01B0:  // ư
            return u'u';
        default:
            return c;
    }
}

// True when a letter already typed could still turn into `target`. A plain a/e/o/u
// may yet receive its diacritic, but a letter that already has one is final:
// "ie" is on its way to "iê", while "ôe" is on its way to nothing at all.
bool CanBecome(char16_t typed, char16_t target) {
    if (typed == target) return true;
    const bool typedIsPlain = (StripDiacritic(typed) == typed);
    return typedIsPlain && StripDiacritic(target) == typed;
}

bool IsPrefix(const std::u16string& prefix, const std::u16string& full) {
    return prefix.size() <= full.size() &&
           std::equal(prefix.begin(), prefix.end(), full.begin());
}

bool OnsetOk(const std::u16string& onset, bool strict) {
    if (onset.empty()) return true;
    for (const char16_t* o : kOnsets) {
        const std::u16string cand(o);
        if (cand == onset) return true;
        if (!strict && IsPrefix(onset, cand)) return true;  // e.g. "q" on the way to "qu"
    }
    return false;
}

bool CodaOk(const std::u16string& coda) {
    if (coda.empty()) return true;
    for (const char16_t* c : kCodas) {
        if (std::u16string(c) == coda) return true;
    }
    return false;
}

bool NucleusOk(const std::u16string& nucleus, bool strict) {
    if (nucleus.empty()) return !strict;
    if (strict) {
        for (const char16_t* n : kNuclei) {
            if (std::u16string(n) == nucleus) return true;
        }
        return false;
    }
    // While typing, diacritics arrive after the letters they belong to, so accept
    // anything that is still on its way to a real nucleus.
    for (const char16_t* n : kNuclei) {
        const std::u16string full(n);
        if (nucleus.size() > full.size()) continue;
        bool ok = true;
        for (size_t i = 0; i < nucleus.size() && ok; ++i) {
            ok = CanBecome(nucleus[i], full[i]);
        }
        if (ok) return true;
    }
    return false;
}

}  // namespace

bool ParseSyllable(const std::u16string& bases, bool strict, Syllable& out) {
    const int size = static_cast<int>(bases.size());
    if (size == 0) return false;

    // Longest onset first, backtracking to shorter ones ("gia" is gi+a, but
    // "gao" has to fall back from "gh"-style matches to plain "g").
    for (int onsetLen = std::min(3, size); onsetLen >= 0; --onsetLen) {
        const std::u16string onset = bases.substr(0, onsetLen);
        if (!OnsetOk(onset, strict)) continue;

        int i = onsetLen;
        while (i < size && IsVowelBase(bases[i])) ++i;
        const std::u16string nucleus = bases.substr(onsetLen, i - onsetLen);
        const std::u16string coda = bases.substr(i);

        if (!NucleusOk(nucleus, strict)) continue;
        if (!CodaOk(coda)) continue;
        if (nucleus.empty() && !coda.empty()) continue;

        out.onsetLen = onsetLen;
        out.nucleusStart = onsetLen;
        out.nucleusLen = static_cast<int>(nucleus.size());
        out.codaStart = i;
        out.codaLen = static_cast<int>(coda.size());
        return true;
    }
    return false;
}

int TonePosition(const std::u16string& bases, const Syllable& s) {
    // Rule 1: in "qu" and "gi" the u/i is part of the onset. ParseSyllable has
    // already excluded it from the nucleus; the only thing left to handle is a
    // syllable with no other vowel, like "gì" or "quỳ".
    if (s.nucleusLen == 0) {
        if (s.onsetLen == 2) {
            const std::u16string onset = bases.substr(0, 2);
            if (onset == u"qu" || onset == u"gi") return s.onsetLen - 1;
        }
        return -1;
    }

    // Rule 2: ê and ơ always win.
    for (int i = 0; i < s.nucleusLen; ++i) {
        const char16_t c = bases[s.nucleusStart + i];
        if (c == 0x00EA || c == 0x01A1) return s.nucleusStart + i;
    }

    // Rule 3: a single vowel takes the tone.
    if (s.nucleusLen == 1) return s.nucleusStart;

    // Rule 4: with a coda, the tone goes on the last vowel.
    if (s.codaLen > 0) return s.nucleusStart + s.nucleusLen - 1;

    // Rule 5: open syllable, classic placement - second to last vowel.
    return s.nucleusStart + s.nucleusLen - 2;
}

bool ToneAllowed(const std::u16string& bases, const Syllable& s, uint8_t tone) {
    if (tone == kNoTone || tone == kAcute || tone == kDot) return true;
    if (s.codaLen == 0) return true;
    const std::u16string coda = bases.substr(s.codaStart, s.codaLen);
    return !(coda == u"c" || coda == u"ch" || coda == u"p" || coda == u"t");
}

bool IsValidSyllable(const std::u16string& bases, uint8_t tone, bool strict) {
    Syllable s;
    if (!ParseSyllable(bases, strict, s)) return false;
    if (tone != kNoTone && TonePosition(bases, s) < 0) return false;
    return ToneAllowed(bases, s, tone);
}

}  // namespace telex
