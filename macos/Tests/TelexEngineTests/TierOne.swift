// Tier 1: the pure engine, no AppKit. See docs/TESTING.md.
//
// This is a direct port of the Windows suite, case for case, on purpose: the
// two builds share no code, so the only thing keeping them the same is that
// they run the same checks and report the same total.
import Foundation
import TelexEngine

struct Failure {
    let keys: String
    let want: String
    let got: String
    let line: Int
}

final class TierOne {
    private(set) var passed = 0
    private(set) var failures: [Failure] = []

    var total: Int { passed + failures.count }

    // Drives the engine exactly the way the macOS layer does, keeping a model
    // of the text on screen. "\u{8}" means Backspace.
    private func type(_ keys: String) -> String {
        let engine = Engine()
        var screen: [Character] = []
        for ch in keys {
            if ch == "\u{8}" {
                _ = engine.onBackspace()
                if !screen.isEmpty { screen.removeLast() }
                continue
            }
            let r = engine.onKey(ch)  // non-letters end the word
            if r.action == .passThrough {
                screen.append(ch)
            } else {
                screen.removeLast(r.backspaces)
                screen.append(contentsOf: r.insert)
            }
        }
        return String(screen)
    }

    private func check(_ keys: String, _ expected: String, line: Int = #line) {
        let got = type(keys)
        if got == expected {
            passed += 1
        } else {
            failures.append(Failure(keys: keys, want: expected, got: got, line: line))
        }
    }

    private func checkRaw(_ label: String, _ condition: Bool, line: Int = #line) {
        if condition {
            passed += 1
        } else {
            failures.append(Failure(keys: label, want: "valid", got: "invalid", line: line))
        }
    }

    // MARK: - Groups

    // docs/TELEX.md section 10
    private func specExamples() {
        check("tieengs vieejt", "tiếng việt")
        check("Tieengs Vieejt", "Tiếng Việt")
        check("xin chaof", "xin chào")
        check("ddaji hocj quoocs gia", "đại học quốc gia")
        check("nguwowif", "người")
        check("nguoiwf", "người")
        check("khoong cos gif", "không có gì")
        check("thuyr tinh", "thủy tinh")
        check("hoaf binhf", "hòa bình")
        check("quays", "quáy")
        check("quys", "quý")
        check("giaf", "già")
        check("cuar", "của")
        check("mias", "mía")
        check("ruowuj", "rượu")
        check("nghieengf", "nghiềng")
        check("bows", "bớ")
        check("aas", "ấ")
        check("w", "ư")
        check("ww", "w")
        check("booong", "boong")
        check("hello", "hello")
        check("sport", "sport")
        check("email", "email")
        check("test", "tét")
        check("asz", "a")
        check("assf", "asf")
    }

    private func tones() {
        // Every tone on every single vowel.
        check("as", "á");   check("af", "à");   check("ar", "ả")
        check("ax", "ã");   check("aj", "ạ")
        check("aws", "ắ");  check("awf", "ằ");  check("awr", "ẳ")
        check("awx", "ẵ");  check("awj", "ặ")
        check("aas", "ấ");  check("aaf", "ầ");  check("aar", "ẩ")
        check("aax", "ẫ");  check("aaj", "ậ")
        check("es", "é");   check("ef", "è");   check("er", "ẻ")
        check("ex", "ẽ");   check("ej", "ẹ")
        check("ees", "ế");  check("eef", "ề");  check("eer", "ể")
        check("eex", "ễ");  check("eej", "ệ")
        check("is", "í");   check("if", "ì");   check("ir", "ỉ")
        check("ix", "ĩ");   check("ij", "ị")
        check("os", "ó");   check("of", "ò");   check("or", "ỏ")
        check("ox", "õ");   check("oj", "ọ")
        check("oos", "ố");  check("oof", "ồ");  check("oor", "ổ")
        check("oox", "ỗ");  check("ooj", "ộ")
        check("ows", "ớ");  check("owf", "ờ");  check("owr", "ở")
        check("owx", "ỡ");  check("owj", "ợ")
        check("us", "ú");   check("uf", "ù");   check("ur", "ủ")
        check("ux", "ũ");   check("uj", "ụ")
        check("uws", "ứ");  check("uwf", "ừ");  check("uwr", "ử")
        check("uwx", "ữ");  check("uwj", "ự")
        check("ys", "ý");   check("yf", "ỳ");   check("yr", "ỷ")
        check("yx", "ỹ");   check("yj", "ỵ")

        // Changing the tone mid-word replaces it.
        check("asf", "à")
        check("asx", "ã")
        check("banjs", "bán")
        // z clears the tone; with no tone it is a literal z.
        check("baz", "baz")
        check("bansz", "ban")
        check("hoafz", "hoa")
    }

