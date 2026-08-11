// The exclusion list: process names, one per line, in exclude.txt next to the
// exe. Reloaded when the file changes; the result is a single flag the keyboard
// hook can read for free.
#include "app.h"

#include <psapi.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

namespace app {
namespace {

// Only ever touched by the worker thread.
std::vector<std::wstring> g_excluded;
FILETIME g_lastWrite = {};
bool g_loaded = false;

// The one value the keyboard hook reads.
std::atomic<bool> g_isExcluded{false};

HANDLE g_worker = nullptr;
HANDLE g_wakeUp = nullptr;   // foreground changed, look again now
HANDLE g_quit = nullptr;

std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return s;
}

std::wstring ExeDirectory() {
    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path, n);
    const size_t slash = s.find_last_of(L'\\');
    return slash == std::wstring::npos ? std::wstring() : s.substr(0, slash + 1);
}

std::wstring BaseName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L'\\');
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

void ParseList(const std::string& utf8) {
    g_excluded.clear();
    size_t start = 0;
    while (start <= utf8.size()) {
        size_t end = utf8.find('\n', start);
        if (end == std::string::npos) end = utf8.size();
        std::string line = utf8.substr(start, end - start);
        start = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
                                 line.back() == '\t')) {
            line.pop_back();
        }
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line = line.substr(first);
        if (line.empty() || line[0] == '#') continue;
        // The rest of the line after the program name is free for a description,
        // which is how the template file labels its entries. Cut after ".exe"
        // rather than at the first space: plenty of games ship as
        // "League of Legends.exe".
        const std::string lower = ToLowerAscii(line);
        const size_t exe = lower.find(".exe");
        if (exe != std::string::npos) {
            line = line.substr(0, exe + 4);
        } else {
            const size_t space = line.find_first_of(" \t");
            if (space != std::string::npos) line = line.substr(0, space);
        }

        const int need = MultiByteToWideChar(CP_UTF8, 0, line.c_str(),
                                             static_cast<int>(line.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(need), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()),
                            wide.data(), need);
        g_excluded.push_back(ToLower(wide));
    }
}

// Returns true when the list changed (or was loaded for the first time).
bool ReloadIfChanged() {
    WIN32_FILE_ATTRIBUTE_DATA attr = {};
    if (!GetFileAttributesExW(ExclusionPath().c_str(), GetFileExInfoStandard, &attr)) {
        if (!g_loaded || !g_excluded.empty()) {
            g_excluded.clear();  // no file means no exclusions
            g_loaded = true;
            g_lastWrite = FILETIME{};
            return true;
        }
        return false;
    }
    if (g_loaded && CompareFileTime(&attr.ftLastWriteTime, &g_lastWrite) == 0) {
        return false;
    }
    g_lastWrite = attr.ftLastWriteTime;
    g_loaded = true;

    HANDLE file = CreateFileW(ExclusionPath().c_str(), GENERIC_READ, FILE_SHARE_READ |
                              FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    std::string content;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(file, buffer, sizeof buffer, &read, nullptr) && read > 0) {
        content.append(buffer, read);
    }
    CloseHandle(file);
    ParseList(content);
    return true;
}

std::wstring ForegroundProcessName() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return std::wstring();
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return std::wstring();

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return std::wstring();
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        name = ToLower(BaseName(std::wstring(path, size)));
    }
    CloseHandle(process);
    return name;
}

// Reading the file and asking for the foreground process name are both slow
// enough to matter: exclude.txt may sit in a cloud-synced folder, and this used
// to run on the same thread as the keyboard hook. Windows drops keystrokes for a
// hook that answers late, which showed up as the occasional letter losing its
// tone mark. All of it now happens here, off the hook thread.
DWORD WINAPI WorkerMain(LPVOID) {
    HANDLE waits[2] = {g_quit, g_wakeUp};
    for (;;) {
        ReloadIfChanged();
        bool excluded = false;
        if (!g_excluded.empty()) {
            const std::wstring name = ForegroundProcessName();
            excluded = !name.empty() &&
                       std::find(g_excluded.begin(), g_excluded.end(), name) !=
                           g_excluded.end();
        }
        g_isExcluded.store(excluded, std::memory_order_relaxed);

        const DWORD result = WaitForMultipleObjects(2, waits, FALSE, 1000);
        if (result == WAIT_OBJECT_0) return 0;  // quit
    }
}

}  // namespace

std::wstring ExclusionPath() { return ExeDirectory() + L"exclude.txt"; }

bool StartExclusionWorker() {
    g_quit = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_wakeUp = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_quit || !g_wakeUp) return false;
    g_worker = CreateThread(nullptr, 0, WorkerMain, nullptr, 0, nullptr);
    return g_worker != nullptr;
}

void StopExclusionWorker() {
    if (!g_worker) return;
    SetEvent(g_quit);
    WaitForSingleObject(g_worker, 2000);
    CloseHandle(g_worker);
    CloseHandle(g_quit);
    CloseHandle(g_wakeUp);
    g_worker = nullptr;
}

void RefreshExclusion() {
    if (g_wakeUp) SetEvent(g_wakeUp);
}

