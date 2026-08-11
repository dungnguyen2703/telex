// Tier 2 tests: launch the real telex.exe, inject real keystrokes into a real
// EDIT control and read the text back. See docs/TESTING.md.
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_pass = 0;
int g_fail = 0;
const char* g_stage = "startup";

// Injecting global input can wedge if something else grabs the foreground, and
// a test that hangs is worse than one that fails. Bail out loudly instead.
DWORD WINAPI Watchdog(LPVOID) {
    Sleep(120000);
    std::printf("\nTIMEOUT while running: %s\n", g_stage);
    std::fflush(stdout);
    // Leave nothing behind: a surviving telex.exe locks the next build.
    HWND stuck = FindWindowW(L"TelexHiddenWindow", nullptr);
    if (stuck) {
        DWORD pid = 0;
        GetWindowThreadProcessId(stuck, &pid);
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (process) {
            TerminateProcess(process, 0);
            CloseHandle(process);
        }
    }
    ExitProcess(3);
}

HWND g_window = nullptr;
HWND g_edit = nullptr;
std::wstring g_telexPath;
std::wstring g_excludePath;
PROCESS_INFORMATION g_telex = {};

std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    const int need = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), need,
                        nullptr, nullptr);
    return out;
}

std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int need = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), need);
    return out;
}

void Pump(DWORD milliseconds) {
    const DWORD end = GetTickCount() + milliseconds;
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);  // turns key events into WM_CHAR for the EDIT
            DispatchMessageW(&msg);
        }
        if (GetTickCount() >= end) return;
        Sleep(1);
    }
}

void SendVk(WORD vk, bool shift) {
    INPUT in[4] = {};
    int n = 0;
    if (shift) {
        in[n].type = INPUT_KEYBOARD;
        in[n++].ki.wVk = VK_SHIFT;
    }
    in[n].type = INPUT_KEYBOARD;
    in[n++].ki.wVk = vk;
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    in[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    if (shift) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        in[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(static_cast<UINT>(n), in, sizeof(INPUT));
}

// Types an ASCII string. '\b' is Backspace.
void SendKeys(const char* text, DWORD perKeyDelay = 6) {
    for (const char* p = text; *p; ++p) {
        const char c = *p;
        if (c == '\b') {
            SendVk(VK_BACK, false);
        } else if (c == ' ') {
            SendVk(VK_SPACE, false);
        } else if (c >= 'a' && c <= 'z') {
            SendVk(static_cast<WORD>(c - 'a' + 'A'), false);
        } else if (c >= 'A' && c <= 'Z') {
            SendVk(static_cast<WORD>(c), true);
        }
        Pump(perKeyDelay);
    }
    Pump(40);
}

void SendAltZ() {
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_MENU;
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = 'Z';
    in[2].type = INPUT_KEYBOARD;
    in[2].ki.wVk = 'Z';
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD;
    in[3].ki.wVk = VK_MENU;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
    Pump(120);
}

std::wstring EditText() {
    const int length = GetWindowTextLengthW(g_edit);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(g_edit, text.data(), length + 1);
    text.resize(static_cast<size_t>(copied));
    return text;
}

void ClearEdit() {
    SendVk(VK_ESCAPE, false);  // ends the word inside telex
    Pump(20);
    SetWindowTextW(g_edit, L"");
    SetFocus(g_edit);
    Pump(20);
}

void Expect(const char* name, const char* expectedUtf8) {
    const std::wstring got = EditText();
    const std::wstring want = FromUtf8(expectedUtf8);
    if (got == want) {
        ++g_pass;
        std::printf("  ok   %s\n", name);
    } else {
        ++g_fail;
        std::printf("  FAIL %s\n       want \"%s\"\n       got  \"%s\"\n", name,
                    expectedUtf8, ToUtf8(got).c_str());
    }
}

void WriteExcludeFile(const char* contents) {
    HANDLE f = CreateFileW(g_excludePath.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        std::printf("  FAIL cannot write %s\n", ToUtf8(g_excludePath).c_str());
        ++g_fail;
        return;
    }
    DWORD written = 0;
    WriteFile(f, contents, static_cast<DWORD>(strlen(contents)), &written, nullptr);
    CloseHandle(f);
}

bool CreateTestWindow(HINSTANCE instance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = L"TelexE2EWindow";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) return false;

    g_window = CreateWindowExW(WS_EX_TOPMOST, L"TelexE2EWindow", L"telex e2e",
                               WS_OVERLAPPEDWINDOW, 100, 100, 640, 200, nullptr,
                               nullptr, instance, nullptr);
    if (!g_window) return false;
    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                             10, 10, 600, 30, g_window, nullptr, instance, nullptr);
    if (!g_edit) return false;

    ShowWindow(g_window, SW_SHOW);

    // Windows refuses SetForegroundWindow to a process that does not currently
    // own the foreground, so borrow the input state of whoever does.
    for (int attempt = 0; attempt < 20; ++attempt) {
        HWND foreground = GetForegroundWindow();
        const DWORD other = GetWindowThreadProcessId(foreground, nullptr);
        const DWORD self = GetCurrentThreadId();
        if (other && other != self) AttachThreadInput(self, other, TRUE);
        BringWindowToTop(g_window);
        SetForegroundWindow(g_window);
        SetActiveWindow(g_window);
        if (other && other != self) AttachThreadInput(self, other, FALSE);
        SetFocus(g_edit);
        Pump(150);
        if (GetForegroundWindow() == g_window) return true;
    }
    return false;
}

