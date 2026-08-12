// Vietnamese character tables. Pure data, no AppKit, no I/O.
//
// Code points are written as escapes rather than as literal characters so the
// table can be checked against docs/TELEX.md without trusting the editor to
// have saved them precomposed.

/// Tone marks. The raw values are used as table indices.
public enum Tone: UInt8, CaseIterable, Sendable {
    case none = 0
    case acute = 1   // s - sac
    case grave = 2   // f - huyen
    case hook = 3    // r - hoi
    case tilde = 4   // x - nga
    case dot = 5     // j - nang

    static let count = 6
}

/// Base vowels, in table order: a ă â e ê i o ô ơ u ư y
public let vowelCount = 12

public let dLower: Character = "\u{0111}"  // đ
public let dUpper: Character = "\u{0110}"  // Đ

// [vowel][tone] -> precomposed code point. Rows: a ă â e ê i o ô ơ u ư y
private let lowerTable: [[Character]] = [
    ["\u{0061}", "\u{00E1}", "\u{00E0}", "\u{1EA3}", "\u{00E3}", "\u{1EA1}"],  // a
    ["\u{0103}", "\u{1EAF}", "\u{1EB1}", "\u{1EB3}", "\u{1EB5}", "\u{1EB7}"],  // ă
    ["\u{00E2}", "\u{1EA5}", "\u{1EA7}", "\u{1EA9}", "\u{1EAB}", "\u{1EAD}"],  // â
    ["\u{0065}", "\u{00E9}", "\u{00E8}", "\u{1EBB}", "\u{1EBD}", "\u{1EB9}"],  // e
    ["\u{00EA}", "\u{1EBF}", "\u{1EC1}", "\u{1EC3}", "\u{1EC5}", "\u{1EC7}"],  // ê
    ["\u{0069}", "\u{00ED}", "\u{00EC}", "\u{1EC9}", "\u{0129}", "\u{1ECB}"],  // i
    ["\u{006F}", "\u{00F3}", "\u{00F2}", "\u{1ECF}", "\u{00F5}", "\u{1ECD}"],  // o
    ["\u{00F4}", "\u{1ED1}", "\u{1ED3}", "\u{1ED5}", "\u{1ED7}", "\u{1ED9}"],  // ô
    ["\u{01A1}", "\u{1EDB}", "\u{1EDD}", "\u{1EDF}", "\u{1EE1}", "\u{1EE3}"],  // ơ
    ["\u{0075}", "\u{00FA}", "\u{00F9}", "\u{1EE7}", "\u{0169}", "\u{1EE5}"],  // u
    ["\u{01B0}", "\u{1EE9}", "\u{1EEB}", "\u{1EED}", "\u{1EEF}", "\u{1EF1}"],  // ư
    ["\u{0079}", "\u{00FD}", "\u{1EF3}", "\u{1EF7}", "\u{1EF9}", "\u{1EF5}"],  // y
]

private let upperTable: [[Character]] = [
    ["\u{0041}", "\u{00C1}", "\u{00C0}", "\u{1EA2}", "\u{00C3}", "\u{1EA0}"],  // A
    ["\u{0102}", "\u{1EAE}", "\u{1EB0}", "\u{1EB2}", "\u{1EB4}", "\u{1EB6}"],  // Ă
    ["\u{00C2}", "\u{1EA4}", "\u{1EA6}", "\u{1EA8}", "\u{1EAA}", "\u{1EAC}"],  // Â
    ["\u{0045}", "\u{00C9}", "\u{00C8}", "\u{1EBA}", "\u{1EBC}", "\u{1EB8}"],  // E
    ["\u{00CA}", "\u{1EBE}", "\u{1EC0}", "\u{1EC2}", "\u{1EC4}", "\u{1EC6}"],  // Ê
    ["\u{0049}", "\u{00CD}", "\u{00CC}", "\u{1EC8}", "\u{0128}", "\u{1ECA}"],  // I
    ["\u{004F}", "\u{00D3}", "\u{00D2}", "\u{1ECE}", "\u{00D5}", "\u{1ECC}"],  // O
    ["\u{00D4}", "\u{1ED0}", "\u{1ED2}", "\u{1ED4}", "\u{1ED6}", "\u{1ED8}"],  // Ô
    ["\u{01A0}", "\u{1EDA}", "\u{1EDC}", "\u{1EDE}", "\u{1EE0}", "\u{1EE2}"],  // Ơ
    ["\u{0055}", "\u{00DA}", "\u{00D9}", "\u{1EE6}", "\u{0168}", "\u{1EE4}"],  // U
    ["\u{01AF}", "\u{1EE8}", "\u{1EEA}", "\u{1EEC}", "\u{1EEE}", "\u{1EF0}"],  // Ư
    ["\u{0059}", "\u{00DD}", "\u{1EF2}", "\u{1EF6}", "\u{1EF8}", "\u{1EF4}"],  // Y
]

