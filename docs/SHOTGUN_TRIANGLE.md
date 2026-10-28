# Enhanced shotgun: three shells, three staggered native hit rays

Source candidate based on `5c6b4be1eb77afc7a31015b3950a0042f37828bc`.
This is the user-requested three-normal-shot workaround, not a new ballistic
engine or a guarantee that every enemy accepts three damage events.

## Player behaviour

The orange shotgun now joins the turquoise machine gun and pink handgun in the
cover-only selector. Expanded weapons remains ON for new settings and can be
turned OFF in Handling. Existing saved OFF remains OFF. The original shield and
constant aiming dot have not been replaced, offset or enlarged.

- Three **shells** in the shotgun magazine. Each fresh trigger press that the
  original arcade game accepts costs **one shell**, not three bullets.
- One shell requests three ordinary native hit rays in a tight triangle around
  that shot: top `(0,+10)`, lower-left `(-9,-5)`, lower-right `(+9,-5)` in the
  game's centred screen-coordinate system. These are not three extra aiming dots.
- The rays are dispatched on separate original weapon updates, at least two
  updates apart: nominally about 0 / 33 / 67 ms at 59.906 updates per second.
  Late dispatch never catches up by issuing several rays in one update.
- One recoil/flash/haptic event per accepted shell. Holding the trigger does not
  auto-fire. A fresh press is required after the 30-update firing recovery
  (roughly half a second at the nominal game rate).
- The original cover-reload event replenishes the selected shotgun to three.
  Switching preserves every weapon's ammunition and cooldown. It is not a reload.
- Right stick up/right selects next; down/left selects previous while in cover.
  Return the stick to centre before another change. No extra click or aiming aid.
- Returning to cover, pause, tracking/focus loss, death, switching, mode changes,
  restart and reload cancel remaining pellets. No ammunition refund is invented.
- The group expires after eight original update ticks. Off-screen individual
  pellets are discarded, never wrapped/clamped to manufacture edge hits. A shot
  the original game classifies as non-damaging/off-screen does not gain pellets.

## Native integration

Original CSZO4 Ver. B generated source remains unchanged on disk. The existing
hash-guarded preparation tool produces instrumented copies in the build directory.
A new matching core is required; an older archive cannot contain this change.

The real accepted-ammunition decrement (`80094A38`) starts the burst only when
its original live-hit flags contain both bits in mask 6. Each pellet uses the
normal live dispatcher `80095038`, native projection `80006B60`, original
fixed-point matrix routine `801F3CA0` and the existing collision/damage call
`800648BC`. No direct enemy-health write or broad damage multiplier is added.

The original dispatcher still runs at its normal scheduled point. The hook selects
one transient projection pair and local dispatch flags only; it does not fabricate
extra trigger presses, recursively run gameplay, advance the CPU/game clock or
alter the global gun/reticle coordinates. Native instructions/scheduler checkpoints
are not removed. Original pause/death gates remain in place. The explicit bridge
writes to game RAM are still restricted to the documented player ammo word.

Pellet centre coordinates are captured at the accepted shot; subsequent controller
aim samples do not drag the group around. Each native projection still uses the
current arcade camera/origin. This is a short screen-space spread, not a claimed
world-space projectile simulation, distance-calibrated cone or off-screen expansion.

## Automated evidence and its limits

`tests/test_shotgun_dispatch.py` executes eight complete, actual generated
functions using synthetic memory/input, including the firing, reload, ray
projection and live dispatcher. The final collision consumer is **recorded**, not
executed against real level/enemy state. Tests verify three separated ordinary
collision calls, their triangle vectors, one ammo debit/feedback event, three-shell
exhaustion, reload, held-trigger rejection, edge rejection and interruption.

A mode-OFF comparison records 128 actual native updates and compares registers,
selected RAM and remaining instruction budgets byte-for-byte with uninstrumented
code. The existing 640-update firing/reload comparison is retained separately.
Pure policy tests cover deadlines, delayed dispatch, tick overflow, invalid input,
independent magazines and switching/cooldown abuse. Real host-input tests cover
all three weapons, saved settings, cover constraints and missing-mesh fallback.
The production stereo selector raster is tested with the third orange card.

This is **not** proof of three times damage on every enemy/boss. Their normal
collision, armour, death/reaction and repeated-hit rules remain authoritative.
Each ray may hit scenery, miss, hit a different target or encounter immunity.
No new shotgun-specific sound or pump animation is claimed by this pass.
No Quest frame-rate improvement or full-game/headset validation is claimed.

Run:

```sh
python tests/test_shotgun_dispatch.py
python tests/test_weapon_hooks.py
python tests/test_quest_candidate.py
```

The ordinary software EGL/GLES/input prerequisites apply. Players do not need to
run these tests or collect diagnostics. All original author/asset notices remain.
