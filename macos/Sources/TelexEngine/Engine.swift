// The Telex input state machine. Pure Swift: no AppKit, no I/O, no globals.
// Rules are specified in docs/TELEX.md.

public enum Action: Sendable {
    case passThrough  // let the key type itself, engine state is already in sync
    case replace      // swallow the key, then apply `backspaces` and `insert`
}

public struct Result: Sendable {
    public let action: Action
    public let backspaces: Int
    public let insert: String

    static let pass = Result(action: .passThrough, backspaces: 0, insert: "")
}

/// One rendered character of the word being typed.
public struct Letter: Sendable {
    public var base: Character          // toneless lowercase base ('a', 'ă', 'đ', 'b', ...)
    public var tone: Tone = .none       // only meaningful on vowels
    public var upper = false
    public var src: [Character] = []    // the raw keys that produced this letter
}

private let aBreve: Character = "\u{0103}"  // ă
private let aCirc: Character = "\u{00E2}"   // â
private let eCirc: Character = "\u{00EA}"   // ê
private let oCirc: Character = "\u{00F4}"   // ô
private let oHorn: Character = "\u{01A1}"   // ơ
private let uHorn: Character = "\u{01B0}"   // ư

private let maxHistory = 128

private func isToneKey(_ low: Character) -> Bool {
    low == "s" || low == "f" || low == "r" || low == "x" || low == "j" || low == "z"
}

private func toneOfKey(_ low: Character) -> Tone {
    switch low {
    case "s": return .acute
    case "f": return .grave
    case "r": return .hook
    case "x": return .tilde
    case "j": return .dot
    default: return .none  // 'z' removes the tone
    }
}

/// The Telex keys that produce a letter, used to rebuild a word we are about to
/// carry on editing.
private func keysForBase(_ base: Character, upper: Bool) -> [Character] {
    var keys: [Character]
    switch base {
    case aBreve: keys = ["a", "w"]
    case aCirc: keys = ["a", "a"]
    case eCirc: keys = ["e", "e"]
    case oCirc: keys = ["o", "o"]
    case oHorn: keys = ["o", "w"]
    case uHorn: keys = ["u", "w"]
    case dLower: keys = ["d", "d"]
    default: keys = [base]
    }
    if upper, !keys.isEmpty { keys[0] = toUpperAscii(keys[0]) }
    return keys
}

public final class Engine {
    private var letters: [Letter] = []
    private var raw: [Character] = []
    // Text we have already put on screen before the current word, kept only so
    // that backspacing back into it can rebuild the word being edited. Dropped
    // the moment we can no longer be sure what is in front of the caret.
    private var history: [Character] = []
    private var dead = false     // word reverted to literal text; pass everything through
    private var edited = false   // backspace was used; never rewrite the whole word again
    // The key that applied the transform currently on screen, or nil. Retyping a
    // key only undoes it when it comes straight after, so that adding a mark
    // back after a backspace is not mistaken for removing it.
    private var lastTransform: Character?

    public init() {}

    // MARK: - Public surface

    /// What the word being typed currently looks like on screen.
    public var display: String { String(displayChars()) }

    /// The literal keys the user pressed for this word.
    public var rawKeys: String { String(raw) }

    /// Forgets everything, including the text before the caret. For anything
    /// that moves the caret somewhere we cannot follow: clicks, arrow keys, a
    /// new window (docs/TELEX.md 7).
    public func reset() {
        letters.removeAll()
        raw.removeAll()
        history.removeAll()
        dead = false
        edited = false
        lastTransform = nil
    }

    /// Ends the current word but remembers the text, so that backspacing back
    /// into it can pick up where the user left off. `ch` is the character that
    /// ended it, or nil when the platform layer does not know which one it was.
    public func endWord(_ ch: Character?) {
        history.append(contentsOf: displayChars())
        if let ch, !isLetter(ch) {
            history.append(ch)
        } else {
            history.append(" ")
        }
        if history.count > maxHistory {
            history.removeFirst(history.count - maxHistory)
        }
        letters.removeAll()
        raw.removeAll()
        dead = false
        edited = false
        lastTransform = nil
    }

