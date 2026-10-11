#pragma once
#include <atomic>
#include <cstdint>

namespace uevr::swgr_diag {
// Cumulative packet counters describe the viewport callback, not actual movie pixels.
inline std::atomic<uint64_t> packets{}, redirected{}, rejected{};
inline std::atomic<uint32_t> width{}, height{}, hdr{};
inline std::atomic<uintptr_t> packet_texture{};
inline std::atomic<bool> marker_down{};
inline std::atomic<uint32_t> marker{};
}
