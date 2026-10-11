#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <spdlog/spdlog.h>
#include "utility/Logging.hpp"
#include "d3d12/CommandContext.hpp"
#include "GalacticRacerAutoMonoDecision.hpp"
namespace uevr::swgr_auto {
class Sampler {
    static constexpr unsigned grid = 6, points = grid * grid;
    static constexpr uint64_t stride = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    // Destruction retires commands before releasing all referenced resources.
    Microsoft::WRL::ComPtr<ID3D12Resource> readback{}, held_left{}, held_right{};
    d3d12::CommandContext commands{};
    Decision decision{};
    uint64_t generation{}, sample_ms{}, last_submit_ms{};
    bool pending{};
    static uint64_t now() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    static bool valid(const D3D12_RESOURCE_DESC& d) {
        return d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.DepthOrArraySize == 1 &&
            d.MipLevels == 1 && d.SampleDesc.Count == 1 && d.Width >= grid && d.Height >= grid &&
            (d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
             d.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS);
    }
public:
    void reset() {
        commands.reset();
        readback.Reset(); held_left.Reset(); held_right.Reset();
        decision.reset(); generation = sample_ms = last_submit_ms = 0; pending = false;
    }
    bool poll(ID3D12Device* device, ID3D12Resource* left, ID3D12Resource* right,
        uint32_t left_width, uint64_t current_generation, D3D12_RESOURCE_STATES left_state) {
        const auto clock = now();
        if (!device || !left || !right) { decision.reset(); return false; }
        const auto ld = left->GetDesc(), rd = right->GetDesc();
        if (!valid(ld) || !valid(rd) || left_width < grid || left_width > ld.Width ||
            left_width != rd.Width || ld.Height != rd.Height) { decision.reset(); return false; }
        if (pending) {
            // No render-thread wait: only read a completed GPU transfer.
            if (commands.poisoned || !commands.try_wait()) return decision.active(clock);
            pending = false;
            const bool same = held_left.Get() == left && held_right.Get() == right && generation == current_generation;
            if (same && clock - sample_ms <= 500) {
                void* mapped{};
                const D3D12_RANGE read_range{0, static_cast<SIZE_T>(stride * points * 2)};
                if (SUCCEEDED(readback->Map(0, &read_range, &mapped))) {
                    unsigned lit[2]{};
                    const auto bytes = static_cast<const uint8_t*>(mapped);
                    for (unsigned eye = 0; eye != 2; ++eye) {
                        for (unsigned i = 0; i != points; ++i) {
                            const auto pixel = bytes + stride * (eye * points + i);
                            if ((std::max)({pixel[0], pixel[1], pixel[2]}) > 12) ++lit[eye];
                        }
                    }
                    const D3D12_RANGE no_write{0, 0}; readback->Unmap(0, &no_write);
                    const auto previous = decision.mono;
                    decision.update(lit[0], lit[1], clock);
                    if (previous != decision.mono) SPDLOG_INFO("[SWGRAutoMono][Transition] mono={} left_lit={}/36 right_lit={}/36 generation={}", decision.mono, lit[0], lit[1], generation);
                    SPDLOG_INFO_EVERY_N_SEC(1, "[SWGRAutoMono][Sample] left_lit={}/36 right_lit={}/36 mono={} age_ms={}", lit[0], lit[1], decision.mono, clock - sample_ms);
                } else { decision.reset(); }
            } else { decision.reset(); }
            held_left.Reset(); held_right.Reset();
        }
        if (generation && generation != current_generation) decision.reset();
        if (clock - last_submit_ms < 40) return decision.active(clock);
        if (!readback) {
            const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
            D3D12_RESOURCE_DESC buffer{};
            buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width = stride * points * 2;
            buffer.Height = 1; buffer.DepthOrArraySize = 1; buffer.MipLevels = 1;
            buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return false;
            if (!commands.setup(L"SWGR asynchronous eye samples")) { readback.Reset(); return false; }
        }
        if (!commands.ready() || !commands.try_wait()) return decision.active(clock);
        held_left = left; held_right = right; generation = current_generation;
        for (unsigned eye = 0; eye != 2; ++eye) {
            auto source = eye == 0 ? left : right;
            const auto state = eye == 0 ? left_state : D3D12_RESOURCE_STATE_RENDER_TARGET;
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition = {source, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state, D3D12_RESOURCE_STATE_COPY_SOURCE};
            commands.cmd_list->ResourceBarrier(1, &barrier);
            D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = source; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            for (unsigned i = 0; i != points; ++i) {
                // Interior grid avoids border black bars; source is pre-UI scene.
                const UINT x = static_cast<UINT>((uint64_t(left_width) * (2 * (i % grid) + 1)) / (2 * grid));
                const UINT y = static_cast<UINT>((uint64_t(ld.Height) * (2 * (i / grid) + 1)) / (2 * grid));
                const D3D12_BOX box{x, y, 0, x + 1, y + 1, 1};
                D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                dst.PlacedFootprint.Offset = stride * (eye * points + i);
                dst.PlacedFootprint.Footprint = {DXGI_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT};
                commands.cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
            }
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            commands.cmd_list->ResourceBarrier(1, &barrier);
        }
        commands.has_commands = true; commands.execute();
        pending = commands.waiting_for_fence && !commands.poisoned;
        sample_ms = last_submit_ms = clock;
        return decision.active(clock);
    }
};
}
