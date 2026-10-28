# Optional soft fade / tactical goggles

**Handling > View border** offers **Off / Soft fade / Goggles**. Changes apply
immediately and are saved. The border defaults OFF; this is independent of
Expanded weapons, which defaults ON and can still be disabled.

Soft fade darkens only the peripheral view. Goggles use a broad rounded opening,
a soft charcoal rim, a subtle inner seam and a small lower nose contour. No green
tint, scratches, lens distortion, blur over the centre or extra reticle is added.
The original aiming dot and shield/cover artwork and mechanics remain unchanged.

The border is head-relative, derived from each eye's real asymmetric projection
and mapped to a common orientation. It is not the same screen-centred circle
stamped into both eyes. It changes no camera projection, game coordinates, target
activation, hit test or simulation timing. The centre remains unmodified in the
automated test views. It draws after the weapon, before the weapon selector and
performance panel, so those interfaces remain readable. Paused options and the
arcade title/selection panels bypass it; focus loss also disables drawing.

This is a presentation/comfort option, NOT automatic detection of missing scenery.
It can obscure extreme peripheral gaps but cannot fix objects popping into the
middle of view or create unsubmitted scene geometry. It does not inspect black
pixels or depth values to guess what should be missing. Peripheral content is
intentionally hidden when enabled, including any targets in that part of view.

## Implementation and evidence

One full-screen triangle per eye when enabled, no texture samples, intermediate
framebuffer, image readback or temporal history. OFF performs no draw and allocates
no border resources. Shader failure leaves the original view available. All GL
state modified by the pass is restored, destination alpha is preserved, and
invalid projection/orientation input skips the optional effect.

The real software EGL/GLES fixture checks the unchanged central region and alpha,
left/right asymmetric optical centres, rotated eye views, OFF/invalid-input
behaviour, state restoration, cleanup/reinitialisation and persistent settings.
The ordinary input/menu/weapon/scene/rendering tests are retained. Border preview
images use a synthetic flat background; they are not headset gameplay captures.

Headset comfort and visual preference have not been measured. There is an extra
draw/shader cost when enabled; no frame-rate improvement is claimed. No diagnostic
collection is required from the player. The effect can be turned off immediately.
