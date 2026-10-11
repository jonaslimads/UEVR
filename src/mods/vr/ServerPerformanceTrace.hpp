#pragma once
#include <windows.h>
#include <atomic>
#include <chrono>
#include <spdlog/spdlog.h>
#include "ServerPerformanceStats.hpp"

namespace uevr::server_performance {
inline std::atomic_bool detailed_enabled{false};
enum class Stage : size_t {
    EngineTick, EngineOriginal, ViewFamilyDispatch, LinkedOriginal,
    PreRenderCallback, RenderWorker, SlateRoute, SlateRestore,
    RenderTexture, SlateWorker, MonoPoll, Count
};
inline constexpr std::array<const char*, static_cast<size_t>(Stage::Count)> names{
    "engine_tick", "engine_original", "view_family_dispatch", "linked_renderer_original",
    "prerender_callback", "render_worker", "slate_route", "slate_restore",
    "render_texture", "slate_worker", "mono_poll"
};
using TraceClock = std::chrono::steady_clock;
struct Bucket {
    Stats stats{};
    TraceClock::time_point first{}, last_entry{};
    double gap_ms{};
    uint64_t gaps{};
};
inline thread_local std::array<Bucket, static_cast<size_t>(Stage::Count)> buckets{};
inline bool thread_cpu(uint64_t& ticks) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return false;
    const auto value = [](FILETIME time) { return (uint64_t{time.dwHighDateTime} << 32) | time.dwLowDateTime; };
    ticks = value(kernel) + value(user);
    return true;
}
class Scope {
    static inline thread_local Scope* current{};
    Stage stage{};
    Scope* parent{};
    TraceClock::time_point started{};
    uint64_t cpu_started{};
    double child_ms{};
    bool active{}, cpu_valid{};
public:
    explicit Scope(Stage selected) : stage{selected}, active{detailed_enabled.load(std::memory_order_relaxed)} {
        if (!active) return;
        cpu_valid = thread_cpu(cpu_started);
        started = TraceClock::now();
        parent = current; current = this;
        auto& bucket = buckets[static_cast<size_t>(stage)];
        if (bucket.first == TraceClock::time_point{}) {
            bucket.first = started;
            wchar_t* description{};
            char utf8[256]{};
            const auto result = GetThreadDescription(GetCurrentThread(), &description);
            if (SUCCEEDED(result) && description) {
                WideCharToMultiByte(CP_UTF8, 0, description, -1, utf8, sizeof(utf8), nullptr, nullptr);
            }
            if (description) LocalFree(description);
            spdlog::info("[SWGR][stage-thread] stage={} tid={} name={} description_result={}",
                names[static_cast<size_t>(stage)], GetCurrentThreadId(), utf8, result);
            // Exclude one-time thread identification from this stage's own sample.
            cpu_valid = thread_cpu(cpu_started);
            started = TraceClock::now();
        }
        if (bucket.last_entry != TraceClock::time_point{}) {
            bucket.gap_ms += std::chrono::duration<double, std::milli>(started - bucket.last_entry).count();
            ++bucket.gaps;
        }
        bucket.last_entry = started;
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    ~Scope() {
        if (!active) return;
        const auto finished = TraceClock::now();
        uint64_t cpu_finished{};
        const bool valid = thread_cpu(cpu_finished) && cpu_valid && cpu_finished >= cpu_started;
        const auto wall = std::chrono::duration<double, std::milli>(finished - started).count();
        current = parent;
        if (parent) parent->child_ms += wall;
        auto& bucket = buckets[static_cast<size_t>(stage)];
        bucket.stats.add(wall, wall - child_ms, valid ? static_cast<double>(cpu_finished - cpu_started) / 10000.0 : 0, valid);
        const auto seconds = std::chrono::duration<double>(finished - bucket.first).count();
        if (seconds < 5.0) return;
        // Fixed-size per-thread aggregation. No shared diagnostic mutex or stack walk.
        const auto& s = bucket.stats;
        spdlog::info("[SWGR][stage-profiler] stage={} tid={} n={} wall_avg_ms={:.4f} self_avg_ms={:.4f} cpu_avg_ms={:.4f} cpu_n={} wall_max_ms={:.4f} p50_upper_ms={} p95_upper_ms={} over_90hz_budget={} calls_per_s={:.2f} entry_gap_avg_ms={:.3f}",
            names[static_cast<size_t>(stage)], GetCurrentThreadId(), s.count,
            s.wall_ms / static_cast<double>(s.count), s.self_ms / static_cast<double>(s.count),
            s.cpu_count ? s.cpu_ms / static_cast<double>(s.cpu_count) : 0, s.cpu_count,
            s.maximum_ms, s.percentile_upper(0.5), s.percentile_upper(0.95), s.over_budget,
            static_cast<double>(s.count) / seconds, bucket.gaps ? bucket.gap_ms / static_cast<double>(bucket.gaps) : 0);
        bucket.stats = {}; bucket.first = finished; bucket.gap_ms = 0; bucket.gaps = 0;
    }
};
}
