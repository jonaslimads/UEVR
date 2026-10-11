#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include "FF7AutoMonoD3D11.hpp"
using Microsoft::WRL::ComPtr;
int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, nullptr, &context))) return 1;
    for (auto format : {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM}) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 128; desc.Height = 64;
        desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = format; desc.Usage = D3D11_USAGE_DEFAULT;
        ComPtr<ID3D11Texture2D> scene;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &scene))) return 2;
        std::vector<unsigned> pixels(128 * 64, 0xff000000);
        for (unsigned y = 0; y < 64; ++y)
            for (unsigned x = 0; x < 64; ++x) pixels[y * 128 + x] = 0xff4080c0;
        context->UpdateSubresource(scene.Get(), 0, nullptr, pixels.data(), 128 * 4, 0);
        uevr::ff7_auto::Sampler sampler;
        const auto wait_for = [&](bool wanted) {
            for (unsigned i = 0; i < 200; ++i) {
                const bool active = sampler.poll(device.Get(), context.Get(), scene.Get(), GetTickCount64());
                // The production sampler never flushes. A test with no Present
                // must submit the queued WARP commands explicitly.
                context->Flush();
                if (active == wanted) return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return false;
        };
        if (!wait_for(true)) return 3;
        std::fill(pixels.begin(), pixels.end(), 0xff4080c0);
        context->UpdateSubresource(scene.Get(), 0, nullptr, pixels.data(), 128 * 4, 0);
        if (!wait_for(false)) return 4;
        sampler.reset();
        if (sampler.poll(device.Get(), context.Get(), scene.Get(), GetTickCount64())) return 5;
        desc.SampleDesc.Count = 2;
        if (uevr::ff7_auto::Sampler::supported(desc)) return 6;
        desc.SampleDesc.Count = 1; desc.Width = 127;
        if (uevr::ff7_auto::Sampler::supported(desc)) return 7;
    }
    std::cout << "FF7 DX11 asynchronous RGBA/BGRA detection, stereo recovery and reset passed\n";
}
