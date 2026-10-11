# Star Wars Galactic Racer: automatic mono fallback

With Joey's native stereo capture, stage introduction cinematics and vehicle crash/explosion sequences can show a complete scene in the left eye and a black scene in the right eye, while UI remains visible. This branch samples the pre-UI scene textures and temporarily copies the left scene to both eyes when the right scene is black. Normal stereo submission resumes when right-eye scene content returns.

## Enable the tested configuration

Build this branch using the repository's `COMPILING.md`. Copy the resulting `UEVRBackend.dll` into your injector directory after closing the game and injector. Copy the adjacent example `SWGRFix-test.ini` into that same directory:

```ini
[SWGRFix]
Variant=4
```

The policy is read once per game process from beside the backend DLL. Variant 4 explicitly enables the automatic fallback; missing configuration retains diagnostic variant 1, which does **not** enable automatic mono. There is no change to default behavior for other games.

Use the existing native stereo profile with OpenXR and the native stereo fix enabled, AFR and texture-array submission disabled. Disable Windows/display HDR and restart the game if colors differ between eyes. Connect Virtual Desktop before starting the game to retain its audio output.

Test automatic operation without pressing F9. The manual force starts OFF on every fresh game process. F9 still toggles an additional manual mono force: turning that force off leaves automatic detection enabled. F8 adds a diagnostic marker to the UEVR log. Mono submissions intentionally omit scene depth; racing recovers its stereo depth when the fallback ends.

## Detection and limits

The detector samples an interior 6 x 6 grid from each scene texture at intervals of at least 40 ms. Readback is asynchronous and fence-gated. Two observations with at least four lit left-eye points and at most one lit right-eye point enter mono. Two observations with at least four lit right-eye points restore stereo. Three both-eye dark observations end mono; both-eye fades cannot enter it. Evidence expires after 500 ms without fresh samples.

Logs include `[SWGRAutoMono][Sample]` and `[SWGRAutoMono][Transition]`. This is content-based fallback, not an engine cinematic-state hook. Very dark asymmetric scenes can produce false positives, and frozen right-eye content that is not black can evade detection. The fallback may take several frames to react and does not repair true cinematic stereo rendering.

The patch also restores the independent D3D12 source states for SWGR's native stereo copy and, for the selected policy, preserves the original Bink packet destination. Sampling validates BGRA descriptors, retains source references until transfer completion, and restores resource states after copying.

## Validation

The user confirmed that variant 4 resolves both stage cinematics and vehicle crash sequences on a Pico 4 through Virtual Desktop with OpenXR and HDR disabled. This is a report from that setup, not a compatibility claim for other games or runtimes.

Standalone Windows tests cover variant policy loading, mono layout/copy guards, transition hysteresis, startup state, dark fades, and stale evidence. Build and run independently:

```powershell
cmake -S tests/swgr-auto-mono -B build/swgr-auto-mono-tests -G "Visual Studio 17 2022" -A x64
cmake --build build/swgr-auto-mono-tests --config Release
ctest --test-dir build/swgr-auto-mono-tests -C Release --output-on-failure
```

Six tests passed. They validate CPU-side policy and copy parameters, not GPU execution or headset behavior. For the original local build and test context, see `TEST-NOTES.pt-BR.md`.
