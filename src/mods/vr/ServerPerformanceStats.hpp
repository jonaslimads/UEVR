#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace uevr::server_performance {
struct Stats {
    static constexpr std::array<double, 12> bounds{0.05, 0.1, 0.25, 0.5, 1, 2, 4, 8, 11.111, 16.667, 33.333, 66.667};
    std::array<uint64_t, bounds.size() + 1> histogram{};
    uint64_t count{}, cpu_count{}, over_budget{};
    double wall_ms{}, self_ms{}, cpu_ms{}, maximum_ms{};
    void add(double wall, double self, double cpu, bool cpu_valid) {
        wall = (std::max)(0.0, wall);
        ++count; wall_ms += wall;
        self_ms += (std::clamp)(self, 0.0, wall);
        maximum_ms = (std::max)(maximum_ms, wall);
        if (wall > 1000.0 / 90.0) ++over_budget;
        if (cpu_valid) { cpu_ms += (std::max)(0.0, cpu); ++cpu_count; }
        size_t bucket{};
        while (bucket < bounds.size() && wall > bounds[bucket]) ++bucket;
        ++histogram[bucket];
    }
    double percentile_upper(double fraction) const {
        if (!count) return 0;
        const auto rank = (std::max)(uint64_t{1}, static_cast<uint64_t>(fraction * static_cast<double>(count) + 0.999999));
        uint64_t cumulative{};
        for (size_t i = 0; i < histogram.size(); ++i) {
            cumulative += histogram[i];
            if (cumulative >= rank) return i < bounds.size() ? bounds[i] : std::numeric_limits<double>::infinity();
        }
        return std::numeric_limits<double>::infinity();
    }
};
}
