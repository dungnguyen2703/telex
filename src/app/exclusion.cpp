// The exclusion list: process names, one per line, in exclude.txt next to the
// exe. Reloaded when the file changes; the result is a single flag the keyboard
// hook can read for free.
#include "app.h"

#include <psapi.h>

#include <algorithm>
#include <string>
#include <vector>

namespace app {
namespace {

std::vector<std::wstring> g_excluded;
FILETIME g_lastWrite = {};
bool g_loaded = false;
bool g_isExcluded = false;

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

}  // namespace

std::wstring ExclusionPath() { return ExeDirectory() + L"exclude.txt"; }

void RefreshExclusion() {
    ReloadIfChanged();
    if (g_excluded.empty()) {
        g_isExcluded = false;
        return;
    }
    const std::wstring name = ForegroundProcessName();
    g_isExcluded = !name.empty() &&
                   std::find(g_excluded.begin(), g_excluded.end(), name) !=
                       g_excluded.end();
}

bool IsExcluded() { return g_isExcluded; }

void OpenExclusionFile() {
    const std::wstring path = ExclusionPath();
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            static const char kTemplate[] =
                "# One program name per line. While one of these is the active\r\n"
                "# window, telex stays out of the way.\r\n"
                "# Lines starting with # are ignored. Saving takes effect at once.\r\n"
                "#\r\n"
                "# code.exe\r\n"
                "# Photoshop.exe\r\n";
            DWORD written = 0;
            WriteFile(file, kTemplate, sizeof kTemplate - 1, &written, nullptr);
            CloseHandle(file);
        }
    }
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace app