bool IsExcluded() { return g_isExcluded.load(std::memory_order_relaxed); }

void OpenExclusionFile() {
    const std::wstring path = ExclusionPath();
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            // Pre-filled with the usual suspects so nobody has to go hunting for
            // exe names in Task Manager: delete the # in front of a line to
            // switch telex off inside that program.
            static const char kTemplate[] =
                "# telex - exclusion list\r\n"
                "#\r\n"
                "# While one of these programs is the active window, telex stays\r\n"
                "# out of the way and your keys come out exactly as typed.\r\n"
                "#\r\n"
                "# Remove the # in front of a line to turn it on. Saving the file\r\n"
                "# takes effect at once - no restart. One program name per line;\r\n"
                "# anything after the name on the same line is ignored.\r\n"
                "\r\n"
                "# --- editors and IDEs ---------------------------------------\r\n"
                "# code.exe                 Visual Studio Code\r\n"
                "# devenv.exe               Visual Studio\r\n"
                "# idea64.exe               IntelliJ IDEA\r\n"
                "# pycharm64.exe            PyCharm\r\n"
                "# webstorm64.exe           WebStorm\r\n"
                "# rider64.exe              Rider\r\n"
                "# sublime_text.exe         Sublime Text\r\n"
                "# notepad++.exe            Notepad++\r\n"
                "# cursor.exe               Cursor\r\n"
                "# windsurf.exe             Windsurf\r\n"
                "\r\n"
                "# --- terminals -----------------------------------------------\r\n"
                "# WindowsTerminal.exe      Windows Terminal\r\n"
                "# cmd.exe                  Command Prompt\r\n"
                "# powershell.exe           Windows PowerShell\r\n"
                "# pwsh.exe                 PowerShell 7\r\n"
                "# mintty.exe               Git Bash\r\n"
                "# putty.exe                PuTTY\r\n"
                "# WindowsSandbox.exe       Windows Sandbox\r\n"
                "\r\n"
                "# --- games ---------------------------------------------------\r\n"
                "# LeagueClient.exe         League of Legends client\r\n"
                "# League of Legends.exe    League of Legends\r\n"
                "# VALORANT-Win64-Shipping.exe   Valorant\r\n"
                "# csgo.exe                 Counter-Strike\r\n"
                "# cs2.exe                  Counter-Strike 2\r\n"
                "# GenshinImpact.exe        Genshin Impact\r\n"
                "# steam.exe                Steam\r\n"
                "\r\n"
                "# --- design and 3D -------------------------------------------\r\n"
                "# Photoshop.exe            Adobe Photoshop\r\n"
                "# Illustrator.exe          Adobe Illustrator\r\n"
                "# AfterFX.exe              After Effects\r\n"
                "# Adobe Premiere Pro.exe   Premiere Pro\r\n"
                "# Figma.exe                Figma\r\n"
                "# blender.exe              Blender\r\n"
                "# AutoCAD.exe              AutoCAD\r\n"
                "\r\n"
                "# --- browsers ------------------------------------------------\r\n"
                "# chrome.exe               Google Chrome\r\n"
                "# msedge.exe               Microsoft Edge\r\n"
                "# firefox.exe              Mozilla Firefox\r\n"
                "# brave.exe                Brave\r\n"
                "# opera.exe                Opera\r\n"
                "# zen.exe                  Zen Browser\r\n"
                "\r\n"
                "# --- chat and mail -------------------------------------------\r\n"
                "# Discord.exe              Discord\r\n"
                "# slack.exe                Slack\r\n"
                "# Telegram.exe             Telegram\r\n"
                "# Zalo.exe                 Zalo\r\n"
                "# Messenger.exe            Messenger\r\n"
                "# Teams.exe                Microsoft Teams\r\n"
                "# ms-teams.exe             Microsoft Teams (new)\r\n"
                "# olk.exe                  Outlook (new)\r\n"
                "# OUTLOOK.EXE              Outlook (classic)\r\n"
                "# Skype.exe                Skype\r\n"
                "# Zoom.exe                 Zoom\r\n"
                "\r\n"
                "# --- office and notes ----------------------------------------\r\n"
                "# WINWORD.EXE              Word\r\n"
                "# EXCEL.EXE                Excel\r\n"
                "# POWERPNT.EXE             PowerPoint\r\n"
                "# notepad.exe              Notepad\r\n"
                "# Notion.exe               Notion\r\n"
                "# Obsidian.exe             Obsidian\r\n"
                "\r\n"
                "# --- other ---------------------------------------------------\r\n"
                "# explorer.exe             File Explorer and the Start menu\r\n"
                "# Spotify.exe              Spotify\r\n"
                "# vlc.exe                  VLC\r\n"
                "# AnyDesk.exe              AnyDesk\r\n"
                "# TeamViewer.exe           TeamViewer\r\n"
                "\r\n"
                "# Not in the list? Right-click the taskbar, open Task Manager,\r\n"
                "# find the program under Details, and use the name shown there.\r\n";
            DWORD written = 0;
            WriteFile(file, kTemplate, sizeof kTemplate - 1, &written, nullptr);
            CloseHandle(file);
        }
    }
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace app
