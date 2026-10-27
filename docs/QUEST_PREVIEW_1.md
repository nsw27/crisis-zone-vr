# Quest Preview 1 — 27 October 2026

Based on 12adbf6. This is a test candidate, not a headset-certified release.

## Included

- Real optical aim projection (including camera offsets); finite and bounded gun registers; out-of-arcade-region rays remain misses instead of fake edge hits.
- Trigger, cover, focus, tracking and simulation-input gating for cabinet-burst feedback. Explicit cancellation on release, cover, pause, focus/tracking loss and hand changes. Held automatic bursts still receive feedback; output is not claimed to identify individual hit events.
- Loaded/selectable pistol and machine gun, with truthful fallback when a model is unavailable.
- Consistent three-page menu with highlighted rows, separate page navigation, edit and reset controls. Refreshes its texture when the selection changes.
- Saved visual recoil and haptics strength (0–100%), muzzle-flash toggle, and refresh preference (default/72/90/120 Hz). Resolution/refresh apply after restarting. The runtime can decline a requested rate.
- Restored all six original resolution choices. Higher resolutions are not a performance recommendation.
- Conservative ray/group broad phase retaining the exact triangle hit algorithm and traversal order. Deterministic tests compare accelerated/reference hits byte for byte; no claim of measured Quest FPS gain.
- Core/source commit and archive-hash checks in Stage B, build/native hashes in the output report.

## Controls

Left stick up/down selects a setting. Left stick left/right changes page.
Right stick adjusts the highlighted setting. Left stick click resets it.
Left menu returns to the game. Gameplay cover and trigger-hand switching remain as before.

## Installation and signing

Preview uses package `org.crisiszone.vr.preview` and label `Crisis Zone VR - Preview 1`.
It installs beside the existing app, preserving that app and its saves. It has separate settings/saves; no automatic save migration. CI generates a temporary test signing key; no signing material is committed, uploaded with the APK or needed on a user's PC. This is not a production signing/upgrade scheme. A future build with another key cannot update this preview in place without a separately agreed signing arrangement.

## Validation and limits

Run `python tests/test_quest_candidate.py` on Linux with GCC, SDL2, EGL/GLES3, OpenXR development headers and a JDK. Tests compile production code with undefined-behaviour and float-cast sanitizers. Graphics tests use software EGL/GLES, not a headset. CI builds a full fresh ARM64 core, generates real H8 from private input, links and verifies a signed/aligned APK. A successful build is not a playtest.

Not included: game-side activation/culling changes, wider hittable arcade region, a demonstrated pop-in fix, or measured Quest frame-time gains. Legacy fake wide-FOV/fade controls are not offered as improvements.