    /// `ch` is the character the key would have produced (case already applied).
    /// Non-letters end the current word and always pass through.
    public func onKey(_ ch: Character) -> Result {
        if !isAsciiLetter(ch) {
            endWord(ch)
            return .pass
        }

        let before = displayChars()

        if dead {
            pushPlain(ch)
            raw.append(ch)
            return diff(before: before, typed: ch)
        }

        let low = toLowerAscii(ch)
        let previousTransform = lastTransform
        lastTransform = nil

        var handled = false
        if isToneKey(low) {
            handled = tryTone(ch, previousTransform)
        } else if low == "d" {
            handled = tryDd(ch, previousTransform)
        } else if low == "w" {
            handled = tryW(ch, previousTransform)
        } else if low == "a" || low == "e" || low == "o" {
            handled = tryCircumflex(ch, previousTransform)
        }

        if !handled {
            pushPlain(ch)
            raw.append(ch)
            repositionTone()
            maybeRevert()
        }
        return diff(before: before, typed: ch)
    }

    public func onBackspace() -> Result {
        if letters.isEmpty {
            if history.isEmpty { return .pass }
            history.removeLast()  // the character the application is deleting
            takeWordFromHistory()
            dead = false
            edited = true
            lastTransform = nil
            return .pass
        }
        letters.removeLast()
        rebuildRaw()
        edited = true
        lastTransform = nil
        // Deliberately no tone repositioning here: the character the user
        // deleted is gone from the screen already and we never rewrite what is
        // left (TELEX.md 8).
        return .pass
    }

    // MARK: - Internals

    private func displayChars() -> [Character] {
        letters.map { composeLetter($0.base, $0.tone, upper: $0.upper) }
    }

    private func bases() -> [Character] {
        letters.map(\.base)
    }

    private func currentTone() -> Tone {
        for l in letters where l.tone != .none { return l.tone }
        return .none
    }

    private func clearTones() {
        for i in letters.indices { letters[i].tone = .none }
    }

    private func hasVowel() -> Bool {
        letters.contains { isVowelBase($0.base) }
    }

    /// Letters before the nucleus belong to the onset and must not be
    /// transformed: the u of "qu" is not a vowel we may turn into ư
    /// ("quowr" is quở, not quưở).
    private func nucleusStart() -> Int {
        guard let s = parseSyllable(bases(), strict: false) else { return 0 }
        return s.nucleusStart
    }

    // Backspace has reached text we typed earlier: adopt the word it ends with,
    // so the user can go on marking it up ("vay" + a is vây, even after a space
    // and a few other words were typed in between).
    private func takeWordFromHistory() {
        var start = history.count
        while start > 0, isLetter(history[start - 1]) { start -= 1 }

        for i in start..<history.count {
            guard let parts = decomposeLetter(history[i]) else { continue }
            letters.append(Letter(base: parts.base,
                                  tone: parts.tone,
                                  upper: parts.upper,
                                  src: keysForBase(parts.base, upper: parts.upper)))
        }
        history.removeSubrange(start...)
        rebuildRaw()
    }

    // The tone mark can move as the syllable grows: "hoà" becomes "hoàn", but
    // "hòa" stays "hòa". Recompute the position after every change.
    private func repositionTone() {
        let tone = currentTone()
        if tone == .none { return }
        let b = bases()
        guard let s = parseSyllable(b, strict: false) else { return }
        let pos = tonePosition(b, s)
        if pos < 0 { return }
        clearTones()
        letters[pos].tone = tone
    }

    private func rebuildRaw() {
        raw = letters.flatMap(\.src)
    }

