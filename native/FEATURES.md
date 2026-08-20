# MarchCraft prototype feature inventory

## Shape editing and freehand formations

- Persistent formations expose image-editor-style uniform resize handles and a visible rotation control when the complete shape is selected.
- Freehand field drawing supports organic forms and letter-like strokes, automatic line/circle recognition, handwriting smoothing, equal arc-length spacing, collision-aware placement, and three performer-assignment strategies.
- The inspector reports selection size, average/minimum spacing, average move, dimensions, and collisions. Configure can display distances in marching steps or yards.

This inventory tracks behavioral parity with the public OpenMarch feature list. MarchCraft is an independent native implementation and does not contain OpenMarch source code.

| Area | Prototype behavior |
| --- | --- |
| Field types | High-school, college, professional, and indoor presets |
| Canvas | GPU-backed native Qt view, zoom, pan, regulation ten-yard end zones, realistic turf mowing bands, five-yard lines, front/back yard numbers, regulation HS/NCAA/NFL hash placement, four vertical one-yard inserts per five-yard interval, and midfield X references fourteen steps outside each hash |
| Animation | Set-to-set or continuous whole-show playback with visible marcher paths and per-set count/BPM timing |
| Roster | Add, edit, remove, batch-create, search, multi-select, custom name/section/instrument/notes |
| Sets | Add, edit, delete, duplicate, batch-create, and mark subsets |
| Drill editing | Drag, keyboard nudge, axis lock, grid snap, grouping/context actions, compact group-colored dot markers plus circle/square/diamond markers, per-set facing with animated turns, persistent shape library, adaptive spirals, and equal-distance distribution |
| Saving | Versioned local project file, atomic save, automatic recovery copy, undo/redo |
| Music | Native MIDI/MusicXML timing and track import, compact selectable measure timeline, built-in FluidSynth playback, count-based set generation, step-mode overrides, waveform audio and synchronization anchors |
| Coordinates | Audience-perspective coordinates: Side 1 left, Side 2 right; front sideline/hash toward the audience, back hash/sideline away from it; redesigned landscape performer sheets with movement analytics, repeated headers, pagination, and full CSV export |
| Analytics | Per-move and total distance, ensemble average, longest move, collision and step-size warnings |
| 3D | Meter-based Y-up world; grounded skinned human performers with path-aware straight-leg marching, backward motion, slides, turns, and three LODs; body-rig, skin, and future equipment socket attributes; configurable rehearsal/stadium/gym/arena environments; lighting and quality presets; props; press-box, overhead, and field cameras; ground-contact diagnostics |
| Platforms | Qt/CMake architecture supports Windows first and portable macOS/Linux builds |
| Bundled test show | The supplied Rancho Bernardo data opens by default with 204 performers and 97 sets |

## Prototype boundaries

- Transition paths are currently direct interpolations. The domain boundary is ready for curved, follow-the-leader, gate, and counter-march path implementations.
- Audio can be attached, previewed, offset, and aligned with anchors; automatic beat/phrase inference remains outside the prototype because it is not authoritative for changing-tempo marching arrangements.
- The built-in assistant is deterministic. Cloud AI proposals will plug into the validated project command layer later.
- “In front of back hash” means toward the audience from the far hash; “behind front hash” means away from the audience from the near hash.
- GLB ingestion is architected through a validated semantic catalog; finished Blender meshes, retargeted clips, cinematic rendering, and video export remain production milestones.
- The undo stack is active during a session; cross-session version browsing will be added with the production project archive format.
