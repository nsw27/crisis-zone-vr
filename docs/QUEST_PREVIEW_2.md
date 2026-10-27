# Quest enhancement pass / Preview 2

Based on the user-tested Preview 1 source `858bb993fd07cc36e3e19decfca49281da2ba17d`. The working Preview 1 is preserved. This revision is a test candidate, not a claimed headset-certified performance upgrade.

## Concrete changes

### Rendering and CPU work

1. Indexed polygon fans: prepare each polygon corner once instead of repeating it for every triangle. Original triangle order, blending, stencil, per-polygon attributes, optical aim and camera projections are retained.
2. Skip unchanged palette, stencil, text, band and gamma uploads. A publication serial prevents stale data when the game reuses a frame-buffer address. The pre-existing sharing between eyes is retained, not claimed as a new feature.
3. Cache camera-uniform locations; skip empty text/priority passes; allocate the optional exact-depth targets only when needed and resize correctly after density changes.
4. Hoist model transformation matrix preparation and the model's light-vector rotation/normalisation out of the vertex loop. Fixed-point shifts and floating-point expression order are unchanged; model output is compared byte-for-byte against Preview 1.

### Features you can actually try

- **Graphics > Scene density: 100 / 85 / 70 percent.** Applies immediately to intermediate world rendering. 85 percent in each dimension means about 72 percent of the scene pixels; 70 percent means about 49 percent, subject to pixel rounding. Gun, aim-guide and VR menus remain at the eye-buffer resolution. The original arcade HUD is part of the scene and becomes softer too. Menus/title/selection panels use full density. This is an explicit image-quality/workload tradeoff, not a wider field of view and not a guarantee of higher FPS when CPU-bound.
- **Graphics > Depth correction.** Exposes the existing upstream exact-depth approach as an optional experiment. It can address some inter-object painter-order overlap, at the cost of extra rendering. It does not spawn missing objects, expand the game's targetable region or claim to cure activation pop-in. Off by default.
- **Handling > Two-hand machine gun.** Optional support-hand grip near the front of the weapon aligns the model, muzzle and aim ray together. No target snapping or time-delayed smoothing. Controllers must be at least 15 cm apart; implausible reaches/crossed hands do not acquire support. Tracking loss, pause, release, hand/model changes cancel it. While supported, visual recoil is reduced to 60 percent of the chosen setting; the game's firing/damage logic is untouched. **With this mode enabled, the off-hand grip supports the gun and the weapon-hand grip operates cover.** Off by default.
- **Handling > Grip cover: Hold / Toggle.** Optional fresh-press toggle avoids holding a grip continuously. Physical ducking still takes precedence. Toggle mode returns to safety on focus/tracking loss, pause and hand changes. Hold mode keeps the original either-grip control unless two-hand mode reserves the off-hand. Hold remains the default.
- **Handling > Recenter / calibrate height.** A discoverable menu action for the existing recenter/height functions, not a newly invented calibration system.
- **Graphics > Performance panel.** Optional measured rendered-frame and game-scene rates, CPU scene-submission ms/eye, GPU scene ms/eye when supported, scene dimensions, triangle count, upload traffic and scene draw calls. Requested refresh is not labelled as measured FPS. GPU queries are asynchronous, only read when available, and discarded after disjoint events. Unsupported GPU timing is `n/a`. Timings exclude the gun/menu and are not total headset/compositor GPU time. The panel/logging itself has overhead, so disable it for final playtests.
- **Handling > Scene trace.** Starts a bounded local diagnostic capture (maximum 600 published scenes or 65,536 model-entry records). Resume and reproduce the appearance/disappearance, then pause to write `quest-scene-trace.csv` in app-private storage. A new capture replaces the preceding trace. Nothing is uploaded automatically and it never auto-starts on launch. It records submitted model entries, translations, built polygon counts and original screen bounds. Reused model numbers are not unique entity IDs. This distinguishes some submitted-vs-built cases; it is a diagnostic, NOT a pop-in fix.

All experimental features default off, scene density defaults to 100 percent, and the working aim/firing behavior remains the baseline.

## Evidence and limits

The regression suite compiles real code with undefined-behaviour and float-cast sanitizers, mocks OpenXR only for deterministic host-input tests, and uses real software EGL/GLES for rendering tests. It checks 22 menu rows in both eyes, two-handed control lifecycle, toggle-cover safety, persistence, timer-query behavior, bounded/deferred trace writing, the existing ray comparisons and the real world polygon builder.

A 24-eye-view synthetic renderer workload matches actual Preview 1 output byte for byte at full density, including alpha, stencil, gamma, text priority, camera offsets and optional depth. In that workload:

| Metric | Preview 1 | Enhanced renderer |
|---|---:|---:|
| Data-bearing texture uploads | 64 | 14 |
| Texture-upload bytes | 9,659,904 | 2,061,760 |
| Vertex/index-buffer upload bytes | 22,032 | 15,168 |

These are deterministic workload counts, NOT measured Quest FPS gains. A separate 1,024-case model-builder comparison covers transposed matrices, zero/nonzero light vectors, normals, model morphing, shading and blending. Per-device graphics timing, actual asset mix, headset comfort, translucent-object correctness and real-game popping still require headset testing.

## Research informing the work

- Lambda1VR documents off-hand grip weapon support and a 15 cm controller-separation deadzone: https://www.lambda1vr.com/
- Half-Life 2: VR Mod separates weapon support, accessibility, comfort and performance decisions: https://halflife2vr.com/manual/
- Doom3Quest uses an adapted multi-threaded renderer and multiview; its authors explicitly note these features are not drop-in transplants: https://www.doom3quest.com/
- Khronos EXT_disjoint_timer_query defines asynchronous, non-stalling GPU timing and disjoint validity: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt
- Khronos OVR_multiview2 requires view-aware shader and target changes: https://registry.khronos.org/OpenGL/extensions/OVR/OVR_multiview2.txt

Implementations here are original adaptations, not copied port code. Multiview and foveation have NOT been enabled: this renderer's intermediate color, priority, gamma and optional depth passes need coordinated changes. Adding a flag to the final OpenXR swapchain alone would not optimise those intermediate passes. Wider game-side hit testing and activation/culling changes remain investigation work.

## Reproduce the tests

On Linux with GCC, SDL2, EGL/GLES3, OpenXR development headers and a JDK:

```sh
git show 858bb993fd07cc36e3e19decfca49281da2ba17d:quest/tc2_gl.c > /tmp/preview1-gl.c
git show 858bb993fd07cc36e3e19decfca49281da2ba17d:upstream/tc2/src/tc2_render.c > /tmp/preview1-model.c
LIBGL_ALWAYS_SOFTWARE=1 python3 tests/test_quest_candidate.py \
  --baseline-renderer /tmp/preview1-gl.c --baseline-model /tmp/preview1-model.c
```

## Build and installation safety

These host and renderer files live in `libcrisiszone_core.a`: a new matching core is mandatory. Stage B checks the core source commit and digest before packaging real H8. Do not use an old Preview 1 core.

A later APK uses package `org.crisiszone.vr.preview2`, version `0.1.9-preview.2`, label `Crisis Zone VR - Preview 2`. It installs alongside Preview 1, with separate settings/saves. Do not uninstall your working version. CI test signing is temporary; no key is installed on a player's PC, and this is not a production signing/update scheme.