    private func pushPlain(_ ch: Character) {
        letters.append(Letter(base: toLowerAscii(ch),
                              tone: .none,
                              upper: isUpperAscii(ch),
                              src: [ch]))
    }

    /// Undo a transform: put back the raw keys that produced this letter.
    private func expandLetter(_ index: Int) {
        let src = letters[index].src
        let tone = letters[index].tone
        var replacement: [Letter] = src.map {
            Letter(base: toLowerAscii($0), tone: .none, upper: isUpperAscii($0), src: [$0])
        }
        if !replacement.isEmpty { replacement[0].tone = tone }
        letters.replaceSubrange(index...index, with: replacement)
    }

    // A word that has been transformed but no longer looks like Vietnamese goes
    // back to exactly what was typed, and stops being processed (TELEX.md 6).
    private func maybeRevert() {
        // Once the user has deleted something, what is on screen is text they
        // have seen and kept. Rewriting it - turning a đ they already accepted
        // back into "dd" - is far more confusing than leaving an odd-looking
        // word alone.
        if edited { return }
        if displayChars() == raw { return }
        if isValidSyllable(bases(), tone: currentTone(), strict: false) { return }

        let saved = raw
        letters.removeAll()
        for c in saved { pushPlain(c) }
        raw = saved
        dead = true
    }

    private func diff(before: [Character], typed: Character) -> Result {
        let after = displayChars()
        if after.count == before.count + 1,
           after.last == typed,
           Array(after[0..<before.count]) == before {
            return .pass
        }
        var common = 0
        let n = min(before.count, after.count)
        while common < n, before[common] == after[common] { common += 1 }
        return Result(action: .replace,
                      backspaces: before.count - common,
                      insert: String(after[common...]))
    }

    // MARK: - Transform keys

    private func tryTone(_ ch: Character, _ previousTransform: Character?) -> Bool {
        let low = toLowerAscii(ch)
        let tone = toneOfKey(low)
        let b = bases()
        if b.isEmpty { return false }

        guard let s = parseSyllable(b, strict: false) else { return false }
        let pos = tonePosition(b, s)
        if pos < 0 { return false }

        let current = currentTone()
        if low == "z" {
            if current == .none { return false }  // nothing to remove, type a literal z
            clearTones()
            rebuildRaw()
            return true
        }
        if current == tone {
            if previousTransform == low {
                // Retyped straight away: undo, and put the tone key back as a
                // plain letter, which is what the user originally typed
                // ("ass" -> "as").
                clearTones()
                pushPlain(ch)
                rebuildRaw()
                return true
            }
            // The mark is already there and this key is not an undo - most
            // likely the user deleted a letter and is putting the mark back.
            // Swallow the key and leave the word alone; pressing it once more
            // does undo it.
            lastTransform = low
            return true
        }
        if !toneAllowed(b, s, tone) { return false }

        clearTones()
        letters[pos].tone = tone
        raw.append(ch)
        lastTransform = low
        return true
    }

    private func tryDd(_ ch: Character, _ previousTransform: Character?) -> Bool {
        if letters.isEmpty { return false }
        let last = letters.count - 1
        if letters[last].base == "d", letters[last].tone == .none {
            let saved = letters
            letters[last].base = dLower
            letters[last].upper = letters[last].upper || isUpperAscii(ch)
            letters[last].src.append(ch)
            if !isValidSyllable(bases(), tone: currentTone(), strict: false) {
                letters = saved  // "ađ" is not a syllable; that d is just a d
                return false
            }
            raw.append(ch)
            lastTransform = "d"
            return true
        }
        if letters[last].base == dLower, previousTransform == "d" {
            expandLetter(last)
            rebuildRaw()
            repositionTone()
            return true
        }

        // Free position, like the other transform keys: "dandf" is đàn. đ is
        // always the onset, so the first letter is the only candidate.
        if letters[0].base == "d", letters[0].tone == .none {
            let saved = letters
            letters[0].base = dLower
            letters[0].upper = letters[0].upper || isUpperAscii(ch)
            letters[0].src.append(ch)
            if !isValidSyllable(bases(), tone: currentTone(), strict: false) {
                letters = saved
                return false
            }
            raw.append(ch)
            lastTransform = "d"
            return true
        }
        return false
    }

