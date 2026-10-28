# Enhanced arcade weapon colours

Requested identity: **turquoise machine gun, pink handgun, orange shotgun**.
The original shield and constant aiming dot are unchanged. The existing
Enhanced cover-only directional weapon selection remains the control scheme.
Expanded weapons defaults ON and can be disabled in Handling; saved OFF remains OFF.

## Implemented asset pass

- `arcade-machine-gun.tcgun`: a new, original, stylised Crisis Zone cabinet-inspired
  exterior mesh by Thatcher Productions. Broad turquoise shell, short muzzle,
  vertical foregrip, separate rear grip, ribbing, vents, recessed dark details,
  panel seams and rounded shell edges. This replaces the generic TMP as the
  preferred machine-gun asset. It is not an exact scan or an official Namco asset.
- `namco-gun-pink.tcgun`: a pink variant of the existing attributed Namco pistol.
  Only the blue plastic texture pixels change. The original vertex positions,
  indices, normals, UVs, muzzle calibration and unselected texture pixels remain
  byte-identical. The original Martoscar CC BY 4.0 credit is retained in NOTICE
  and the variant metadata; this is not represented as an entirely new mesh.
- `arcade-shotgun.tcgun`: an original orange arcade-style prop with a dark barrel,
  orange receiver/pump/grip, ribbing and bevels. Its render slot and validation
  are prepared, but the current gameplay still has only machine gun and handgun.
  An asset is not a damage implementation. The three-shell shotgun's damage,
  spread and firing logic must be completed before it appears as selectable.

The weapon selector uses the same turquoise/pink identity as the available models,
with a separate bright selection marker. Orange is reserved, not an inactive
advertised weapon. No new crosshair or shield artwork is introduced.

The runtime prefers the new machine-gun/pistol files and retains the original
files as fallback. Vertex-colour meshes with only a moving trigger now retain
whole-gun visual recoil; the loader detects an actual slide part rather than
assuming every vertex-colour asset has one. Accepted-shot gating is unchanged.

## Reproduction and tests

`python tools/build_arcade_weapons.py` uses Python's standard library only.
It regenerates the two new meshes and the selective pink variant without network
access, proprietary generation services or changes to original source assets.
`python tests/test_arcade_weapons.py` checks deterministic regeneration, finite
geometry/normals, nondegenerate triangles, colour-mask boundaries, the shared
palette and production EGL/GLES rendering, including muzzle transforms and cleanup.
It renders four views of each asset through the actual game's weapon shader.

Machine gun: 5,180 triangles. Shotgun: 3,464 triangles. The original handgun's
17,108 triangles are retained. Counts are not claimed as measured Quest FPS gains.
Rendering tests do not certify headset ergonomics, exact real-cabinet fidelity
or completed shotgun gameplay. Original game, DR-89, reconstruction and asset
credits remain intact.

Visual references used for the machine-gun silhouette, not redistributed textures:
- Namco's original controller page: https://bandainamco-am.co.jp/am/vg/crisiszone/machine-gun.php
- Cabinet photograph: https://psauction.com/item/view/779365/namco-crisis-zone-cabinet-1999

The optional goggle/vignette border, stronger shotgun damage and wider hit testing
are not implemented by this asset pass. No pop-in fix is claimed.