/// Index into the vowel tables for a toneless lowercase base vowel, or nil.
public func vowelIndex(_ lowerBase: Character) -> Int? {
    for i in 0..<vowelCount where lowerTable[i][0] == lowerBase { return i }
    return nil
}

public func isVowelBase(_ lowerBase: Character) -> Bool {
    vowelIndex(lowerBase) != nil
}

/// The precomposed character for a (vowel, tone, case) triple.
public func composeVowel(_ index: Int, _ tone: Tone, upper: Bool) -> Character? {
    guard index >= 0, index < vowelCount else { return nil }
    let row = upper ? upperTable[index] : lowerTable[index]
    return row[Int(tone.rawValue)]
}

/// Splits any Vietnamese letter into its toneless lowercase base, tone and case.
/// Returns nil for characters outside the Vietnamese alphabet.
public func decomposeLetter(_ c: Character) -> (base: Character, tone: Tone, upper: Bool)? {
    for v in 0..<vowelCount {
        for t in 0..<Tone.count {
            if lowerTable[v][t] == c {
                return (lowerTable[v][0], Tone(rawValue: UInt8(t))!, false)
            }
            if upperTable[v][t] == c {
                return (lowerTable[v][0], Tone(rawValue: UInt8(t))!, true)
            }
        }
    }
    if c == dLower || c == dUpper {
        return (dLower, .none, c == dUpper)
    }
    if c.isASCII, c >= "a", c <= "z" {
        return (c, .none, false)
    }
    if c.isASCII, c >= "A", c <= "Z" {
        return (toLowerAscii(c), .none, true)
    }
    return nil
}

/// Composes a letter from the parts produced by `decomposeLetter`.
public func composeLetter(_ lowerBase: Character, _ tone: Tone, upper: Bool) -> Character {
    if let v = vowelIndex(lowerBase), let c = composeVowel(v, tone, upper: upper) {
        return c
    }
    if lowerBase == dLower { return upper ? dUpper : dLower }
    if lowerBase.isASCII, lowerBase >= "a", lowerBase <= "z" {
        return upper ? toUpperAscii(lowerBase) : lowerBase
    }
    return lowerBase
}

public func isLetter(_ c: Character) -> Bool {
    decomposeLetter(c) != nil
}

// ASCII-only case helpers; the engine never sees non-ASCII input keys.

public func toLowerAscii(_ c: Character) -> Character {
    guard c.isASCII, c >= "A", c <= "Z" else { return c }
    return Character(Unicode.Scalar(c.asciiValue! - 65 + 97))
}

public func toUpperAscii(_ c: Character) -> Character {
    guard c.isASCII, c >= "a", c <= "z" else { return c }
    return Character(Unicode.Scalar(c.asciiValue! - 97 + 65))
}

public func isUpperAscii(_ c: Character) -> Bool {
    c.isASCII && c >= "A" && c <= "Z"
}

public func isAsciiLetter(_ c: Character) -> Bool {
    c.isASCII && ((c >= "a" && c <= "z") || (c >= "A" && c <= "Z"))
}