    private func tryCircumflex(_ ch: Character, _ previousTransform: Character?) -> Bool {
        let low = toLowerAscii(ch)
        let circumflex: Character = (low == "a") ? aCirc : (low == "e") ? eCirc : oCirc
        let firstVowel = nucleusStart()

        // Free position: people type the doubled key after the whole word, as in
        // "vanas" for vấn. Cases like "ngoeor" (ngoẻo), where reaching back
        // would produce the impossible nucleus "ôe", are stopped by the check.
        var i = letters.count - 1
        while i >= firstVowel {
            defer { i -= 1 }
            if letters[i].base == low {
                let saved = letters
                letters[i].base = circumflex
                letters[i].src.append(ch)
                repositionTone()
                if !isValidSyllable(bases(), tone: currentTone(), strict: false) {
                    letters = saved
                    repositionTone()
                    continue  // an earlier vowel may still be the right target
                }
                raw.append(ch)
                lastTransform = low
                return true
            }
            if letters[i].base == circumflex, previousTransform == low {
                expandLetter(i)
                rebuildRaw()
                repositionTone()
                return true
            }
        }
        return false
    }

    private func tryW(_ ch: Character, _ previousTransform: Character?) -> Bool {
        let firstVowel = nucleusStart()

        // The "uo" cluster is handled first: in "duongw" the w turns both
        // letters into "ươ" at once.
        var i = letters.count - 1
        while i >= firstVowel + 1 {
            let first = letters[i - 1].base
            let second = letters[i].base
            let firstIsU = (first == "u" || first == uHorn)
            let secondIsO = (second == "o" || second == oHorn)
            if !firstIsU || !secondIsO { i -= 1; continue }

            if first == uHorn, second == oHorn {   // already ươ
                if previousTransform != "w" { break }  // not an undo, leave it alone
                expandLetter(i)
                expandLetter(i - 1)
                rebuildRaw()
                repositionTone()
                return true
            }
            let saved = letters
            letters[i - 1].base = uHorn
            letters[i].base = oHorn
            letters[i].src.append(ch)
            repositionTone()
            if isValidSyllable(bases(), tone: currentTone(), strict: false) {
                raw.append(ch)
                lastTransform = "w"
                return true
            }
            letters = saved
            break
        }

        var j = letters.count - 1
        while j >= firstVowel {
            defer { j -= 1 }
            let base = letters[j].base
            var horned: Character?
            if base == "a" { horned = aBreve }
            else if base == "o" { horned = oHorn }
            else if base == "u" { horned = uHorn }

            if let horned {
                let saved = letters
                letters[j].base = horned
                letters[j].src.append(ch)
                repositionTone()
                if !isValidSyllable(bases(), tone: currentTone(), strict: false) {
                    letters = saved
                    repositionTone()
                    continue  // an earlier vowel may still be the right target:
                              // in "mua" the a cannot take a horn but the u can
                }
                raw.append(ch)
                lastTransform = "w"
                return true
            }
            if base == aBreve || base == oHorn || base == uHorn {
                if previousTransform != "w" { continue }
                expandLetter(j)
                rebuildRaw()
                repositionTone()
                return true
            }
        }

        // A lone w with no vowel to attach to becomes ư ("w" -> ư, "tw" -> tư).
        if !hasVowel() {
            letters.append(Letter(base: uHorn, tone: .none, upper: isUpperAscii(ch), src: [ch]))
            if !isValidSyllable(bases(), tone: currentTone(), strict: false) {
                letters.removeLast()
                return false
            }
            raw.append(ch)
            lastTransform = "w"
            return true
        }
        return false
    }
}
