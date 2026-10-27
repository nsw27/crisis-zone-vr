# Quest Preview 3: rendering efficiency, edge smoothing and point-and-trigger menus

Built on Preview 2 game source `26485c0e765103fe7db7a7d5fa3deea438e34771` (the enhancement branch at `d546ef0` also adds standalone developer tools). Original port and previous APKs are preserved. This is a headset test candidate, not a claim of improved headset frame rates.

## What changes for the player

- **Point-and-trigger menus, enabled without configuration.** Point the weapon-hand controller at a setting label and pull the trigger to select it. Point at its minus or plus to adjust it, or at the page arrows to change page. The yellow cursor follows the actual panel plane; stick selection/edit/reset controls continue to work. A held trigger does not repeat, including when opening a menu or regaining tracking. This operates only while paused and does not alter gameplay hit coordinates.
- **Graphics > Edge smoothing**, an optional spatial scene-edge filter inside the existing final colour pass. Applies immediately. It rounds some high-contrast staircase edges without temporal history, view jitter or an added rendering pass. Low-contrast/flat regions are left alone. Text-marked priority pixels and their neighbouring pixels are conservatively protected; title/selection panels bypass the filter. The tracked weapon and VR menus are drawn afterward and are not filtered. This is NOT MSAA, texture reconstruction, a sharper low-resolution HUD or a guaranteed cure for distant texture shimmer. It adds shader work and may soften scene detail, so it defaults off and can be disabled immediately.
- **Grouped rendering passes, always active.** Finish optional depth, then text priority, then scene colour, then the final colour correction. Each off-screen target is completed without repeatedly leaving and returning to it. The original painter order, transparency, stencil and text priorities within each target remain unchanged. This reduces avoidable framebuffer switching; it does not change resolution, field of view or game activation/culling.

## Evidence, without making the player run diagnostics

The automated suite uses synthetic assets and real software EGL/GLES, with undefined-behaviour and float-cast sanitizers. OpenXR input is mocked for deterministic lifecycle tests. The existing scene, firing, two-hand, cover, options, renderer and model tests remain included.

- **192 eye views** match actual Preview 2 renderer output byte-for-byte with smoothing off. Cases include text-only/empty frames, stereo camera offsets, scene densities, alpha, gamma, original palette effects, all text-priority values and depth correction.
- Actual framebuffer changes in that workload: **703 before, 505 after**. These are API-work counts, not a measured Quest speedup or bandwidth measurement.
- The simpler original 24-view workload is also unchanged: 111 versus 75 framebuffer changes.
- The filter test softens **96 synthetic diagonal edge pixels** while keeping **336 text-marked pixels** exact. Flat regions, image boundaries, original gamma and the existing single resolve draw are checked. This is not a gameplay screenshot or a visual-quality rating.
- **690 pointer/button projections** cover both hands and rotated panels. Production input tests exercise label selection, +/- edits, page clicks, held-trigger suppression, off-panel presses and tracking loss. All 23 menu rows render in both eyes.

No diagnostic collection is needed to play. Preview 2's optional developer tools remain optional and inactive by default.

## Installation

Version `0.1.10-preview.3`, package `org.crisiszone.vr.preview3`, label **Crisis Zone VR - Preview 3**. Installs beside existing versions, with separate settings and saves. Do not uninstall your working build. CI test signing is temporary; no key needs to be kept on the player's PC. This is not yet a production in-place update/signing arrangement.

## Original-developer handover

The source-only handover separates the rendering-pass reorder from the optional filter and menu work. The reorder is the smallest upstream-friendly patch. Tests compare against the actual previous source instead of a reimplemented approximation. These are original changes; no other port's source was copied.

Technical reference for completing each target before changing framebuffer:
https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/mali-performance-2-how-to-correctly-handle-framebuffers

This explains the design rationale, not a claim that measurements from a Mali GPU predict Quest results. The implementation uses ordinary GLES 3 operations; it does not pretend to add multiview or foveation.

## Still not fixed or verified

Game-side activation pop-in and genuinely wider arcade hit detection are unchanged. No multiview, foveation, weapon MSAA or manual-reload system is claimed. Actual headset performance, pointer ergonomics and visual preference require normal play, not requested diagnostic work.
