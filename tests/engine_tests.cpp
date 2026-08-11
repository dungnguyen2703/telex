// Tier 1 tests: the pure engine, no Windows involved. See docs/TESTING.md.
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/engine/syllable.h"
#include "../src/engine/tables.h"
#include "../src/engine/telex.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

int g_pass = 0;
int g_fail = 0;

std::string ToUtf8(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        const unsigned int cp = c;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u16string FromUtf8(const std::string& s) {
    std::u16string out;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char16_t>(c));
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            out.push_back(static_cast<char16_t>(((c & 0x1F) << 6) |
                                                (s[i + 1] & 0x3F)));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            out.push_back(static_cast<char16_t>(((c & 0x0F) << 12) |
                                                ((s[i + 1] & 0x3F) << 6) |
                                                (s[i + 2] & 0x3F)));
            i += 3;
        } else {
            i += 1;  // corpus files are plain UTF-8; anything else is a typo
        }
    }
    return out;
}

// Drives the engine exactly the way the Windows layer does, keeping a model of
// the text on screen. '\b' means Backspace.
std::u16string Type(const std::string& keys) {
    telex::Engine engine;
    std::u16string screen;
    for (char raw : keys) {
        const char16_t ch = static_cast<char16_t>(static_cast<unsigned char>(raw));
        if (ch == u'\b') {
            engine.OnBackspace();
            if (!screen.empty()) screen.pop_back();
            continue;
        }
        const telex::Result r = engine.OnKey(ch);  // non-letters end the word
        if (r.action == telex::Action::PassThrough) {
            screen.push_back(ch);
        } else {
            screen.erase(screen.size() - static_cast<size_t>(r.backspaces));
            screen += r.insert;
        }
    }
    return screen;
}

void Check(const char* keys, const char* expectedUtf8, const char* file, int line) {
    const std::u16string got = Type(keys);
    const std::u16string want = FromUtf8(expectedUtf8);
    if (got == want) {
        ++g_pass;
        return;
    }
    ++g_fail;
    std::printf("FAIL %s:%d  keys=\"%s\"\n     want \"%s\"\n     got  \"%s\"\n",
                file, line, keys, expectedUtf8, ToUtf8(got).c_str());
}

#define CHECK(keys, expected) Check(keys, expected, __FILE__, __LINE__)

// Derives the Telex keystrokes that should produce `word`, per TELEX.md.
std::string KeysFor(const std::u16string& word) {
    std::string keys;
    char toneKey = 0;
    for (char16_t c : word) {
        char16_t base = 0;
        uint8_t tone = 0;
        bool upper = false;
        if (!telex::DecomposeLetter(c, base, tone, upper)) {
            keys.push_back(static_cast<char>(c));
            continue;
        }
        std::string piece;
        switch (base) {
            case 0x0103: piece = "aw"; break;  // ă
            case 0x00E2: piece = "aa"; break;  // â
            case 0x00EA: piece = "ee"; break;  // ê
            case 0x00F4: piece = "oo"; break;  // ô
            case 0x01A1: piece = "ow"; break;  // ơ
            case 0x01B0: piece = "uw"; break;  // ư
            case 0x0111: piece = "dd"; break;  // đ
            default: piece = std::string(1, static_cast<char>(base)); break;
        }
        if (upper) piece[0] = static_cast<char>(std::toupper(piece[0]));
        keys += piece;
        switch (tone) {
            case telex::kAcute: toneKey = 's'; break;
            case telex::kGrave: toneKey = 'f'; break;
            case telex::kHook:  toneKey = 'r'; break;
            case telex::kTilde: toneKey = 'x'; break;
            case telex::kDot:   toneKey = 'j'; break;
            default: break;
        }
    }
    if (toneKey != 0) keys.push_back(toneKey);
    return keys;
}

