# MarchCraft Blender asset contract

This directory is the source-of-truth location for editable `.blend` files. Runtime code never refers to a Blender filename; projects store semantic IDs from `assets/catalog.json`.

## Scene coordinates

- Units: metric, unit scale `1.0`.
- Up: `+Y` in the exported GLB. Blender authors work Z-up; the glTF exporter performs the axis conversion.
- Forward: `-Z` after export.
- Performer origin: `(0, 0, 0)` on the ground, centered between the feet.
- Prop and venue-piece origin: centered on the ground-contact footprint unless its manifest explicitly documents a hinge.
- Apply object rotation and scale before export. No negative or non-uniform scale may remain on a skinned object.
- Locomotion clips must be in-place. The root bone may rotate but may not translate horizontally.

## Canonical performer rig

Every body variant uses the `marchcraft.canonical.v1` skeleton and these attachment sockets:

`hand.left`, `hand.right`, `chest`, `shoulder.left`, `shoulder.right`, `waist`, `back`, `head`, `equipment`.

Required clips are `idle`, `march.forward`, `march.backward`, `slide.left`, `slide.right`, `mark_time`, `direction_change`, `horn.up`, and `horn.down`. Keep bone names and rest-pose orientation identical across body variants so the animation library can be retargeted deterministically.

## Materials and modular parts

- Use metallic/roughness PBR materials.
- Uniform materials expose `primary`, `secondary`, `accent`, `plume`, `gloves`, and `footwear` slots.
- Prefer shared texture atlases and materials across rigs.
- Instruments are separate assets aligned to their catalog socket and carry profile.
- Keep logos and venue branding separate from regulatory field markings.

## Levels of detail

- LOD0: presentation model for close cameras.
- LOD1: reduced geometry for normal field viewing, retaining the canonical skeleton.
- LOD2 keeps the weighted human silhouette with aggressively reduced geometry for press-box and performance views.

## Export and validation

Export GLB/glTF 2.0 with skins, animations, normals, tangents, and PBR textures. Before an asset can be added to the application, add or update its entry in `assets/catalog.json` and validate:

- semantic ID and positive version;
- meter scale and ground contact within 2 mm of `Y=0`;
- canonical rig family and required sockets;
- required clip names with no horizontal root drift;
- finite bounds and reasonable triangle/material counts;
- license and attribution metadata.

The bundled human uses validated, preconditioned GLBs decoded by `HumanGeometry` and the explicit Qt Quick 3D Skin API. User-supplied GLB files are intentionally unsupported in the initial catalog.

## Canonical human performer

The authorized Blender source is `Models/lowpolyboy_rigged.blend`;
its creator attribution and the contributor's direct-permission record are in
`Models/LowPolyBoyAttribution.txt`. First run `extract_lowpolyboy_mesh.py`
through Blender to preserve its authored deformation weights in a temporary
JSON interchange file, then run `build_human_performer.py` on that JSON. The
conditioner retargets the mesh to the canonical 1.75-meter, Y-up bind pose and
authors all in-place drill clips. `import_human_performer_blender.py` can import
the conditioned GLB for manual weight and pose refinement.

The runtime gait is count-driven by `qml/HumanGait.js`. It uses planted-foot
targets and a two-bone leg solve for forward and backward technique, while
slides distribute facing progressively through the pelvis, chest, and shoulders.
The neutral runtime pose is marching attention: grounded closed heels, 90-degree
total toe-out, a vertically stacked pelvis and torso, head approximately 10
degrees above center, and a face-height triangular hand set. The source finger
geometry is preserved while its weights are collapsed into the runtime's compact
canonical hand bones. Forward technique presents a high toe at
heel contact, rolls through the full foot, and releases from the forefoot; backward
technique stays on a calm forefoot platform. After changing the rig, weights, or
gait constants, run both validators and regenerate the contact sheet:

```powershell
python native/scripts/assets/validate_human_performer.py native/assets/performer/human_performer.glb
python native/scripts/assets/validate_human_gait.py native/assets/performer/human_performer.glb
python native/scripts/assets/render_human_performer_preview.py native/assets/performer/human_performer.glb human-gait-preview.bmp
```

The gait validator deforms the actual weighted mesh through eight compass
headings plus half and extended stride cases. It rejects turf penetration beyond
3 mm, floating beyond 2 mm, planted-ankle drift beyond 1 mm, and upper-body
height variation beyond 15 mm.
