# FF7 Remake DX11 cutscene fallback

This branch extends the published SWGR diagnostic backend without changing its D3D12 fallback or timing instrumentation. FF7 uses the original praydog FF7R plugin at revision `d5815b0d9491ba2f1c2f9596fd02fbb89755ec21` for UI and lighting. That plugin requires DX11 and Windowed mode; do not enable Native Stereo Fix or replace it with the SWGR profile.

The new DX11 path is gated to the exact `ff7remake_.exe` filename, OpenXR, native double-wide rendering, matching HMD/source extents, single-sample/single-mip/single-array RGBA8/BGRA8 textures, and no AFR, texture-array, extreme-compatibility or 2D screen mode. Other games and unsupported FF7 configurations retain existing submission.

`FF7AutoMonoD3D11.hpp` copies 36 interior pixels per eye into one 72x1 staging texture. An event query is polled with `D3D11_ASYNC_GETDATA_DONOTFLUSH`; mapping uses `D3D11_MAP_FLAG_DO_NOT_WAIT`. There is no production flush, wait, full-resolution readback, new worker or engine patch. Resource/format/extent changes release samples; failed readback returns normal rendering. Evidence expires after 500 ms. This is a sparse image heuristic and can misclassify unusually dark stereo content.

The existing tested missing-eye decision policy requires two bright-left/dark-right samples to activate, two lit-right samples to recover, or three both-dark samples to release. Active frames copy the left half of the pre-UI scene into both halves of the OpenXR color image and omit scene-depth submission. UI composition remains owned by the existing plugin and overlay code. This provides the same scene to both eyes; it cannot create stereo depth for mono cutscene content. Transition logs use `[FF7][auto-mono][D3D11]`.

## Validation

Build Release and run `tests/swgr-auto-mono` with CMake/CTest. `FF7D3D11Tests` creates a real DX11 WARP device, uploads a left-lit/right-black image in RGBA and BGRA, exercises asynchronous production readback, then uploads a stereo image and checks recovery and reset. WARP tests explicitly flush because they have no Present; production does not. Existing SWGR mono/INI and timing tests remain part of this suite.

Headset behavior, FF7 cutscene triggering, visual copy bounds on runtime swapchains, UI/lights, controller actions and performance still need in-game verification. No visual or FPS improvement is claimed from unit/WARP tests alone. Use the paired profile from `jonaslimads/uevr_profiles` branch `ff7-pico4-profile` and follow its validation checklist.
