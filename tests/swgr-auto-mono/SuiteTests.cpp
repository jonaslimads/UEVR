#include <array>
#include <stdexcept>
#include <cstdlib>
#include "GalacticRacerTestPolicy.hpp"
#include "GalacticRacerStereoCopy.hpp"
#include "GalacticRacerAutoMonoDecision.hpp"
static_assert(uevr::swgr_test::select_policy(1).redirect_bink);
static_assert(!uevr::swgr_test::select_policy(2).redirect_bink && !uevr::swgr_test::select_policy(2).allow_manual_mono);
static_assert(!uevr::swgr_test::select_policy(3).redirect_bink && uevr::swgr_test::select_policy(3).allow_manual_mono);
static_assert(uevr::swgr_test::select_policy(999).variant == 1);
static_assert(uevr::swgr_test::select_policy(4).auto_mono && !uevr::swgr_test::select_policy(4).redirect_bink);
static_assert(!uevr::swgr_test::select_policy(3).auto_mono);
struct Call { ID3D12Resource* src{}; ID3D12Resource* dst{}; D3D12_BOX box{}; UINT x{}; D3D12_RESOURCE_STATES state{}; };
struct Commands {
    std::array<Call, 2> calls{}; size_t count{};
    void copy_region(ID3D12Resource* src, ID3D12Resource* dst, D3D12_BOX* box, UINT x, UINT y, UINT z,
        D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES dst_state) {
        if (y || z || dst_state != D3D12_RESOURCE_STATE_RENDER_TARGET) throw std::runtime_error("Wrong destination");
        calls.at(count++) = {src, dst, *box, x, state};
    }
};
void require(bool value) { if (!value) throw std::runtime_error("Unsafe mono layout or copy"); }
int main(int argc, char** argv) {
    if (argc == 2) {
        const int variant = std::atoi(argv[1]);
        const auto module = GetModuleHandleW(nullptr);
        wchar_t name[32768]{};
        const auto length = GetModuleFileNameW(module, name, 32768);
        require(length != 0 && length < 32768);
        std::wstring path{name, length};
        path.resize(path.find_last_of(L"\\/") + 1);
        path += L"SWGRFix-test.ini";
        require(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES);
        require(WritePrivateProfileStringW(L"SWGRFix", L"Variant", std::to_wstring(variant).c_str(), path.c_str()) != FALSE);
        const auto selected = uevr::swgr_test::policy(module);
        const auto expected = uevr::swgr_test::select_policy(variant);
        const bool passed = selected.variant == expected.variant && selected.redirect_bink == expected.redirect_bink && selected.allow_manual_mono == expected.allow_manual_mono && selected.auto_mono == expected.auto_mono;
        // The runtime policy must remain fixed until the process is restarted.
        require(WritePrivateProfileStringW(L"SWGRFix", L"Variant", L"1", path.c_str()) != FALSE);
        const bool fixed = uevr::swgr_test::policy(module).variant == expected.variant;
        require(DeleteFileW(path.c_str()) != FALSE);
        require(passed && fixed);
        return 0;
    }
    // Start stereo; one bad observation cannot trigger mono. Both-eye fades
    // cannot activate mono; bright right-eye evidence restores stereo.
    uevr::swgr_auto::Decision automatic;
    require(!automatic.active(100));
    require(!automatic.update(20, 0, 100));
    require(automatic.update(20, 0, 140));
    require(automatic.active(160));
    require(automatic.update(20, 20, 180));
    require(!automatic.update(20, 20, 220));
    require(!automatic.update(0, 0, 260));
    require(!automatic.update(0, 0, 300));
    require(!automatic.update(0, 0, 340));
    require(!automatic.update(20, 0, 380));
    require(automatic.update(20, 0, 420));
    require(!automatic.active(921));
    require(!automatic.update(20, 0, 1000));
    automatic.reset();
    require(!automatic.mono && !automatic.active(1020));
    D3D12_RESOURCE_DESC source{};
    source.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    source.Width = 6528; source.Height = 3264; source.MipLevels = 1; source.DepthOrArraySize = 1;
    source.SampleDesc.Count = 1; source.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    auto output = source; output.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    require(uevr::swgr_copy::valid_mono_layout(source, output, 3264, 3264));
    auto invalid = output; invalid.Width = 3264;
    require(!uevr::swgr_copy::valid_mono_layout(source, invalid, 3264, 3264));
    invalid = output; invalid.DepthOrArraySize = 2;
    require(!uevr::swgr_copy::valid_mono_layout(source, invalid, 3264, 3264));
    invalid = output; invalid.SampleDesc.Count = 4;
    require(!uevr::swgr_copy::valid_mono_layout(source, invalid, 3264, 3264));
    invalid = output; invalid.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    require(!uevr::swgr_copy::valid_mono_layout(source, invalid, 3264, 3264));
    require(!uevr::swgr_copy::valid_mono_layout(source, output, 0, 3264));
    require(!uevr::swgr_copy::valid_mono_layout(source, output, 3264, 0));
    Commands commands;
    auto src = reinterpret_cast<ID3D12Resource*>(uintptr_t{1});
    auto dst = reinterpret_cast<ID3D12Resource*>(uintptr_t{2});
    uevr::swgr_copy::copy_mono_double_wide(commands, src, dst, {0,0,0,3264,3264,1}, 3264);
    require(commands.count == 2);
    const auto& a = commands.calls[0]; const auto& b = commands.calls[1];
    require(a.src == src && b.src == src && a.dst == dst && b.dst == dst);
    require(a.x == 0 && b.x == 3264);
    require(a.box.left == b.box.left && a.box.right == b.box.right && a.box.bottom == b.box.bottom);
    require(a.state == uevr::swgr_copy::shader_read && b.state == uevr::swgr_copy::shader_read);
    return 0;
}
