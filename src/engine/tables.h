// Vietnamese character tables. Pure data, no Win32, no I/O.
#pragma once

#include <cstdint>
#include <string>

namespace telex {

// Tone marks. The numeric values are used as table indices.
enum Tone : uint8_t {
    kNoTone = 0,
    kAcute  = 1,  // s - sac
    kGrave  = 2,  // f - huyen
    kHook   = 3,  // r - hoi
    kTilde  = 4,  // x - nga
    kDot    = 5,  // j - nang
    kToneCount = 6,
};

// Base vowels, in table order: a ă â e ê i o ô ơ u ư y
constexpr int kVowelCount = 12;

constexpr char16_t kDLower = 0x0111;  // d with stroke
constexpr char16_t kDUpper = 0x0110;

// Returns the index into the vowel tables for a toneless lowercase base vowel,
// or -1 if the character is not one.
int VowelIndex(char16_t lowerBase);

// Builds the precomposed character for a (vowel, tone, case) triple.
char16_t ComposeVowel(int vowelIndex, uint8_t tone, bool upper);

// Splits any Vietnamese letter into its toneless lowercase base, tone and case.
// Returns false for characters outside the Vietnamese alphabet.
bool DecomposeLetter(char16_t c, char16_t& lowerBase, uint8_t& tone, bool& upper);

// Composes a letter from the parts produced by DecomposeLetter.
char16_t ComposeLetter(char16_t lowerBase, uint8_t tone, bool upper);

bool IsVowelBase(char16_t lowerBase);
bool IsLetter(char16_t c);

// ASCII-only case helpers; the engine never sees non-ASCII input keys.
char16_t ToLowerAscii(char16_t c);
bool IsUpperAscii(char16_t c);

}  // namespace telex
