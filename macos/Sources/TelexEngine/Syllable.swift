// Vietnamese syllable structure: parsing, validity and tone placement.
// Operates on toneless lowercase base letters (see Tables.swift).
//
// The two validity modes matter: see docs/TELEX.md section 4. The engine only
// ever calls the lenient one.

public struct Syllable: Sendable {
    public var onsetLen = 0
    public var nucleusStart = 0
    public var nucleusLen = 0
    public var codaStart = 0
    public var codaLen = 0
}

private let onsets: [[Character]] = [
    "ngh",
    "ch", "gh", "gi", "kh", "ng", "nh", "ph", "qu", "th", "tr",
    "b", "c", "d", "\u{0111}", "g", "h", "k", "l", "m", "n", "p",
    "r", "s", "t", "v", "x",
].map(Array.init)

private let codas: [[Character]] = [
    "ch", "ng", "nh", "c", "m", "n", "p", "t",
].map(Array.init)

// Every nucleus the language allows. Order is irrelevant.
private let nuclei: [[Character]] = [
    // single
    "a", "\u{0103}", "\u{00E2}", "e", "\u{00EA}", "i", "o", "\u{00F4}",
    "\u{01A1}", "u", "\u{01B0}", "y",
    // double
    "ai", "ao", "au", "ay", "\u{00E2}u", "\u{00E2}y", "eo", "\u{00EA}u",
    "ia", "i\u{00EA}", "iu", "oa", "o\u{0103}", "oe", "oi", "\u{00F4}i",
    "\u{01A1}i", "ua", "u\u{00E2}", "u\u{00EA}", "ui", "u\u{00F4}", "u\u{01A1}",
    "uy", "\u{01B0}a", "\u{01B0}i", "\u{01B0}\u{01A1}", "\u{01B0}u", "y\u{00EA}",
    // triple
    "i\u{00EA}u", "y\u{00EA}u", "oai", "oao", "oay", "oeo", "u\u{00E2}y",
    "u\u{00F4}i", "uya", "uy\u{00EA}", "uyu", "\u{01B0}\u{01A1}i", "\u{01B0}\u{01A1}u",
].map(Array.init)

private func stripDiacritic(_ c: Character) -> Character {
    switch c {
    case "\u{0103}", "\u{00E2}": return "a"   // ă â
    case "\u{00EA}": return "e"               // ê
    case "\u{00F4}", "\u{01A1}": return "o"   // ô ơ
    case "\u{01B0}": return "u"               // ư
    default: return c
    }
}

/// True when a letter already typed could still turn into `target`. A plain
/// a/e/o/u may yet receive its diacritic, but a letter that already has one is
/// final: "ie" is on its way to "iê", while "ôe" is on its way to nothing.
private func canBecome(_ typed: Character, _ target: Character) -> Bool {
    if typed == target { return true }
    let typedIsPlain = stripDiacritic(typed) == typed
    return typedIsPlain && stripDiacritic(target) == typed
}

private func isPrefix(_ prefix: ArraySlice<Character>, of full: [Character]) -> Bool {
    prefix.count <= full.count && zip(prefix, full).allSatisfy { $0 == $1 }
}

private func onsetOk(_ onset: ArraySlice<Character>, strict: Bool) -> Bool {
    if onset.isEmpty { return true }
    for candidate in onsets {
        if candidate.count == onset.count, zip(candidate, onset).allSatisfy({ $0 == $1 }) {
            return true
        }
        // e.g. "q" on the way to "qu"
        if !strict, isPrefix(onset, of: candidate) { return true }
    }
    return false
}

private func codaOk(_ coda: ArraySlice<Character>) -> Bool {
    if coda.isEmpty { return true }
    return codas.contains { $0.count == coda.count && zip($0, coda).allSatisfy { $0 == $1 } }
}

private func nucleusOk(_ nucleus: ArraySlice<Character>, strict: Bool) -> Bool {
    if nucleus.isEmpty { return !strict }
    if strict {
        return nuclei.contains { $0.count == nucleus.count && zip($0, nucleus).allSatisfy { $0 == $1 } }
    }
    // While typing, diacritics arrive after the letters they belong to, so
    // accept anything that is still on its way to a real nucleus.
    for full in nuclei where nucleus.count <= full.count {
        var ok = true
        for (i, c) in nucleus.enumerated() where ok {
            ok = canBecome(c, full[i])
        }
        if ok { return true }
    }
    return false
}

/// Splits `bases` into onset / nucleus / coda, or nil if it cannot be one.
public func parseSyllable(_ bases: [Character], strict: Bool) -> Syllable? {
    let size = bases.count
    if size == 0 { return nil }

    // Longest onset first, backtracking to shorter ones ("gia" is gi+a, but
    // "gao" has to fall back from "gh"-style matches to plain "g").
    var onsetLen = min(3, size)
    while onsetLen >= 0 {
        defer { onsetLen -= 1 }
        let onset = bases[0..<onsetLen]
        if !onsetOk(onset, strict: strict) { continue }

        var i = onsetLen
        while i < size, isVowelBase(bases[i]) { i += 1 }
        let nucleus = bases[onsetLen..<i]
        let coda = bases[i...]

        if !nucleusOk(nucleus, strict: strict) { continue }
        if !codaOk(coda) { continue }
        if nucleus.isEmpty && !coda.isEmpty { continue }

        var out = Syllable()
        out.onsetLen = onsetLen
        out.nucleusStart = onsetLen
        out.nucleusLen = nucleus.count
        out.codaStart = i
        out.codaLen = coda.count
        return out
    }
    return nil
}

/// Index of the letter that should carry the tone mark, or -1 if there is none.
public func tonePosition(_ bases: [Character], _ s: Syllable) -> Int {
    // Rule 1: in "qu" and "gi" the u/i is part of the onset. parseSyllable has
    // already excluded it from the nucleus; the only thing left to handle is a
    // syllable with no other vowel, like "gì" or "quỳ".
    if s.nucleusLen == 0 {
        if s.onsetLen == 2 {
            let onset = String(bases[0..<2])
            if onset == "qu" || onset == "gi" { return s.onsetLen - 1 }
        }
        return -1
    }

    // Rule 2: ê and ơ always win.
    for i in 0..<s.nucleusLen {
        let c = bases[s.nucleusStart + i]
        if c == "\u{00EA}" || c == "\u{01A1}" { return s.nucleusStart + i }
    }

    // Rule 3: a single vowel takes the tone.
    if s.nucleusLen == 1 { return s.nucleusStart }

    // Rule 4: with a coda, the tone goes on the last vowel.
    if s.codaLen > 0 { return s.nucleusStart + s.nucleusLen - 1 }

    // Rule 5: open syllable, classic placement - second to last vowel.
    return s.nucleusStart + s.nucleusLen - 2
}

/// Codas c/ch/p/t only accept the acute and dot tones.
public func toneAllowed(_ bases: [Character], _ s: Syllable, _ tone: Tone) -> Bool {
    if tone == .none || tone == .acute || tone == .dot { return true }
    if s.codaLen == 0 { return true }
    let coda = String(bases[s.codaStart..<(s.codaStart + s.codaLen)])
    return !(coda == "c" || coda == "ch" || coda == "p" || coda == "t")
}

/// parseSyllable + toneAllowed in one call.
public func isValidSyllable(_ bases: [Character], tone: Tone, strict: Bool) -> Bool {
    guard let s = parseSyllable(bases, strict: strict) else { return false }
    if tone != .none, tonePosition(bases, s) < 0 { return false }
    return toneAllowed(bases, s, tone)
}
