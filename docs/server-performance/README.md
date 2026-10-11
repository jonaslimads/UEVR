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

For unattended SWGR investigation, place the provided `UEVRServerPerformance.ini` beside `UEVRBackend.dll` before injection. `[Diagnostics] FrameTiming=1` enables the same lightweight timers for SWGR only, independently of profile saves or resets. It is read once per injected process. Remove the file or set `FrameTiming=0` and restart the game to disable this override. Other games and the default build behavior remain opt-in. This does not enable hitch snapshots or change rendering settings.

`[VR][timing-config]` records the profile path, raw profile value, reset request, launch override and effective state at each configuration load. Confirm `effective=true` and actual profiler summaries before treating a capture as an internally measured run. The post-present timing summary also records its current Windows thread ID for correlation with external CPU sampling.

Timing summaries are emitted at approximately five-second intervals while the relevant paths are active:

- `[UE57][engine-render-profiler]`: CPU wall time in selected engine render hooks, including the secondary-eye work that those hooks cover.
- `[D3D12][frame-profiler]`: copy, submit, spectator and post-present CPU wall times.
- `[D3D12][fence-profiler]`: CPU waits for GPU command context retirement, where supported by the existing fence profiler.
- `[OpenXR][frame-profiler]`: frame and swapchain lifecycle timings, including `xrWaitFrame` by call site.
- `[VR][server-performance]`: time waiting to acquire the post-present VR mutex and time holding it. The hold measurement covers work inside that critical section, including synchronous frame pacing. It excludes its own log emission and is not a GPU execution timer.

The first server-performance sample also reports Windows-visible logical processor count and process/system affinity masks. It does not change affinity, priority, processor groups or host/guest CPU allocation.

Timers can nest and overlap. Do not sum them as independent costs. Low overall CPU utilization can coexist with a thread-limited workload. Low average GPU utilization can coexist with busy intervals or dependency stalls.

## Safe experiment sequence

### v3 stage tracing

The provided launch INI additionally sets `RenderTrace=1` for SWGR. This emits fixed-size, per-thread five-second summaries under `[SWGR][stage-profiler]` for engine tick/original tick, native-family dispatch/original linked renderer, the complete pre-render callback including its deferred worker, the worker alone, Slate routing/restoration, render-texture callbacks/Slate worker and automatic-mono polling. `[SWGR][stage-thread]` records each stage's Windows thread name and ID. `[SWGR][family-layout]` records input and linked-family counts, view counts, capture extent and generation.

Each stage reports inclusive wall time, wall time excluding directly nested instrumented scopes, inclusive thread CPU time, histogram percentile upper bounds, calls per second, entry intervals and calls exceeding the 90 Hz frame budget. CPU time is quantized by Windows accounting and can exceed individual short wall samples; interpret aggregates. The final histogram overflow is printed as infinity. Exclusive time excludes only instrumented child scopes, not arbitrary engine work. Asynchronous render commands continue outside the game-thread dispatch scope.

Thread identification and the stage's own summary emission are outside its sample; enclosing scopes can still observe logging and timing overhead. There is no shared diagnostic aggregation lock and no background sampler, thread suspension or stack walk in these hooks. CPU-time queries add overhead. Disable `RenderTrace` for a separate overhead comparison. Partial buckets at shutdown may not be emitted. These diagnostics are not an optimization or an FPS-gain claim.

`[VR][timing-save]` and `[SWGR][trace-config]` make profile saves and effective trace activation visible. The launch file remains independent of profile persistence. Use an external sampled CPU trace to identify the expensive functions outside these instrumented boundaries; the profiles repository includes `tools/performance/Collect-CPUStacks.ps1` for the local WPR CPU profile. This requires an administrator terminal, refuses to replace an existing recording, and saves an ETL plus module/PID metadata outside the repository. Filter CPU Usage (Sampled) to the recorded game PID and hot render TID, then use the matching backend PDB to attribute UEVR frames. CPU profile stack collection and its overhead require a separate short run.

1. Capture the existing installed build as a baseline with shader compilation finished.
2. Install this experimental build separately and test with frame timing enabled, original mirror settings and Very Late synchronization.
3. Measure a repeatable racing interval, then validate menus, both eyes, cinematics, crash/explosion mono transitions and stereo recovery.
4. Test Desktop Spectator View off as the only additional change.
5. If wait timings dominate, test a different existing synchronization stage in an isolated run. Restore Very Late afterward.
6. Design concurrency changes around the measured stall and preserve OpenXR call order, resource lifetime and fence ownership. An asynchronous `xrWaitFrame` path already exists upstream but requires texture-array submission; it is not enabled here because the confirmed SWGR configuration uses the double-wide path.

No rendering resources, stereo policy, automatic mono detector, OpenXR call ordering or installed DLL are changed simply by building this branch. Keep the confirmed build available for rollback. In-game validation is required before promoting any experimental configuration.
