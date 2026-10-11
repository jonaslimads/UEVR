# FF7 Remake DX11 cutscene fallback

This branch extends the published SWGR diagnostic backend without changing its D3D12 fallback or timing instrumentation. FF7 uses the original praydog FF7R plugin at revision `d5815b0d9491ba2f1c2f9596fd02fbb89755ec21` for UI and lighting. That plugin requires DX11 and Windowed mode; do not enable Native Stereo Fix or replace it with the SWGR profile.

The new DX11 path is gated to the exact `ff7remake_.exe` filename, OpenXR, native double-wide rendering, matching HMD/source extents, single-sample/single-mip/single-array RGBA8/BGRA8 textures, and no AFR, texture-array, extreme-compatibility or 2D screen mode. Other games and unsupported FF7 configurations retain existing submission.

`FF7AutoMonoD3D11.hpp` copies 36 interior pixels per eye into one 72x1 staging texture. An event query is polled with `D3D11_ASYNC_GETDATA_DONOTFLUSH`; mapping uses `D3D11_MAP_FLAG_DO_NOT_WAIT`. There is no production flush, wait, full-resolution readback, new worker or engine patch. Resource/format/extent changes release samples; failed readback returns normal rendering. Evidence expires after 500 ms. This is a sparse image heuristic and can misclassify unusually dark stereo content.

The existing tested missing-eye decision policy requires two bright-left/dark-right samples to activate, two lit-right samples to recover, or three both-dark samples to release. Active frames copy the left half of the pre-UI scene into both halves of the OpenXR color image and omit scene-depth submission. UI composition remains owned by the existing plugin and overlay code. This provides the same scene to both eyes; it cannot create stereo depth for mono cutscene content. Transition logs use `[FF7][auto-mono][D3D11]`.

## Validation

Build Release and run `tests/swgr-auto-mono` with CMake/CTest. `FF7D3D11Tests` creates a real DX11 WARP device, uploads a left-lit/right-black image in RGBA and BGRA, exercises asynchronous production readback, then uploads a stereo image and checks recovery and reset. WARP tests explicitly flush because they have no Present; production does not. Existing SWGR mono/INI and timing tests remain part of this suite.

Headset behavior, FF7 cutscene triggering, visual copy bounds on runtime swapchains, UI/lights, controller actions and performance still need in-game verification. No visual or FPS improvement is claimed from unit/WARP tests alone. Use the paired profile from `jonaslimads/uevr_profiles` branch `ff7-pico4-profile` and follow its validation checklist.

## Intermittent subtitles investigation

The user reported satisfactory third-person visuals and one cutscene, but intermittent subtitle disagreement between the eyes during a 3D conversation/cutscene. The first captured run confirmed commit `ddc7bfa1`, DX11 and the original plugin, but contained no auto-mono transitions. It therefore does not validate missing-eye recovery. Setup logged scene format 90 (`B8G8R8A8_TYPELESS`), which v1 rejected. V2 accepts RGBA/BGRA typeless sources and copies into compatible typed staging textures. WARP coverage now includes UNORM, SRGB and TYPELESS for both channel layouts.

The existing normal OpenXR path submits the UI as one shared image/layer with BOTH eye visibility. The plugin redirects `InGameUIRenderTarget` to UEVR's UI target. Neither observation proves that every cinematic subtitle is routed through that target: it may be in the scene, a second UI path or involve stale engine state. Changing the shared quad's distance would not reliably fix text already baked into eye-specific scenes. No blanket scene duplication or subtitle-region copying has been introduced.

V2 adds FF7-only five-second `[FF7][ui-diagnostics]` summaries of source/UI extents and formats, sampler eligibility/support/evidence/pending state, mono state and rendering flags, plus `[FF7][ui-layer]` records of requested eye visibility and overlay settings. These are metadata, not screenshots or subtitle recognition. Correlate a reproduced scene with its time and these records; if the shared layer is correct, inspect the scene/UI images next. Existing gameplay and v1 are preserved for comparison.
