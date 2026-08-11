// The Telex input state machine. Pure C++: no Win32, no I/O, no globals.
// Rules are specified in docs/TELEX.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace telex {

enum class Action {
    PassThrough,  // let the key type itself, engine state is already in sync
    Replace,      // swallow the key, then apply `backspaces` and `insert`
};

struct Result {
    Action action = Action::PassThrough;
    int backspaces = 0;
    std::u16string insert;
};

// One rendered character of the word being typed.
struct Letter {
    char16_t base = 0;        // toneless lowercase base ('a', 'ă', 'đ', 'b', ...)
    uint8_t tone = 0;         // Tone enum value, only meaningful on vowels
    bool upper = false;
    std::u16string src;       // the raw keys that produced this letter
};

class Engine {
public:
    // `ch` is the character the key would have produced (case already applied).
    // Non-letters end the current word and always pass through.
    Result OnKey(char16_t ch);
    Result OnBackspace();
    void Reset();

    // What the word being typed currently looks like on screen.
    std::u16string Display() const;
    // The literal keys the user pressed for this word.
    const std::u16string& Raw() const { return raw_; }

private:
    std::u16string Bases() const;
    uint8_t CurrentTone() const;
    void ClearTones();
    void RepositionTone();
    void RebuildRaw();
    void PushPlain(char16_t ch);
    void ExpandLetter(size_t index);
    void MaybeRevert();
    bool HasVowel() const;
    Result Diff(const std::u16string& before, char16_t typed) const;

    bool TryTone(char16_t ch);
    bool TryDd(char16_t ch);
    bool TryW(char16_t ch);
    bool TryCircumflex(char16_t ch);

    std::vector<Letter> letters_;
    std::u16string raw_;
    bool dead_ = false;  // word reverted to literal text; pass everything through
};

}  // namespace telex
