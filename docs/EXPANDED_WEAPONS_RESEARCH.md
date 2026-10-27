# Optional expanded weapons: first implementation

Base: Preview 3 source `635d49de35e00eaa43d359c937bd400c0f2969db`.
Status: isolated source candidate. Automated synthetic integration tests are not
full-game/headset tests. No new APK is included in this source change.

## Player-facing change

Enable **Handling > Expanded weapons**, then click the **physical right stick**
during play to switch between the existing machine gun and a 12-round handgun.
The right-stick click retains its existing handedness role while paused.

The handgun requires a fresh trigger press for each shot. Holding the trigger does
not fire automatically. Its minimum recovery is 10 original player-update ticks
(about 167 ms at the game's nominal 59.906 Hz); it does not change the game clock.
A brief, valid press is queued for at most 8 player-update ticks if the game has
not yet accepted it. Returning to cover, pause, tracking/action/focus loss and
switching cancel that request. Recovery does not turn a held trigger into a shot.

Each weapon keeps its own magazine and cooldown. Switching cannot refill either.
The existing shield/cover action reloads the selected weapon through the original
game's actual reload event. Switching while already hidden does not invent an
additional reload. A new game/restart resets the loadout. Turning expanded mode
off returns to the original machine-gun rules without refilling its magazine.

The original arcade ammunition display reads the very same game word we update;
there is no second independent fake ammunition counter. The handgun model is the
existing pistol. Shot damage, aiming projection, constant aiming dot and shield
artwork/behaviour are unchanged. Two-hand support remains machine-gun-only, and
with two-hand mode enabled the support grip stays reserved while switching; it
cannot accidentally change cover merely because the pistol was selected.

Off by default; no diagnostic collection is required. Replay input does not enable
custom weapons. A mismatched runtime ammunition transition disables custom firing
rather than guessing. Turning the option off recovers the original weapon path.

## Why this is not just a model/trigger filter

This version directly intercepts *game-confirmed* ammunition consumption. It does
not count trigger pulls, cabinet recoil bits, muzzle flashes, or elapsed time as
proof of a shot. The gameplay instructions remain authoritative for whether a
shot is accepted and when cover reloads. Only a documented shooting-permission
bit is withheld when the handgun has no valid press; other flags are preserved.

The following sites are read from the tracked CSZO4 Ver. B lifted source. Addresses
are executable instruction locations, not a generic cheat for another version.

| Site | Original instruction/use | Adaptation |
|---|---|---|
| `80090968` | Initialises player `+4` to `0x28` (40 rounds) | Reset the private policy state after the original store |
| `800913F8` | Restart/continue sets that ammo word to 40 | Reset loadout; select appropriate capacity when context is valid |
| `800914A0` | Actual cover-reload store to player `+4` | Refill only the selected weapon; handgun capacity 12 |
| `800947F0` | Original player weapon update entry | Validate context, commit requested switch, advance policy tick |
| `80004BD0` | Original live gun decision receives eligibility flags in `a0` | Clear only bit 0 when a handgun shot is not authorised |
| `80094A38` | Original update stores `ammo - 1` after checking accepted result bit 0 | Confirm one shot, account ammunition and allow one feedback event |

The single player's owner is `805495A0`, stats are owner `+2A4` = `80549844`, and
ammo is stats `+4` = `80549848`. GP is `803B4E00`. The bridge verifies the player and
owner pointers at GP `+55F4` / `+55F8`; flags `+30 & 0x40` use the alternate recorded
path and `+30 & 0x02` bypass live fire. Custom handling does not apply there.
The gameplay HUD loads player `+4` at `8009B0B8` and calls `801A7CA4` to draw it.
Every custom game-RAM write is restricted to this single ammo word. Health,
hit coordinates, damage and cover fields are never written by the bridge.

The original generated C files are not edited. `tools/prepare_weapon_hooks.py`
checks their exact SHA-256 values and creates instrumented copies in the CMake
build directory. Any upstream input drift fails configuration before either copy
is written. The imported-core Stage B path does not run this preparation or
recompile the lifted files. These changes require a new matching core archive.

## Automated evidence and scope

- Policy tests: 12 confirmed rounds, no hold-to-fire, bounded rejected requests,
  independent magazines/cooldowns, reload events, invalid input and mode recovery.
- Production host tests: original controls retained, switching without changing
  cover/aim coordinates, held-trigger suppression, action/focus loss, start/menu
  trigger compatibility, saved settings and resets.
- Real lifted-code fixture executes five *actual* generated functions: init,
  reload, reset, weapon-update and live gun-decision. Synthetic cabinet/RAM input
  drives them; peripheral, rendering/audio and non-extracted callees are mocked.
  It proves a held handgun produces one accepted decrement, twelve shots exhaust
  it, and original reload restores twelve. It is not a full-game damage test.
- With expanded mode off, 640 synthetic game updates compare registers, selected
  game-memory regions and instruction budgets byte-for-byte against the original
  uninstrumented functions. This is narrower than a full-game replay comparison.
- Existing ray, cover, two-hand, options and real software EGL/GLES rendering tests
  remain in the regression suite. No Quest frame-rate claim is made.

Run `python tests/test_weapon_hooks.py` for the lifted comparison and
`python tests/test_quest_candidate.py` for the combined host/graphics suite.
The latter needs the documented Linux SDL2/EGL/GLES/OpenXR headers and a JDK.

## Not implemented by this revision

Shotgun, increased damage, pellet spread, a new handgun animation/sound, turquoise
cabinet gun replacement, goggle/vignette border, wider hit testing and activation
pop-in. The current work does not invent a shotgun by sending a rapid burst or
modify accuracy to make the machine gun worse. The unchanged original aiming dot
and shield are retained; no replacement reticle or shield has been added.