HWND FindTelexWindow() { return FindWindowW(L"TelexHiddenWindow", nullptr); }

bool StartTelex() {
    // A leftover instance from an earlier run would block this one.
    HWND old = FindTelexWindow();
    if (old) {
        PostMessageW(old, WM_CLOSE, 0, 0);
        for (int i = 0; i < 50 && FindTelexWindow(); ++i) Pump(100);
    }

    STARTUPINFOW si = {};
    si.cb = sizeof si;
    std::wstring cmdline = L"\"" + g_telexPath + L"\"";
    if (!CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &si, &g_telex)) {
        std::printf("cannot start %s\n", ToUtf8(g_telexPath).c_str());
        return false;
    }
    for (int i = 0; i < 50; ++i) {
        if (FindTelexWindow()) {
            Pump(300);  // let it install its hooks
            return true;
        }
        Pump(100);
    }
    std::printf("telex.exe did not come up\n");
    return false;
}

// Exiting on its own means the message loop was healthy and the teardown path
// (unhook, remove the tray icon) actually ran.
void StopTelex() {
    HWND hwnd = FindTelexWindow();
    if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    bool clean = false;
    if (g_telex.hProcess) {
        clean = WaitForSingleObject(g_telex.hProcess, 5000) == WAIT_OBJECT_0;
        if (!clean) TerminateProcess(g_telex.hProcess, 0);
        CloseHandle(g_telex.hProcess);
        CloseHandle(g_telex.hThread);
    }
    if (clean) {
        ++g_pass;
        std::printf("  ok   9. shuts down cleanly on WM_CLOSE\n");
    } else {
        ++g_fail;
        std::printf("  FAIL 9. did not shut down on WM_CLOSE\n");
    }
}

// --- scenarios -------------------------------------------------------------

void ScenarioTyping() {
    ClearEdit();
    SendKeys("tieengs vieejt");
    Expect("1. types Vietnamese", "tiếng việt");
}

void ScenarioToggleOff() {
    ClearEdit();
    SendAltZ();
    SendKeys("tieengs vieejt");
    Expect("2. Alt+Z turns it off", "tieengs vieejt");
}

void ScenarioToggleOn() {
    ClearEdit();
    SendAltZ();
    SendKeys("tieengs vieejt");
    Expect("3. Alt+Z turns it back on", "tiếng việt");
}

void ScenarioExcluded() {
    WriteExcludeFile("# test\r\ne2e_test.exe\r\n");
    Pump(1600);  // the app polls the file once a second
    ClearEdit();
    SendKeys("tieengs vieejt");
    Expect("4. excluded app is left alone", "tieengs vieejt");
}

void ScenarioNotExcludedAnyMore() {
    WriteExcludeFile("# test\r\n");
    Pump(1600);
    ClearEdit();
    SendKeys("tieengs vieejt");
    Expect("5. removing the entry takes effect live", "tiếng việt");
}

void ScenarioBackspace() {
    ClearEdit();
    SendKeys("hoas\b\b");
    Expect("6. backspace over a rewritten word", "h");
}

void ScenarioBurst() {
    ClearEdit();
    std::string keys;
    std::string expected;
    for (int i = 0; i < 25; ++i) {
        keys += "tieengs ";
        expected += "tiếng ";
    }
    SendKeys(keys.c_str(), 0);  // no delay between keys at all
    Pump(600);
    Expect("7. 200 keys with no pause", expected.c_str());
}

void ScenarioSoak() {
    ClearEdit();
    const DWORD end = GetTickCount() + 3000;
    while (GetTickCount() < end) {
        SendKeys("dduwowngf ", 1);
    }
    ClearEdit();
    SendKeys("tieengs");
    Expect("8. hook still alive after sustained typing", "tiếng");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IONBF, 0);
    CloseHandle(CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr));
    if (argc < 2) {
        std::printf("usage: e2e_test.exe <path to telex.exe>\n");
        return 2;
    }
    g_telexPath = argv[1];
    const size_t slash = g_telexPath.find_last_of(L'\\');
    g_excludePath = g_telexPath.substr(0, slash + 1) + L"exclude.txt";
    DeleteFileW(g_excludePath.c_str());

    SetProcessDPIAware();
    g_stage = "creating the test window";
    if (!CreateTestWindow(GetModuleHandleW(nullptr))) {
        std::printf("could not create or focus the test window\n");
        return 2;
    }
    g_stage = "starting telex.exe";
    if (!StartTelex()) return 2;

    struct Step { const char* name; void (*run)(); };
    const Step steps[] = {
        {"typing", ScenarioTyping},
        {"toggle off", ScenarioToggleOff},
        {"toggle on", ScenarioToggleOn},
        {"excluded", ScenarioExcluded},
        {"no longer excluded", ScenarioNotExcludedAnyMore},
        {"backspace", ScenarioBackspace},
        {"burst", ScenarioBurst},
        {"soak", ScenarioSoak},
    };
    for (const Step& step : steps) {
        g_stage = step.name;
        step.run();
        std::fflush(stdout);
    }

    StopTelex();
    DeleteFileW(g_excludePath.c_str());
    DestroyWindow(g_window);

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