    private func letterTransforms() {
        check("aa", "â");   check("ee", "ê");   check("oo", "ô")
        check("aw", "ă");   check("ow", "ơ");   check("uw", "ư")
        check("dd", "đ")
        check("caan", "cân")
        check("chee", "chê")
        check("khoong", "không")
        check("nawm", "năm")
        check("mowi", "mơi")
        check("tuw", "tư")
        check("ddi", "đi")
        check("ddaan", "đân")
        // Standalone w and w after a consonant.
        check("tw", "tư")
        check("w", "ư")
        // The uo cluster, typed in every order.
        check("duongw", "dương")
        check("dduowngf", "đường")
        check("dduwowngf", "đường")
        check("nguwowif", "người")
        check("ruwowuj", "rượu")
        check("tuwowi", "tươi")
        // The u of "qu" is part of the onset and must not become ư.
        check("quowr", "quở")
        check("quee", "quê")
        check("quaans", "quấn")
    }

    private func undo() {
        check("ass", "as")
        check("aff", "af")
        check("arr", "ar")
        check("axx", "ax")
        check("ajj", "aj")
        check("aaa", "aa")
        check("eee", "ee")
        check("ooo", "oo")
        check("aww", "aw")
        check("oww", "ow")
        check("uww", "uw")
        check("ddd", "dd")
        check("ww", "w")
        check("uoww", "uow")
        check("booong", "boong")
        check("xooong", "xoong")
        // After an undo the word is literal again and keeps behaving normally.
        check("assn", "asn")
        check("aaan", "aan")
    }

    private func freePosition() {
        check("vieetj", "việt")
        check("vieejt", "việt")
        check("vietj", "viẹt")
        check("toans", "toán")
        check("hoacj", "hoạc")
        check("duongwf", "dường")
        check("hoafn", "hoàn")
        check("hoanf", "hoàn")
        check("tieengs", "tiếng")
        check("tiengs", "tiéng")

        // Type the whole word first, put the marks on afterwards. This is how
        // most people actually type, and every one of these came from a real
        // session.
        check("vanas", "vấn")
        check("vanas ddeef", "vấn đề")
        check("quanaf", "quần")
        check("thayas", "thấy")
        check("danhs", "dánh")
        check("danhas", "dấnh")
        check("ddanhs", "đánh")
        check("nguoiwf", "người")
        check("duocwj", "dược")
        check("dduocwj", "được")
        check("dduowcj", "được")
        check("chungs", "chúng")
        check("chungs ta", "chúng ta")
        check("khongo", "không")
        check("congoj", "cộng")
        check("muonos", "muốn")
        check("tienes", "tiến")
        check("nguyeenj", "nguyện")
        check("nguyenej", "nguyện")
        check("truongwf", "trường")
        check("ngoeor", "ngoẻo")
        check("khoeof", "khoèo")
        check("xooong", "xoong")
        // Undo only counts straight after the key that applied the mark, so
        // this trailing e is just a letter that does not belong to the syllable.
        check("vieete", "vieete")
        // Adding a letter that cannot belong to the syllable puts the word back.
        check("chungso", "chungso")
    }

    // đ is the letter people report problems with most, so it gets its own group.
    private func dStroke() {
        check("dd", "đ")
        check("ddanf", "đàn")
        check("dandf", "đàn")       // free position, mark typed after the word
        check("ddaji", "đại")
        check("dajid", "đại")
        check("ddoongf", "đồng")
        check("doongdf", "đồng")
        check("ddepj", "đẹp")
        check("depjd", "đẹp")
        check("dduongwf", "đường")
        check("dduwowngf", "đường")
        check("duongwdf", "đường")
        check("ddi", "đi")
        check("did", "đi")
        check("ddeef", "đề")
        check("Ddanhs", "Đánh")
        check("DDanhs", "Đánh")
        // The word already had its đ; a stray d at the end cannot belong to it.
        check("ddand", "ddand")
        // Known mangling: English words where the first letter is also a d.
        check("dad", "đa")
        check("handed", "handed")
        check("addd", "addd")
    }