void RunSpecExamples() {
    // docs/TELEX.md section 10
    CHECK("tieengs vieejt", "tiếng việt");
    CHECK("Tieengs Vieejt", "Tiếng Việt");
    CHECK("xin chaof", "xin chào");
    CHECK("ddaji hocj quoocs gia", "đại học quốc gia");
    CHECK("nguwowif", "người");
    CHECK("nguoiwf", "người");
    CHECK("khoong cos gif", "không có gì");
    CHECK("thuyr tinh", "thủy tinh");
    CHECK("hoaf binhf", "hòa bình");
    CHECK("quays", "quáy");
    CHECK("quys", "quý");
    CHECK("giaf", "già");
    CHECK("cuar", "của");
    CHECK("mias", "mía");
    CHECK("ruowuj", "rượu");
    CHECK("nghieengf", "nghiềng");
    CHECK("bows", "bớ");
    CHECK("aas", "ấ");
    CHECK("w", "ư");
    CHECK("ww", "w");
    CHECK("booong", "boong");
    CHECK("hello", "hello");
    CHECK("sport", "sport");
    CHECK("email", "email");
    CHECK("test", "tét");
    CHECK("asz", "a");
    CHECK("assf", "asf");
}

void RunTones() {
    // Every tone on every single vowel.
    CHECK("as", "á");   CHECK("af", "à");   CHECK("ar", "ả");
    CHECK("ax", "ã");   CHECK("aj", "ạ");
    CHECK("aws", "ắ");  CHECK("awf", "ằ");  CHECK("awr", "ẳ");
    CHECK("awx", "ẵ");  CHECK("awj", "ặ");
    CHECK("aas", "ấ");  CHECK("aaf", "ầ");  CHECK("aar", "ẩ");
    CHECK("aax", "ẫ");  CHECK("aaj", "ậ");
    CHECK("es", "é");   CHECK("ef", "è");   CHECK("er", "ẻ");
    CHECK("ex", "ẽ");   CHECK("ej", "ẹ");
    CHECK("ees", "ế");  CHECK("eef", "ề");  CHECK("eer", "ể");
    CHECK("eex", "ễ");  CHECK("eej", "ệ");
    CHECK("is", "í");   CHECK("if", "ì");   CHECK("ir", "ỉ");
    CHECK("ix", "ĩ");   CHECK("ij", "ị");
    CHECK("os", "ó");   CHECK("of", "ò");   CHECK("or", "ỏ");
    CHECK("ox", "õ");   CHECK("oj", "ọ");
    CHECK("oos", "ố");  CHECK("oof", "ồ");  CHECK("oor", "ổ");
    CHECK("oox", "ỗ");  CHECK("ooj", "ộ");
    CHECK("ows", "ớ");  CHECK("owf", "ờ");  CHECK("owr", "ở");
    CHECK("owx", "ỡ");  CHECK("owj", "ợ");
    CHECK("us", "ú");   CHECK("uf", "ù");   CHECK("ur", "ủ");
    CHECK("ux", "ũ");   CHECK("uj", "ụ");
    CHECK("uws", "ứ");  CHECK("uwf", "ừ");  CHECK("uwr", "ử");
    CHECK("uwx", "ữ");  CHECK("uwj", "ự");
    CHECK("ys", "ý");   CHECK("yf", "ỳ");   CHECK("yr", "ỷ");
    CHECK("yx", "ỹ");   CHECK("yj", "ỵ");

    // Changing the tone mid-word replaces it.
    CHECK("asf", "à");
    CHECK("asx", "ã");
    CHECK("banjs", "bán");
    // z clears the tone; with no tone it is a literal z.
    CHECK("baz", "baz");
    CHECK("bansz", "ban");
    CHECK("hoafz", "hoa");
}

void RunLetterTransforms() {
    CHECK("aa", "â");   CHECK("ee", "ê");   CHECK("oo", "ô");
    CHECK("aw", "ă");   CHECK("ow", "ơ");   CHECK("uw", "ư");
    CHECK("dd", "đ");
    CHECK("caan", "cân");
    CHECK("chee", "chê");
    CHECK("khoong", "không");
    CHECK("nawm", "năm");
    CHECK("mowi", "mơi");
    CHECK("tuw", "tư");
    CHECK("ddi", "đi");
    CHECK("ddaan", "đân");
    // Standalone w and w after a consonant.
    CHECK("tw", "tư");
    CHECK("w", "ư");
    // The uo cluster, typed in every order.
    CHECK("duongw", "dương");
    CHECK("dduowngf", "đường");
    CHECK("dduwowngf", "đường");
    CHECK("nguwowif", "người");
    CHECK("ruwowuj", "rượu");
    CHECK("tuwowi", "tươi");
    // The u of "qu" is part of the onset and must not become ư.
    CHECK("quowr", "quở");
    CHECK("quee", "quê");
    CHECK("quaans", "quấn");
}

