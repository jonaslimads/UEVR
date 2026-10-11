#pragma once
#include <windows.h>
#include <string>

namespace uevr::server_performance {
inline std::wstring ini_path(HMODULE module) {
    wchar_t buffer[32768]{};
    if (!module) return {};
    const auto length = GetModuleFileNameW(module, buffer, 32768);
    if (!length || length >= 32768) return {};
    std::wstring path{buffer, length};
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    path.resize(slash + 1);
    return path + L"UEVRServerPerformance.ini";
}
inline bool read_enabled(const std::wstring& path, bool target_game) {
    if (!target_game || path.empty()) return false;
    // Explicit launch-level opt-in, independent of frontend profile saves.
    // Only the exact numeric value 1 enables it; missing files remain off.
    return GetPrivateProfileIntW(L"Diagnostics", L"FrameTiming", 0, path.c_str()) == 1;
}
}
