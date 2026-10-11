#include <stdexcept>
#include "ServerPerformancePolicy.hpp"
#include "ServerPerformanceTrace.hpp"
#include <thread>
#include <cmath>

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
    require(!read_detailed_enabled(file, true));
    for (const auto value : {L"0", L"2", L"invalid", L"true"}) {
        require(WritePrivateProfileStringW(L"Diagnostics", L"FrameTiming", value, file) != FALSE);
        require(!read_enabled(file, true));
    }
    require(WritePrivateProfileStringW(L"Diagnostics", L"FrameTiming", L"1", file) != FALSE);
    require(read_enabled(file, true));
    require(!read_enabled(file, false));
    require(WritePrivateProfileStringW(L"Diagnostics", L"FrameTiming", L"0", file) != FALSE);
    require(!read_enabled(file, true));
    require(WritePrivateProfileStringW(L"Diagnostics", L"RenderTrace", L"1", file) != FALSE);
    require(read_detailed_enabled(file, true));
    require(!read_detailed_enabled(file, false));
    require(!read_detailed_enabled(L"", true));
    require(WritePrivateProfileStringW(L"Diagnostics", L"RenderTrace", L"0", file) != FALSE);
    require(!read_detailed_enabled(file, true));

    Stats stats;
    for (int i = 0; i < 95; ++i) stats.add(1, 0.25, 0.5, true);
    for (int i = 0; i < 5; ++i) stats.add(20, 5, 0, false);
    require(stats.count == 100 && stats.cpu_count == 95 && stats.over_budget == 5);
    require(stats.percentile_upper(0.5) == 1 && stats.percentile_upper(0.95) == 1);
    require(stats.percentile_upper(1) == 33.333 && stats.maximum_ms == 20);
    require(stats.wall_ms == 195 && stats.self_ms == 48.75 && stats.cpu_ms == 47.5);
    Stats overflow;
    overflow.add(100, 200, 0, false);
    require(overflow.self_ms == 100 && std::isinf(overflow.percentile_upper(0.95)));

    buckets = {};
    detailed_enabled.store(false);
    { Scope disabled{Stage::ViewFamilyDispatch}; }
    require(buckets[static_cast<size_t>(Stage::ViewFamilyDispatch)].stats.count == 0);
    detailed_enabled.store(true);
    {
        Scope outer{Stage::ViewFamilyDispatch};
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        { Scope inner{Stage::LinkedOriginal}; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    }
    const auto& outer = buckets[static_cast<size_t>(Stage::ViewFamilyDispatch)].stats;
    const auto& inner = buckets[static_cast<size_t>(Stage::LinkedOriginal)].stats;
    require(outer.count == 1 && inner.count == 1);
    require(outer.wall_ms >= inner.wall_ms && outer.self_ms < outer.wall_ms);
    require(outer.self_ms >= 0 && outer.self_ms <= outer.wall_ms);
    bool isolated{};
    std::thread worker{[&] {
        { Scope other{Stage::ViewFamilyDispatch}; }
        isolated = buckets[static_cast<size_t>(Stage::ViewFamilyDispatch)].stats.count == 1;
    }};
    worker.join();
    require(isolated && outer.count == 1);
    require(outer.cpu_count == 1 && inner.cpu_count == 1);
    buckets[static_cast<size_t>(Stage::ViewFamilyDispatch)].first = TraceClock::now() - std::chrono::seconds(6);
    { Scope emit_summary{Stage::ViewFamilyDispatch}; }
    require(buckets[static_cast<size_t>(Stage::ViewFamilyDispatch)].stats.count == 0);
    detailed_enabled.store(false);
}
