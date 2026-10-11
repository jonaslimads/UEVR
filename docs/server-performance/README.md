# Server performance investigation

Experimental branch: `swgr-server-performance`, based on the confirmed SWGR automatic mono fix in `bivi`. This build adds diagnostics and exposes existing controlled experiments. It does not claim a measured FPS gain.

## New option

In the UEVR advanced Debug page, expand **Server Performance Investigation**. Enable **Frame Timing Diagnostics**, or set this line in the profile `config.txt` with the game closed:

```ini
VR_FrameTimingDiagnostics=true
```

This option defaults to false. It enables existing engine render, D3D12 copy/fence and OpenXR timing summaries without enabling the hitch snapshot ring, JSON writer, or hitch worker thread. The existing `VR_EnableHitchDiagnostics=true` still enables these timers as before and retains its original snapshot behavior.

The panel also exposes the existing Desktop Spectator View and Sync Mode controls. Start with the normal mirror setting and Very Late synchronization. Disable the desktop spectator view in a separate test to measure mirror cost; the desktop VR mirror may disappear. Resolution and refresh settings are not changed by the panel.

## Reading the logs

Timing summaries are emitted at approximately five-second intervals while the relevant paths are active:

- `[UE57][engine-render-profiler]`: CPU wall time in selected engine render hooks, including the secondary-eye work that those hooks cover.
- `[D3D12][frame-profiler]`: copy, submit, spectator and post-present CPU wall times.
- `[D3D12][fence-profiler]`: CPU waits for GPU command context retirement, where supported by the existing fence profiler.
- `[OpenXR][frame-profiler]`: frame and swapchain lifecycle timings, including `xrWaitFrame` by call site.
- `[VR][server-performance]`: time waiting to acquire the post-present VR mutex and time holding it. The hold measurement covers work inside that critical section, including synchronous frame pacing. It excludes its own log emission and is not a GPU execution timer.

The first server-performance sample also reports Windows-visible logical processor count and process/system affinity masks. It does not change affinity, priority, processor groups or host/guest CPU allocation.

Timers can nest and overlap. Do not sum them as independent costs. Low overall CPU utilization can coexist with a thread-limited workload. Low average GPU utilization can coexist with busy intervals or dependency stalls.

## Safe experiment sequence

1. Capture the existing installed build as a baseline with shader compilation finished.
2. Install this experimental build separately and test with frame timing enabled, original mirror settings and Very Late synchronization.
3. Measure a repeatable racing interval, then validate menus, both eyes, cinematics, crash/explosion mono transitions and stereo recovery.
4. Test Desktop Spectator View off as the only additional change.
5. If wait timings dominate, test a different existing synchronization stage in an isolated run. Restore Very Late afterward.
6. Design concurrency changes around the measured stall and preserve OpenXR call order, resource lifetime and fence ownership. An asynchronous `xrWaitFrame` path already exists upstream but requires texture-array submission; it is not enabled here because the confirmed SWGR configuration uses the double-wide path.

No rendering resources, stereo policy, automatic mono detector, OpenXR call ordering or installed DLL are changed simply by building this branch. Keep the confirmed build available for rollback. In-game validation is required before promoting any experimental configuration.
