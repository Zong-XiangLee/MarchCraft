# Human performer restoration — 2026-09-14

## Model and technique

The authorized `Models/lowpolyboy_rigged.blend` source is conditioned to the
22-joint, 1.75-meter canonical skeleton. Creator attribution and the contributor's
permission record remain in `Models/LowPolyBoyAttribution.txt`. Runtime GLBs have
17 embedded clips; the application's authoritative animation is the deterministic,
count-driven `HumanGait.js` pose, not a generic walk clip.

The implemented technique is modern corps-style straight-leg marching: forward
heel presentation and roll-through, backward forefoot support, facing-preserving
slides/diagonals, stable upper-body carriage, and a phrase-final close to attention.
Consecutive moving sets retain foot alternation, including odd-count boundaries.
Standing sets are planted; mark time is an optional preview control. Horns-up and
horns-down are session-wide carriage choices.

## Corrections verified

- Explicit XYZ-composed quaternions match the authoring/validation transforms.
  Qt's Euler property uses a different order; using it directly had spread the
  arms and changed multi-axis leg poses. See the [Qt Node rotation contract](https://doc.qt.io/qt-6/qml-qtquick3d-node.html).
- Travel headings respect field Y increasing away from the audience and the
  canonical model's 180-degree audience-facing rotation.
- Stride is normalized by performer height. Planted ankles are checked in actual
  Qt scene coordinates across eight travel headings and four body facings.
- Dense shoe-contact residuals correct slide contact between the original
  32-point calibration samples. Validator tolerances remain unchanged.
- LOD generation preserves shoe vertices/weights and the crown. Mesh reduction
  no longer changes attention toe angles or passing-foot alignment.
- Wrist orientation directs the source finger geometry toward the horn rather
  than crossing the palms above the forehead. Mesh-centroid probes supplement
  wrist-position tests.

## Reproduction

Result: Windows Release build, both Qt suites, all three structural validators,
and all three gait validators passed. The final dense LOD0 sweep measured at
most 2.613 mm penetration, 1.284 mm float, 0.391 mm planted-ankle drift, and
5.854 mm height variation across its reported cases.

Build in `native/build-worktree-mingw`, then run:

```powershell
ctest --test-dir native/build-worktree-mingw --output-on-failure
python native/scripts/assets/validate_human_performer.py native/assets/performer/human_performer.glb
python native/scripts/assets/validate_human_performer.py native/assets/performer/human_performer_lod1.glb
python native/scripts/assets/validate_human_performer.py native/assets/performer/human_performer_lod2.glb
python native/scripts/assets/validate_human_gait.py native/assets/performer/human_performer.glb --samples 256
python native/scripts/assets/validate_human_gait.py native/assets/performer/human_performer_lod1.glb
python native/scripts/assets/validate_human_gait.py native/assets/performer/human_performer_lod2.glb
python native/scripts/assets/render_human_performer_preview.py native/assets/performer/human_performer.glb native/build-worktree-mingw/human-gait-preview.bmp
```

The Qt suites contain 58 C++ cases and 43 QML cases (including fixture lifecycle
cases). Structural checks cover all three meshes: 3,878 / 3,037 / 2,111 triangles.
Dense gait validation samples eight headings at standard stride through a full
two-count cycle, plus half/extended-stride cases sampled at 16 phases. Thresholds
are 3 mm penetration, 2 mm float, 1 mm planted-ankle drift, and 15 mm height variation.

Native screenshot QA covers attention, both carriage choices, forward/backward
marching, slide, mark time, and the bundled 204-performer show's active transition.
Use `--qa-human --qa-human-count 2.3 --3d-view performer-side --screenshot path.png`
for a close sampled pose, and `--3d --qa-current-transition --screenshot-delay 3200
--screenshot path.png` for the ensemble smoke check. Capture runs exit cleanly.
Screenshots and diagnostic outputs stay in the ignored build directory.

## Deliberate limits

The source has no independently articulated fingers in the compact runtime rig;
this is a general instrument-ready carriage, not instrument-specific fingering.
Instrument meshes, modular uniforms, and per-set horn choreography are not part
of this change. Body IDs remain compatible aliases; height is the active body
variation. The 0.70-meter canonical stride cap avoids unvalidated extreme leg
reach but can produce foot slip on larger authored steps; authored drill paths
are never changed to hide this. Existing Clinic warnings remain available.
The playback smoke check is not a frame-rate guarantee on all hardware.
