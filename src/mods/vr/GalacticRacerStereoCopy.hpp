#pragma once
#include <d3d12.h>
#include <cstdint>

namespace uevr::swgr_copy {
inline constexpr auto shader_read = static_cast<D3D12_RESOURCE_STATES>(
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

constexpr bool use_independent_states(bool validated_external_scene, bool native_packet_ready,
    bool owned_scene_is_source, D3D12_RESOURCE_STATES scene_state) {
    return validated_external_scene && native_packet_ready && owned_scene_is_source && scene_state == shader_read;
}

template<class Commands>
void copy_double_wide(Commands& commands, ID3D12Resource* left, ID3D12Resource* right,
    ID3D12Resource* output, D3D12_BOX left_box, D3D12_BOX right_box, UINT right_x) {
    commands.copy_region(left, output, &left_box, 0, 0, 0,
        shader_read, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands.copy_region(right, output, &right_box, right_x, 0, 0,
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

constexpr bool valid_mono_layout(const D3D12_RESOURCE_DESC& source, const D3D12_RESOURCE_DESC& output,
    UINT half_width, UINT height) {
    const auto bgra = [](DXGI_FORMAT format) {
        return format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || format == DXGI_FORMAT_B8G8R8A8_TYPELESS;
    };
    return half_width != 0 && height != 0 &&
        source.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && output.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        source.Width == uint64_t{half_width} * 2 && output.Width == source.Width &&
        source.Height == height && output.Height == height &&
        source.DepthOrArraySize == 1 && output.DepthOrArraySize == 1 &&
        source.MipLevels == 1 && output.MipLevels == 1 &&
        source.SampleDesc.Count == 1 && output.SampleDesc.Count == 1 && bgra(source.Format) && bgra(output.Format);
}
template<class Commands>
void copy_mono_double_wide(Commands& commands, ID3D12Resource* source, ID3D12Resource* output,
    D3D12_BOX source_box, UINT half_width) {
    commands.copy_region(source, output, &source_box, 0, 0, 0, shader_read, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands.copy_region(source, output, &source_box, half_width, 0, 0, shader_read, D3D12_RESOURCE_STATE_RENDER_TARGET);
}
}