    // Typing, deleting part of it, then carrying on: the engine has to stay in
    // step with what is actually on screen.
    private func backspaceThenContinue() {
        check("tieengs\u{8}\u{8}ng", "tiếng")
        check("vieejt\u{8}t", "việt")
        check("hoaf\u{8}n", "hòn")
        check("hoaf\u{8}\u{8}as", "há")
        // The đ survives: a backspace stands between it and this d, so the d no
        // longer counts as retyping.
        check("ddi\u{8}da", "đda")
        check("nguwowif\u{8}\u{8}owif", "người")
        // Putting the mark back after a backspace must not strip it instead.
        check("tieengs\u{8}s", "tiến")
        check("tieengs\u{8}gs", "tiếng")
        check("quanaf\u{8}f", "quầ")
        check("thayas\u{8}s", "thấ")
        check("hoangf\u{8}gf", "hoàng")
        check("ngoaif\u{8}if", "ngoài")
        check("dduongwf\u{8}f", "đườn")
        // Pressing it a second time still removes it.
        check("tieengs\u{8}ss", "tiêns")
        check("banj\u{8}\u{8}ans", "bán")
        check("khoong\u{8}\u{8}ng", "không")
        // The â was deleted, so the retyped vowel is a plain a.
        check("vanas\u{8}\u{8}ans", "ván")
        check("cuar\u{8}af", "cùa")
        check("aas\u{8}\u{8}aas", "ấ")
        // Text the user has already seen and kept is never rewritten afterwards.
        check("ddanf\u{8}\u{8}nf", "đnf")
        check("ddanf\u{8}\u{8}", "đ")
        // Marks still work on what is left after deleting.
        check("dduongwf\u{8}\u{8}\u{8}ngf", "đừng")
        check("ddaji\u{8}of", "đào")
    }

    // Backspacing back over a space into a word that was finished a while ago.
    private func backspaceAcrossWords() {
        check("vay roi\u{8}\u{8}\u{8}\u{8}a", "vây")
        check("vay roi di\u{8}\u{8}\u{8}\u{8}\u{8}\u{8}\u{8}a", "vây")
        check("vay\u{8}\u{8}\u{8}vaya", "vây")
        check("tieeng viet\u{8}\u{8}\u{8}\u{8}\u{8}s", "tiếng")
        check("hoa binh\u{8}\u{8}\u{8}\u{8}\u{8}f", "hòa")
        check("con nguoi\u{8}\u{8}\u{8}\u{8}\u{8}\u{8}f", "còn")
        check("dang lam\u{8}\u{8}\u{8}\u{8}d", "đang")
        check("nha cua\u{8}\u{8}\u{8}\u{8}f", "nhà")
        check("mua ban\u{8}\u{8}\u{8}\u{8}w", "mưa")
        check("muaw", "mưa")
        // Two words back, with punctuation rather than a space in between.
        check("hoa,binh\u{8}\u{8}\u{8}\u{8}\u{8}f", "hòa")
        check("mot 123\u{8}\u{8}\u{8}\u{8}j", "mọt")
        // Marks that were already applied survive the round trip.
        check("tieengs vieejt\u{8}\u{8}\u{8}\u{8}\u{8} ok", "tiếng ok")
        check("dduongwf xa\u{8}\u{8}\u{8}s", "đướng")
        // A word we never typed is not ours to touch.
        check("\u{8}\u{8}\u{8}a", "a")
    }

    private func tonePlacement() {
        // Rule 1: qu and gi onsets.
        check("quys", "quý")
        check("quyj", "quỵ")
        check("quar", "quả")
        check("giaf", "già")
        check("gif", "gì")
        check("giuwx", "giữ")
        // Rule 2: ê and ơ win.
        check("tieenf", "tiền")
        check("nguwowif", "người")
        check("chieeus", "chiếu")
        // Rule 3: single vowel.
        check("banj", "bạn")
        check("hocj", "học")
        check("khoongs", "khống")
        // Rule 4: with a coda, the last vowel.
        check("muoons", "muốn")
        check("hoafn", "hoàn")
        check("xuaanf", "xuần")
        check("tuyeetj", "tuyệt")
        // Rule 5: open syllable, second to last vowel.
        check("hoaf", "hòa")
        check("hoas", "hóa")
        check("thuyr", "thủy")
        check("cuar", "của")
        check("mias", "mía")
        check("chuooix", "chuỗi")
        check("ngoaif", "ngoài")
        check("khuyur", "khuỷu")
        check("caaus", "cấu")
    }

    private func englishWords() {
        let words = [
            "hello", "sport", "email", "world", "string", "please", "modern",
            "screen", "border", "helper", "silver", "morning", "problem",
            "windows", "keyboard", "monitor", "printer", "network", "browser",
            "compiler", "debugger", "terminal", "shell", "success",
        ]
        for w in words { check(w, w) }

        // Known and accepted mangling: these are valid Vietnamese syllables, so
        // the engine has no reason to back off.
        check("test", "tét")
        check("wrong", "ưỏng")
        check("win", "ưin")
        // A doubled tone key is an undo, so one of the two letters is consumed.
        check("password", "pasword")
        check("miss", "mis")
    }

