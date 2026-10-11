#include <stdexcept>
#include "ServerPerformancePolicy.hpp"

void require(bool condition) {
    if (!condition) throw std::runtime_error("Launch diagnostic policy failed");
}
int main() {
    using namespace uevr::server_performance;
    require(ini_path(nullptr).empty());
    const auto module_path = ini_path(GetModuleHandleW(nullptr));
    require(module_path.ends_with(L"UEVRServerPerformance.ini"));
    require(!read_enabled(L"", true));
    wchar_t directory[MAX_PATH]{}, file[MAX_PATH]{};
    require(GetTempPathW(MAX_PATH, directory) != 0);
    require(GetTempFileNameW(directory, L"uvp", 0, file) != 0);
    struct Cleanup { const wchar_t* path; ~Cleanup() { DeleteFileW(path); } } cleanup{file};
    require(!read_enabled(file, true));
    for (const auto value : {L"0", L"2", L"invalid", L"true"}) {
        require(WritePrivateProfileStringW(L"Diagnostics", L"FrameTiming", value, file) != FALSE);
        require(!read_enabled(file, true));
    }
    require(WritePrivateProfileStringW(L"Diagnostics", L"FrameTiming", L"1", file) != FALSE);
    require(read_enabled(file, true));
    require(!read_enabled(file, false));
    require(WritePrivateProfileStringW(L"Diagnostics", L"FrameTiming", L"0", file) != FALSE);
    require(!read_enabled(file, true));
}
