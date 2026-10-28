# Crisis Zone VR - Enhanced

Unofficial enhancements by **Thatcher Productions**.
Original VR port: **DR-89 and contributors**.
Underlying game reconstruction: **spacestate1/cmcrann**.
Original Crisis Zone game: **Namco**.

This is an enhancement fork, not a replacement claim of authorship or an official
Namco release. Existing licenses, notices, acknowledgements and original game
artwork remain intact. `NOTICE.md` continues to be packaged in the APK.

## Cover-based weapon selection

Inspired by Time Crisis 3's hide-and-select flow. Namco's product page describes
pulling the trigger with the pedal released to change weapons:
https://bandainamco-am.co.jp/am/english/aa/timecrisis3/

This version deliberately uses the requested **physical right thumbstick** instead:

- In shield/cover or calibrated physical crouch, **up/right selects next** and
  **down/left selects previous**. No thumbstick click is required.
- Return the stick to centre before the next selection. Holding or moving
  diagonally cannot auto-cycle the loadout; the dominant axis selects once.
- While exposed/firing, stick movements do not switch weapons. A direction held
  while entering cover, unpausing or regaining tracking must return to centre.
- A small stereo weapon strip appears below the centre of view while selecting
  in cover, showing available weapons and their confirmed remaining magazines.
  It disappears on exposure, pause, tracking loss or leaving live gameplay.
  A pending switch is labelled until the game commits it at its update boundary.
- The original shield and constant aiming dot are unchanged. This is not another
  reticle, target snapping, a shield replacement or an aim-coordinate change.
- Right-stick click retains its paused handedness function. The pause menu's
  right-stick setting controls and point-and-trigger menu remain available.

**Expanded weapons is ON for new or missing settings.** It can be turned off in
**Handling > Expanded weapons**, and an explicitly saved OFF remains off.
Resetting that setting restores ON. The original machine-gun-only gameplay path
remains available when disabled. No diagnostics are needed to play.

## Available mechanics versus planned work

The source now implements the original turquoise machine gun, the **12-round
single-shot pink handgun**, and an **orange three-shell shotgun**. A shotgun shell
uses three ordinary native hit rays in a triangle, separated by two original
weapon-update ticks. One confirmed shell costs one round and one feedback event.
The actual enemy damage/collision rules are retained; threefold damage against
every target is not guaranteed. This is not headset-tested gameplay.

Each weapon retains its magazine and cooldown. Switching never refills ammo; the
original cover reload event remains authoritative. The selector and original
ammunition display reflect accepted game events rather than a cosmetic counter.
See `SHOTGUN_TRIANGLE.md` for timing, safeguards and the testing boundary, and
`ARCADE_WEAPON_COLOURS.md` for asset provenance.

**Handling > View border: Off / Soft fade / Goggles** adds optional peripheral
presentation without changing aim, camera projection, the constant aiming dot or
shield mechanics. It defaults OFF, applies immediately and saves. It is hidden
for paused menus and arcade title/selection panels. See `GOGGLE_BORDER.md`.
Pop-in and the arcade's hittable region are not changed here.

## Branding and installation

Menu title: **CRISIS ZONE VR - ENHANCED**.
Menu footer: **UNOFFICIAL ENHANCEMENTS: THATCHER PRODUCTIONS**, with Namco and
DR-89 credits immediately underneath. The complete reconstruction/dependency
credits remain in `NOTICE.md` and the README. No original logo was replaced.

A later build from this source uses label **Crisis Zone VR - Enhanced (Preview 4)**,
package `org.crisiszone.vr.enhanced.preview4`, version `0.2.0-enhanced.preview4`.
This preserves the original and earlier previews as separate installations with
separate settings/saves. No APK is produced by this source-validation change.
The existing temporary preview signing is not a production in-place upgrade
scheme; no signing material is shipped to a player's computer.

## Verification and limitations

`tests/test_quest_candidate.py` exercises real host controls with mocked OpenXR,
plus software EGL/GLES rendering. Added cases check cover gating (hold/toggle and
physical ducking), no held-stick selection on context changes, all four directions,
no magazine refill, existing aiming/cover preservation, saved OFF, and a real
stereo selector raster. Its texture uploads only on state changes, not per eye.

`tests/test_weapon_hooks.py` still executes the extracted original firing/reload
functions using synthetic memory/input; 640 mode-OFF updates are compared with
the uninstrumented functions. The additional native ray-dispatch hooks are covered by `test_shotgun_dispatch.py`;
its final collision consumer is recorded, not a real-enemy damage test. No
headset-performance result is claimed. Normal headset play is still needed to assess readability/comfort.