    private func capitalisation() {
        check("Aa", "Â")
        check("DD", "Đ")
        check("dD", "Đ")
        check("Dd", "Đ")
        check("Ows", "Ớ")
        check("Tieengs", "Tiếng")
        check("VIEEJT", "VIỆT")
        check("Nguwowif", "Người")
        check("HOAF", "HÒA")
    }

    private func backspace() {
        check("hoas\u{8}", "hó")
        check("hoas\u{8}\u{8}", "h")
        check("ddi\u{8}", "đ")
        check("ddi\u{8}\u{8}", "")
        check("dd\u{8}d", "d")
        check("tieengs\u{8}s", "tiến")
        check("as\u{8}s", "s")
        check("\u{8}as", "á")
    }

    private func wordBoundaries() {
        check("as as", "á á")
        check("as.as", "á.á")
        check("as1as", "á1á")
        check("hoaf, hoaf", "hòa, hòa")
        check("a-s", "a-s")
        check("tieengs\ttieengs", "tiếng\ttiếng")
        check("mootj hai ba", "một hai ba")
    }

    private func revert() {
        // A transformed word that stops being Vietnamese goes back to raw keys.
        check("hell", "hell")
        check("weather", "weather")
        check("ddark", "ddark")
        check("cheept", "cheept")
        check("chees", "chế")
        // Reverting is final for the rest of the word, but the next word is fresh.
        check("hello tieengs", "hello tiếng")
    }

    // The structural checker directly. This is the only place the strict mode is
    // exercised at all; the engine itself only ever uses lenient.
    private func syllableUnit() {
        let cases: [(word: String, strict: Bool, valid: Bool)] = [
            ("ban", true, true), ("hoa", true, true),
            ("quy", true, true), ("gia", true, true),
            ("khuyu", true, true), ("nghieng", true, false),
            ("nghieng", false, true),   // "ie" is on its way to "iê"
            ("hell", false, false), ("spo", false, false),
            ("strong", false, false), ("bpt", false, false),
            ("world", false, false), ("q", false, true),
            ("q", true, false),
        ]
        for c in cases {
            let got = isValidSyllable(Array(c.word), tone: .none, strict: c.strict)
            checkRaw("syllable \"\(c.word)\" strict=\(c.strict)", got == c.valid)
        }
    }

    /// Derives the Telex keystrokes that should produce `word`, per TELEX.md.
    private func keysFor(_ word: String) -> String {
        var keys = ""
        var toneKey: Character?
        for c in word {
            guard let parts = decomposeLetter(c) else {
                keys.append(c)
                continue
            }
            var piece: [Character]
            switch parts.base {
            case "\u{0103}": piece = ["a", "w"]  // ă
            case "\u{00E2}": piece = ["a", "a"]  // â
            case "\u{00EA}": piece = ["e", "e"]  // ê
            case "\u{00F4}": piece = ["o", "o"]  // ô
            case "\u{01A1}": piece = ["o", "w"]  // ơ
            case "\u{01B0}": piece = ["u", "w"]  // ư
            case "\u{0111}": piece = ["d", "d"]  // đ
            default: piece = [parts.base]
            }
            if parts.upper { piece[0] = toUpperAscii(piece[0]) }
            keys.append(contentsOf: piece)
            switch parts.tone {
            case .acute: toneKey = "s"
            case .grave: toneKey = "f"
            case .hook: toneKey = "r"
            case .tilde: toneKey = "x"
            case .dot: toneKey = "j"
            case .none: break
            }
        }
        if let toneKey { keys.append(toneKey) }
        return keys
    }

    /// Round-trips every syllable in docs/corpus.txt.
    @discardableResult
    func corpus(at path: String) -> Int {
        guard let text = try? String(contentsOfFile: path, encoding: .utf8) else {
            failures.append(Failure(keys: path, want: "readable corpus", got: "cannot open", line: #line))
            return 0
        }
        var words = 0
        for rawLine in text.split(separator: "\n", omittingEmptySubsequences: false) {
            let line = rawLine.trimmingCharacters(in: CharacterSet(charactersIn: "\r "))
            if line.isEmpty || line.hasPrefix("#") { continue }
            words += 1
            let keys = keysFor(line)
            let got = type(keys)
            if got == line {
                passed += 1
            } else {
                failures.append(Failure(keys: "corpus \"\(line)\" -> keys \"\(keys)\"",
                                        want: line, got: got, line: #line))
            }
        }
        return words
    }

    func runAll() {
        specExamples()
        tones()
        letterTransforms()
        undo()
        freePosition()
        dStroke()
        backspaceThenContinue()
        backspaceAcrossWords()
        tonePlacement()
        englishWords()
        capitalisation()
        backspace()
        wordBoundaries()
        revert()
        syllableUnit()
    }
}
