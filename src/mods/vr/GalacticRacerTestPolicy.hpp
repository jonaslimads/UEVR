#pragma once
#include <windows.h>
#include <string>
#include <atomic>

namespace uevr::swgr_test {
struct Policy { int variant; bool redirect_bink; bool allow_manual_mono; bool auto_mono{}; };
constexpr Policy select_policy(int variant) {
    if (variant == 4) return {4, false, true, true};
    if (variant == 2) return {2, false, false};
    if (variant == 3) return {3, false, true};
    return {1, true, false};
}
inline const Policy& policy(HMODULE module) {
    static const Policy selected = [module] {
        wchar_t buffer[32768]{};
        const DWORD length = GetModuleFileNameW(module, buffer, 32768);
        if (!module || !length || length >= 32768) return select_policy(1);
        std::wstring path{buffer, length};
        const auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return select_policy(1);
        path.resize(slash + 1);
        path += L"SWGRFix-test.ini";
        return select_policy(static_cast<int>(GetPrivateProfileIntW(L"SWGRFix", L"Variant", 1, path.c_str())));
    }();
    return selected;
}
inline std::atomic<bool> mono_requested{false};
inline std::atomic<bool> f9_down{false};
}
