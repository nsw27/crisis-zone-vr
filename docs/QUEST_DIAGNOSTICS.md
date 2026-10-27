# Quest Preview 2: capture evidence without rebuilding

The playable Preview 2 APK remains source revision `26485c0e765103fe7db7a7d5fa3deea438e34771`, version `0.1.9-preview.2`, package `org.crisiszone.vr.preview2`. Adding this desktop helper does not change the APK, native core, gameplay or signing identity. Keep Preview 1 installed as the known-working fallback.

## Play first

Install Preview 2 alongside Preview 1. It has separate settings and saves. Start at scene density 100%, depth correction off, and the original hold-cover controls. Check ordinary aim, firing, cover and menus before enabling optional handling changes.

Try Handling > Two-hand machine gun with the machine-gun model selected. While this option is enabled, the off-hand grip supports the weapon and the weapon-hand grip operates cover. Try toggle cover separately so its behaviour is not confused with support-hand changes.

Scene density 85% and 70% lower intermediate world resolution, not field of view. Gun, aim guide and VR menu rendering stay at eye-buffer resolution. Keep the same stage, display resolution and refresh preference when comparing density settings. Lower density is a quality/workload tradeoff, not a promised FPS increase.

## Capture a performance issue

Enable Graphics > Performance panel. Resume gameplay and reproduce the slow section, then pause. Keep a note of the stage, action and settings; a short headset recording is useful. Do not restart repeatedly before collection: the app retains only its current and previous game logs.

The panel has overhead. After diagnosis, turn it off for normal play. GPU timing can legitimately be unavailable. CPU submission and GPU scene timings are filtered per-eye measurements, not total headset/compositor frame time. Render rate is not the requested display rate. The helper does not manufacture raw-frame percentiles or 1% lows from these summaries.

## Capture visible pop-in

Just before the problem, open Handling and enable Scene trace. Resume, reproduce the appearance/disappearance, then pause again so the bounded local capture is written. Capture stops at 600 published scenes or 65,536 model-entry records, whichever comes first. A subsequent capture replaces the preceding trace. Collect immediately after the event; there is no automatic upload.

The report distinguishes submitted model records with no built polygons and model-ID presence changes between adjacent complete frames. It refuses presence comparisons across incomplete/truncated frames. Multiple enemies can share a model ID, so those observations are NOT entity spawn/despawn events. Bounds outside the arcade's 640x480 viewport do not prove an object is invisible in VR. Trace frames are relative to capture start and cannot be precisely time-aligned with performance log samples in this format. A recorded pop remains necessary to identify the relevant object and moment.

## Collect on Windows

Requirements: Python 3.9 or later, the existing Android Platform Tools/ADB used for sideloading, a USB-connected headset, and USB debugging already authorised by the headset owner. No additional Python packages are needed. The helper does not bypass Android permissions, root the headset, change settings, clear data, restart the app, or read other applications.

From the repository or unpacked helper ZIP:

```powershell
python tools/quest_diagnostics.py --adb "C:\path\to\adb.exe" --out "quest-diagnostics-test1"
```

When `adb` is on PATH, omit `--adb`. When multiple devices are attached, select the intended headset with `--serial` using the identifier displayed by `adb devices`; the identifier is not stored in collection.json. The default package is Preview 2. The two earlier Crisis Zone package IDs are available via `--package`, but their logs need not contain Preview 2 diagnostics.

The tool reads only these app-private files when present:

- `files/crisiszone-vr.log`
- `files/crisiszone-vr-previous.log`
- `files/quest-scene-trace.csv`

It uses `adb shell -T run-as` with fixed file arguments and bounded reads. It does not collect game ROMs, saves, signing material, screenshots, system-wide logcat or other apps' records. Android's existing ADB authorisation mechanism still applies; the tool does not install or distribute signing keys.

Output is a new local folder containing the available original files, file hashes, collection time, `summary.json` and `REPORT.md`. Missing optional files are reported. Existing capture folders are never overwritten. Review the logs before sharing; they can contain game/environment paths. Nothing is sent to a server automatically. Only the APK is needed to play; this helper is optional.

To analyse files already copied to your PC without using ADB:

```powershell
python tools/quest_diagnostics.py --from-folder "existing-capture" --out "analysed-capture"
```

## Validation

`python -m unittest discover -s tests -p test_quest_diagnostics.py -v` runs 26 synthetic parser/collection tests, including duplicate model IDs, truncation, invalid values, unknown GPU timing, requested-vs-rendered rate, device selection, read-only command allowlisting and overwrite protection. The helper was also run against a 600-scene CSV emitted by the actual production trace code using synthetic geometry. No headset measurement or Windows USB test is implied by those tests.

Official references: Android ADB commands and device authorisation at https://developer.android.com/tools/adb ; run-as's debuggable-app checks and working-directory behaviour at https://android.googlesource.com/platform/system/core/+/master/run-as/run-as.cpp ; asynchronous/disjoint GPU timing at https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt .
