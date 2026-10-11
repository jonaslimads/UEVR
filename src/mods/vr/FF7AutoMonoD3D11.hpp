#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <filesystem>
#include "GalacticRacerAutoMonoDecision.hpp"

namespace uevr::ff7_auto {
inline bool executable_matches() {
    static const bool matches = [] {
        wchar_t path[32768]{};
        const auto length = GetModuleFileNameW(nullptr, path, 32768);
        return length != 0 && length < 32768 &&
            _wcsicmp(std::filesystem::path(path).filename().c_str(), L"ff7remake_.exe") == 0;
    }();
    return matches;
}

// Sample a sparse interior grid before UI composition. Never flush or wait for
// GPU completion. Only a tiny 72-pixel staging texture is mapped.
class Sampler {
public:
    void reset() {
        m_staging.Reset(); m_query.Reset(); m_device.Reset();
        m_pending = false; m_last_request = 0; m_desc = {}; m_decision.reset();
        m_samples = 0; m_left_lit = m_right_lit = 0;
    }
    bool poll(ID3D11Device* device, ID3D11DeviceContext* context,
              ID3D11Texture2D* scene, uint64_t now) {
        D3D11_TEXTURE2D_DESC desc{};
        scene->GetDesc(&desc);
        if (!supported(desc)) { reset(); return false; }
        if (device != m_device.Get() || desc.Width != m_desc.Width ||
            desc.Height != m_desc.Height || desc.Format != m_desc.Format) {
            reset();
            D3D11_TEXTURE2D_DESC staging{};
            staging.Width = 72; staging.Height = 1;
            staging.MipLevels = staging.ArraySize = staging.SampleDesc.Count = 1;
            staging.Format = desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS ? DXGI_FORMAT_B8G8R8A8_UNORM
                : desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ? DXGI_FORMAT_R8G8B8A8_UNORM : desc.Format;
            staging.Usage = D3D11_USAGE_STAGING;
            staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            D3D11_QUERY_DESC query{D3D11_QUERY_EVENT, 0};
            if (FAILED(device->CreateTexture2D(&staging, nullptr, &m_staging)) ||
                FAILED(device->CreateQuery(&query, &m_query))) { reset(); return false; }
            m_device = device; m_desc = desc;
        }
        if (m_pending) {
            if (now - m_last_request > 500) { reset(); return false; }
            const auto ready = context->GetData(m_query.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (FAILED(ready)) { reset(); return false; }
            if (ready == S_OK) {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                const auto result = context->Map(m_staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
                if (SUCCEEDED(result)) {
                    const auto pixels = static_cast<const unsigned char*>(mapped.pData);
                    unsigned lit[2]{};
                    for (unsigned i = 0; i < 72; ++i) {
                        const auto p = pixels + i * 4;
                        if (p[0] > 12 || p[1] > 12 || p[2] > 12) ++lit[i / 36];
                    }
                    context->Unmap(m_staging.Get(), 0);
                    ++m_samples; m_left_lit = lit[0]; m_right_lit = lit[1];
                    m_decision.update(lit[0], lit[1], now);
                    m_pending = false;
                } else if (result != DXGI_ERROR_WAS_STILL_DRAWING) { reset(); return false; }
            }
        }
        if (!m_pending && (m_last_request == 0 || now - m_last_request >= 40)) {
            for (unsigned eye = 0; eye < 2; ++eye) {
                for (unsigned y = 0; y < 6; ++y) {
                    for (unsigned x = 0; x < 6; ++x) {
                        const auto sx = eye * (desc.Width / 2) + (x + 1) * (desc.Width / 2) / 7;
                        const auto sy = (y + 1) * desc.Height / 7;
                        const D3D11_BOX box{sx, sy, 0, sx + 1, sy + 1, 1};
                        context->CopySubresourceRegion(m_staging.Get(), 0, eye * 36 + y * 6 + x, 0, 0, scene, 0, &box);
                    }
                }
            }
            context->End(m_query.Get()); m_pending = true; m_last_request = now;
        }
        return m_decision.active(now);
    }
    static bool supported(const D3D11_TEXTURE2D_DESC& d) {
        const bool format = d.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS ||
            d.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
            d.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
            d.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        return format && d.Width >= 14 && d.Width % 2 == 0 && d.Height >= 7 &&
            d.ArraySize == 1 && d.MipLevels == 1 && d.SampleDesc.Count == 1;
    }
    uint64_t samples() const { return m_samples; }
    unsigned left_lit() const { return m_left_lit; }
    unsigned right_lit() const { return m_right_lit; }
    bool pending() const { return m_pending; }
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_staging{};
    Microsoft::WRL::ComPtr<ID3D11Query> m_query{};
    Microsoft::WRL::ComPtr<ID3D11Device> m_device{};
    D3D11_TEXTURE2D_DESC m_desc{};
    swgr_auto::Decision m_decision{};
    bool m_pending{};
    uint64_t m_last_request{};
    uint64_t m_samples{};
    unsigned m_left_lit{}, m_right_lit{};
};
}