void RunUndo() {
    CHECK("ass", "as");
    CHECK("aff", "af");
    CHECK("arr", "ar");
    CHECK("axx", "ax");
    CHECK("ajj", "aj");
    CHECK("aaa", "aa");
    CHECK("eee", "ee");
    CHECK("ooo", "oo");
    CHECK("aww", "aw");
    CHECK("oww", "ow");
    CHECK("uww", "uw");
    CHECK("ddd", "dd");
    CHECK("ww", "w");
    CHECK("uoww", "uow");
    CHECK("booong", "boong");
    CHECK("xooong", "xoong");
    // After an undo the word is literal again and keeps behaving normally.
    CHECK("assn", "asn");
    CHECK("aaan", "aan");
}

void RunFreePosition() {
    CHECK("vieetj", "việt");
    CHECK("vieejt", "việt");
    CHECK("vietj", "viẹt");
    CHECK("toans", "toán");
    CHECK("hoacj", "hoạc");
    CHECK("duongwf", "dường");
    CHECK("hoafn", "hoàn");
    CHECK("hoanf", "hoàn");
    CHECK("tieengs", "tiếng");
    CHECK("tiengs", "tiéng");

    // Type the whole word first, put the marks on afterwards. This is how most
    // people actually type, and every one of these came from a real session.
    CHECK("vanas", "vấn");
    CHECK("vanas ddeef", "vấn đề");
    CHECK("quanaf", "quần");
    CHECK("thayas", "thấy");
    CHECK("danhs", "dánh");
    CHECK("danhas", "dấnh");
    CHECK("ddanhs", "đánh");
    CHECK("nguoiwf", "người");
    CHECK("duocwj", "dược");
    CHECK("dduocwj", "được");
    CHECK("dduowcj", "được");
    CHECK("chungs", "chúng");
    CHECK("chungs ta", "chúng ta");
    CHECK("khongo", "không");
    CHECK("congoj", "cộng");
    CHECK("muonos", "muốn");
    CHECK("tienes", "tiến");
    CHECK("nguyeenj", "nguyện");
    CHECK("nguyenej", "nguyện");
    CHECK("truongwf", "trường");
    CHECK("ngoeor", "ngoẻo");
    CHECK("khoeof", "khoèo");
    CHECK("xooong", "xoong");
    // Undo only counts straight after the key that applied the mark, so this
    // trailing e is just a letter that does not belong to the syllable.
    CHECK("vieete", "vieete");
    // Adding a letter that cannot belong to the syllable puts the word back.
    CHECK("chungso", "chungso");
}

// đ is the letter people report problems with most, so it gets its own group.
void RunDStroke() {
    CHECK("dd", "đ");
    CHECK("ddanf", "đàn");
    CHECK("dandf", "đàn");       // free position, mark typed after the word
    CHECK("ddaji", "đại");
    CHECK("dajid", "đại");
    CHECK("ddoongf", "đồng");
    CHECK("doongdf", "đồng");
    CHECK("ddepj", "đẹp");
    CHECK("depjd", "đẹp");
    CHECK("dduongwf", "đường");
    CHECK("dduwowngf", "đường");
    CHECK("duongwdf", "đường");
    CHECK("ddi", "đi");
    CHECK("did", "đi");
    CHECK("ddeef", "đề");
    CHECK("Ddanhs", "Đánh");
    CHECK("DDanhs", "Đánh");
    // The word already had its đ; a stray d at the end cannot belong to it.
    CHECK("ddand", "ddand");
    // Known mangling: English words where the first letter is also a d.
    CHECK("dad", "đa");
    CHECK("handed", "handed");
    CHECK("addd", "addd");
}

