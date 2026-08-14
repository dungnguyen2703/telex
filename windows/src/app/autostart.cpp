// "Start with Windows": a single REG_SZ value under HKCU Run, pointing at the
// running exe. No installer, no scheduled task, no elevation.
#include "app.h"

namespace app {
namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"telex";

}  // namespace

bool IsAutoStartEnabled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    LONG result = RegQueryValueExW(key, kRunValueName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void SetAutoStartEnabled(bool on) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key) !=
        ERROR_SUCCESS) {
        return;
    }
    if (on) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        // Quoted so a path containing spaces still parses as one token.
        std::wstring quoted = L"\"" + std::wstring(path) + L"\"";
        RegSetValueExW(key, kRunValueName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(quoted.c_str()),
                       static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kRunValueName);
    }
    RegCloseKey(key);
}

}  // namespace app
