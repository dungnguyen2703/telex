#include "tables.h"

namespace telex {
namespace {

// [vowel][tone] -> precomposed code point. Rows: a ă â e ê i o ô ơ u ư y
const char16_t kLower[kVowelCount][kToneCount] = {
    {0x0061, 0x00E1, 0x00E0, 0x1EA3, 0x00E3, 0x1EA1},  // a
    {0x0103, 0x1EAF, 0x1EB1, 0x1EB3, 0x1EB5, 0x1EB7},  // ă
    {0x00E2, 0x1EA5, 0x1EA7, 0x1EA9, 0x1EAB, 0x1EAD},  // â
    {0x0065, 0x00E9, 0x00E8, 0x1EBB, 0x1EBD, 0x1EB9},  // e
    {0x00EA, 0x1EBF, 0x1EC1, 0x1EC3, 0x1EC5, 0x1EC7},  // ê
    {0x0069, 0x00ED, 0x00EC, 0x1EC9, 0x0129, 0x1ECB},  // i
    {0x006F, 0x00F3, 0x00F2, 0x1ECF, 0x00F5, 0x1ECD},  // o
    {0x00F4, 0x1ED1, 0x1ED3, 0x1ED5, 0x1ED7, 0x1ED9},  // ô
    {0x01A1, 0x1EDB, 0x1EDD, 0x1EDF, 0x1EE1, 0x1EE3},  // ơ
    {0x0075, 0x00FA, 0x00F9, 0x1EE7, 0x0169, 0x1EE5},  // u
    {0x01B0, 0x1EE9, 0x1EEB, 0x1EED, 0x1EEF, 0x1EF1},  // ư
    {0x0079, 0x00FD, 0x1EF3, 0x1EF7, 0x1EF9, 0x1EF5},  // y
};

const char16_t kUpper[kVowelCount][kToneCount] = {
    {0x0041, 0x00C1, 0x00C0, 0x1EA2, 0x00C3, 0x1EA0},  // A
    {0x0102, 0x1EAE, 0x1EB0, 0x1EB2, 0x1EB4, 0x1EB6},  // Ă
    {0x00C2, 0x1EA4, 0x1EA6, 0x1EA8, 0x1EAA, 0x1EAC},  // Â
    {0x0045, 0x00C9, 0x00C8, 0x1EBA, 0x1EBC, 0x1EB8},  // E
    {0x00CA, 0x1EBE, 0x1EC0, 0x1EC2, 0x1EC4, 0x1EC6},  // Ê
    {0x0049, 0x00CD, 0x00CC, 0x1EC8, 0x0128, 0x1ECA},  // I
    {0x004F, 0x00D3, 0x00D2, 0x1ECE, 0x00D5, 0x1ECC},  // O
    {0x00D4, 0x1ED0, 0x1ED2, 0x1ED4, 0x1ED6, 0x1ED8},  // Ô
    {0x01A0, 0x1EDA, 0x1EDC, 0x1EDE, 0x1EE0, 0x1EE2},  // Ơ
    {0x0055, 0x00DA, 0x00D9, 0x1EE6, 0x0168, 0x1EE4},  // U
    {0x01AF, 0x1EE8, 0x1EEA, 0x1EEC, 0x1EEE, 0x1EF0},  // Ư
    {0x0059, 0x00DD, 0x1EF2, 0x1EF6, 0x1EF8, 0x1EF4},  // Y
};

}  // namespace

int VowelIndex(char16_t lowerBase) {
    for (int i = 0; i < kVowelCount; ++i) {
        if (kLower[i][kNoTone] == lowerBase) return i;
    }
    return -1;
}

bool IsVowelBase(char16_t lowerBase) { return VowelIndex(lowerBase) >= 0; }

char16_t ComposeVowel(int vowelIndex, uint8_t tone, bool upper) {
    if (vowelIndex < 0 || vowelIndex >= kVowelCount || tone >= kToneCount) return 0;
    return upper ? kUpper[vowelIndex][tone] : kLower[vowelIndex][tone];
}

bool DecomposeLetter(char16_t c, char16_t& lowerBase, uint8_t& tone, bool& upper) {
    for (int v = 0; v < kVowelCount; ++v) {
        for (uint8_t t = 0; t < kToneCount; ++t) {
            if (kLower[v][t] == c) {
                lowerBase = kLower[v][kNoTone];
                tone = t;
                upper = false;
                return true;
            }
            if (kUpper[v][t] == c) {
                lowerBase = kLower[v][kNoTone];
                tone = t;
                upper = true;
                return true;
            }
        }
    }
    if (c == kDLower || c == kDUpper) {
        lowerBase = kDLower;
        tone = kNoTone;
        upper = (c == kDUpper);
        return true;
    }
    if (c >= u'a' && c <= u'z') {
        lowerBase = c;
        tone = kNoTone;
        upper = false;
        return true;
    }
    if (c >= u'A' && c <= u'Z') {
        lowerBase = static_cast<char16_t>(c - u'A' + u'a');
        tone = kNoTone;
        upper = true;
        return true;
    }
    return false;
}

char16_t ComposeLetter(char16_t lowerBase, uint8_t tone, bool upper) {
    const int v = VowelIndex(lowerBase);
    if (v >= 0) return ComposeVowel(v, tone, upper);
    if (lowerBase == kDLower) return upper ? kDUpper : kDLower;
    if (lowerBase >= u'a' && lowerBase <= u'z') {
        return upper ? static_cast<char16_t>(lowerBase - u'a' + u'A') : lowerBase;
    }
    return lowerBase;
}

bool IsLetter(char16_t c) {
    char16_t base;
    uint8_t tone;
    bool upper;
    return DecomposeLetter(c, base, tone, upper);
}

char16_t ToLowerAscii(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c - u'A' + u'a');
    return c;
}

bool IsUpperAscii(char16_t c) { return c >= u'A' && c <= u'Z'; }

}  // namespace telex
