// Vietnamese syllable structure: parsing, validity and tone placement.
// Operates on toneless lowercase base letters (see tables.h).
#pragma once

#include <cstdint>
#include <string>

namespace telex {

struct Syllable {
    int onsetLen = 0;
    int nucleusStart = 0;
    int nucleusLen = 0;
    int codaStart = 0;
    int codaLen = 0;
};

// Splits `bases` into onset / nucleus / coda.
// strict = true  : the word must be a complete, well-formed syllable.
// strict = false : the word only has to be a plausible prefix of one, which is
//                  what we need while the user is still typing (see TELEX.md 6).
bool ParseSyllable(const std::u16string& bases, bool strict, Syllable& out);

// Index of the letter that should carry the tone mark, or -1 if there is none.
int TonePosition(const std::u16string& bases, const Syllable& s);

// Codas c/ch/p/t only accept the acute and dot tones.
bool ToneAllowed(const std::u16string& bases, const Syllable& s, uint8_t tone);

// ParseSyllable + ToneAllowed in one call.
bool IsValidSyllable(const std::u16string& bases, uint8_t tone, bool strict);

}  // namespace telex