// Typing, deleting part of it, then carrying on: the engine has to stay in step
// with what is actually on screen.
void RunBackspaceThenContinue() {
    CHECK("tieengs\b\bng", "tiếng");
    CHECK("vieejt\bt", "việt");
    CHECK("hoaf\bn", "hòn");
    CHECK("hoaf\b\bas", "há");
    // The đ survives: a backspace stands between it and this d, so the d no
    // longer counts as retyping.
    CHECK("ddi\bda", "đda");
    CHECK("nguwowif\b\bowif", "người");
    // Putting the mark back after a backspace must not strip it instead.
    CHECK("tieengs\bs", "tiến");
    CHECK("tieengs\bgs", "tiếng");
    CHECK("quanaf\bf", "quầ");
    CHECK("thayas\bs", "thấ");
    CHECK("hoangf\bgf", "hoàng");
    CHECK("ngoaif\bif", "ngoài");
    CHECK("dduongwf\bf", "đườn");
    // Pressing it a second time still removes it.
    CHECK("tieengs\bss", "tiêns");
    CHECK("banj\b\bans", "bán");
    CHECK("khoong\b\bng", "không");
    // The â was deleted, so the retyped vowel is a plain a.
    CHECK("vanas\b\bans", "ván");
    CHECK("cuar\baf", "cùa");
    CHECK("aas\b\baas", "ấ");
    // Text the user has already seen and kept is never rewritten afterwards.
    CHECK("ddanf\b\bnf", "đnf");
    CHECK("ddanf\b\b", "đ");
    // Marks still work on what is left after deleting.
    CHECK("dduongwf\b\b\bngf", "đừng");
    CHECK("ddaji\bof", "đào");
}

void RunTonePlacement() {
    // Rule 1: qu and gi onsets.
    CHECK("quys", "quý");
    CHECK("quyj", "quỵ");
    CHECK("quar", "quả");
    CHECK("giaf", "già");
    CHECK("gif", "gì");
    CHECK("giuwx", "giữ");
    // Rule 2: ê and ơ win.
    CHECK("tieenf", "tiền");
    CHECK("nguwowif", "người");
    CHECK("chieeus", "chiếu");
    // Rule 3: single vowel.
    CHECK("banj", "bạn");
    CHECK("hocj", "học");
    CHECK("khoongs", "khống");
    // Rule 4: with a coda, the last vowel.
    CHECK("muoons", "muốn");
    CHECK("hoafn", "hoàn");
    CHECK("xuaanf", "xuần");
    CHECK("tuyeetj", "tuyệt");
    // Rule 5: open syllable, second to last vowel.
    CHECK("hoaf", "hòa");
    CHECK("hoas", "hóa");
    CHECK("thuyr", "thủy");
    CHECK("cuar", "của");
    CHECK("mias", "mía");
    CHECK("chuooix", "chuỗi");
    CHECK("ngoaif", "ngoài");
    CHECK("khuyur", "khuỷu");
    CHECK("caaus", "cấu");
}

void RunEnglishWords() {
    const char* words[] = {
        "hello", "sport", "email", "world", "string", "please", "modern",
        "screen", "border", "helper", "silver", "morning", "problem",
        "windows", "keyboard", "monitor", "printer", "network", "browser",
        "compiler", "debugger", "terminal", "shell", "success",
    };
    for (const char* w : words) CHECK(w, w);

    // Known and accepted mangling: these are valid Vietnamese syllables, so the
    // engine has no reason to back off. This is what the exclusion list is for.
    CHECK("test", "tét");
    CHECK("wrong", "ưỏng");
    CHECK("win", "ưin");
    // A doubled tone key is an undo, so one of the two letters is consumed.
    CHECK("password", "pasword");
    CHECK("miss", "mis");
}

void RunCapitalisation() {
    CHECK("Aa", "Â");
    CHECK("DD", "Đ");
    CHECK("dD", "Đ");
    CHECK("Dd", "Đ");
    CHECK("Ows", "Ớ");
    CHECK("Tieengs", "Tiếng");
    CHECK("VIEEJT", "VIỆT");
    CHECK("Nguwowif", "Người");
    CHECK("HOAF", "HÒA");
}

void RunBackspace() {
    CHECK("hoas\b", "hó");
    CHECK("hoas\b\b", "h");
    CHECK("ddi\b", "đ");
    CHECK("ddi\b\b", "");
    CHECK("dd\bd", "d");
    CHECK("tieengs\bs", "tiến");
    CHECK("as\bs", "s");
    CHECK("\bas", "á");
}

