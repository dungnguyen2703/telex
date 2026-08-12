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

    // Ends the current word but remembers the text, so that backspacing back
    // into it can pick up where the user left off. `ch` is the character that
    // ended it, or 0 when the platform layer does not know which one it was.
    void EndWord(char16_t ch);

    // Forgets everything, including the text before the caret. For anything that
    // moves the caret somewhere we cannot follow: clicks, arrow keys, a new
    // window (docs/TELEX.md 7).
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
    void TakeWordFromHistory();
    bool HasVowel() const;
    Result Diff(const std::u16string& before, char16_t typed) const;

    bool TryTone(char16_t ch, char16_t previousTransform);
    bool TryDd(char16_t ch, char16_t previousTransform);
    bool TryW(char16_t ch, char16_t previousTransform);
    bool TryCircumflex(char16_t ch, char16_t previousTransform);

    std::vector<Letter> letters_;
    std::u16string raw_;
    // Text we have already put on screen before the current word, kept only so
    // that backspacing back into it can rebuild the word being edited. Dropped
    // the moment we can no longer be sure what is in front of the caret.
    std::u16string history_;
    bool dead_ = false;    // word reverted to literal text; pass everything through
    bool edited_ = false;  // backspace was used; never rewrite the whole word again
    // The key that applied the transform currently on screen, or 0. Retyping a
    // key only undoes it when it comes straight after, so that adding a mark
    // back after a backspace is not mistaken for removing it.
    char16_t lastTransform_ = 0;
};

}  // namespace telex