// Backspacing back over a space into a word that was finished a while ago. The
// engine has to adopt that word again instead of treating it as untouchable
// text, otherwise "vay" + a comes out as "vaya" instead of vây.
void RunBackspaceAcrossWords() {
    CHECK("vay roi\b\b\b\ba", "vây");
    CHECK("vay roi di\b\b\b\b\b\b\ba", "vây");
    CHECK("vay\b\b\bvaya", "vây");
    CHECK("tieeng viet\b\b\b\b\bs", "tiếng");
    CHECK("hoa binh\b\b\b\b\bf", "hòa");
    CHECK("con nguoi\b\b\b\b\b\bf", "còn");
    CHECK("dang lam\b\b\b\bd", "đang");
    CHECK("nha cua\b\b\b\bf", "nhà");
    CHECK("mua ban\b\b\b\bw", "mưa");
    CHECK("muaw", "mưa");
    // Two words back, with punctuation rather than a space in between.
    CHECK("hoa,binh\b\b\b\b\bf", "hòa");
    CHECK("mot 123\b\b\b\bj", "mọt");
    // Marks that were already applied survive the round trip.
    CHECK("tieengs vieejt\b\b\b\b\b ok", "tiếng ok");
    CHECK("dduongwf xa\b\b\bs", "đướng");
    // A word we never typed is not ours to touch.
    CHECK("\b\b\ba", "a");
}

void RunWordBoundaries() {
    CHECK("as as", "á á");
    CHECK("as.as", "á.á");
    CHECK("as1as", "á1á");
    CHECK("hoaf, hoaf", "hòa, hòa");
    CHECK("a-s", "a-s");
    CHECK("tieengs\ttieengs", "tiếng\ttiếng");
    CHECK("mootj hai ba", "một hai ba");
}

void RunRevert() {
    // A transformed word that stops being Vietnamese goes back to raw keys.
    CHECK("hell", "hell");
    CHECK("weather", "weather");
    CHECK("ddark", "ddark");
    CHECK("cheept", "cheept");
    CHECK("chees", "chế");
    // Reverting is final for the rest of the word, but the next word is fresh.
    CHECK("hello tieengs", "hello tiếng");
}

// Round-trips every syllable in tests/corpus.txt.
void RunCorpus(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::printf("FAIL cannot open corpus file %s\n", path);
        ++g_fail;
        return;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);

    size_t start = 0;
    int words = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        const std::u16string want = FromUtf8(line);
        const std::string keys = KeysFor(want);
        const std::u16string got = Type(keys);
        ++words;
        if (got == want) {
            ++g_pass;
        } else {
            ++g_fail;
            std::printf("FAIL corpus \"%s\" -> keys \"%s\" -> got \"%s\"\n",
                        line.c_str(), keys.c_str(), ToUtf8(got).c_str());
        }
    }
    std::printf("corpus: %d syllables\n", words);
}

void RunSyllableUnit() {
    // Tone placement is exercised through the engine above; here we only make
    // sure the structural checker rejects what it should.
    struct Case { const char* word; bool strict; bool valid; };
    const Case cases[] = {
        {"ban", true, true},        {"hoa", true, true},
        {"quy", true, true},        {"gia", true, true},
        {"khuyu", true, true},      {"nghieng", true, false},
        {"nghieng", false, true},   // "ie" is on its way to "iê"
        {"hell", false, false},     {"spo", false, false},
        {"strong", false, false},   {"bpt", false, false},
        {"world", false, false},    {"q", false, true},
        {"q", true, false},
    };
    for (const Case& c : cases) {
        const std::u16string bases = FromUtf8(c.word);
        const bool got = telex::IsValidSyllable(bases, telex::kNoTone, c.strict);
        if (got == c.valid) {
            ++g_pass;
        } else {
            ++g_fail;
            std::printf("FAIL syllable \"%s\" (strict=%d) expected %s\n", c.word,
                        c.strict ? 1 : 0, c.valid ? "valid" : "invalid");
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    RunSpecExamples();
    RunTones();
    RunLetterTransforms();
    RunUndo();
    RunFreePosition();
    RunDStroke();
    RunBackspaceThenContinue();
    RunBackspaceAcrossWords();
    RunTonePlacement();
    RunEnglishWords();
    RunCapitalisation();
    RunBackspace();
    RunWordBoundaries();
    RunRevert();
    RunSyllableUnit();
    RunCorpus(argc > 1 ? argv[1] : "tests/corpus.txt");

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
