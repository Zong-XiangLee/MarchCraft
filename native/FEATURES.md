# MarchCraft prototype feature inventory

## Shape editing and freehand formations

- Persistent formations expose image-editor-style uniform resize handles and a visible rotation control when the complete shape is selected.
- Freehand field drawing supports organic forms and letter-like strokes, automatic line/circle recognition, handwriting smoothing, equal arc-length spacing, collision-aware placement, and three performer-assignment strategies.
- The inspector reports selection size, average/minimum spacing, average move, dimensions, and collisions. Configure can display distances in marching steps or yards.
- The application opens to a professional native welcome workspace with New/Open actions, a pinned editable sample, current-project resume, and eight persisted recent projects.
- A restrained graphite visual system standardizes controls, disabled states, dialogs, panels, menus, and workspace motion; the quiet launch sound is optional and suppressed during QA.
- Group selection is direct: clicking or right-clicking a member selects its group, while context actions expose only valid Group, Remove from group, and Ungroup operations.

This inventory tracks behavioral parity with the public OpenMarch feature list. MarchCraft is an independent native implementation and does not contain OpenMarch source code.

| Area | Prototype behavior |
| --- | --- |
| Field types | High-school, college, professional, and indoor presets |
| Canvas | GPU-backed native Qt view, zoom, pan, regulation ten-yard end zones, realistic turf mowing bands, five-yard lines, front/back yard numbers, regulation HS/NCAA/NFL hash placement, and four vertical one-yard inserts per five-yard interval |
| Animation | Set-to-set or continuous whole-show playback with visible, constant-speed authored marcher paths and per-set count/BPM timing |
| Roster | Add, edit, remove, batch-create, search, multi-select, custom name/section/instrument/notes |
| Sets | Add, edit, delete, duplicate, batch-create, and mark subsets |
| Drill editing | Drag, keyboard nudge, axis lock, grid snap, grouping/context actions, compact group-colored dot markers plus circle/square/diamond markers, per-set facing with animated turns, persistent shape library, adaptive spirals, equal-distance distribution, and transactional transition authoring for curves, FTL, gate/pivot, and stagger/ripple timing |
| Saving | Versioned local project file, atomic save, automatic recovery copy, undo/redo |
| Music | Native MIDI/MusicXML timing and track import, unified time-scaled drill/music/audio lanes with shared zoom, scrubbing, follow-playhead, and independent page/range selection, persistent color-coded movements and parts, built-in FluidSynth playback, count-based set generation, step-mode overrides, waveform audio and synchronization anchors |
| Coordinates | Audience-perspective coordinates: Side 1 left, Side 2 right; front sideline/hash toward the audience, back hash/sideline away from it; redesigned landscape performer sheets with movement analytics, repeated headers, pagination, and full CSV export |
| Analytics | Per-move and total distance, ensemble average, longest move, collision and step-size warnings |
| 3D | Meter-based Y-up world with grounded, height-scaled rigged human performers; modern corps-style forward, backward, slide, turn-in-place, diagonal, and phrase-close gait; authored facing and selection; rehearsal/stadium/gym/arena environments, lighting/quality presets, props, and press-box/overhead/field cameras. |
| Platforms | Qt/CMake architecture supports Windows first and portable macOS/Linux builds |
| Bundled test show | The supplied Rancho Bernardo data opens by default with 204 performers and 97 sets |

## Prototype boundaries

- Transition authoring supports direct and multi-control-point curves, persisted Follow the Leader routes, gate/pivot arcs, individual correction/copy/mirror operations, and explicit stagger/ripple starts. Group tools resolve to per-performer path data; automatic counter-march vocabulary beyond these authored routes remains a future extension.
- Audio can be attached, previewed, offset, and aligned with anchors; automatic beat/phrase inference remains outside the prototype because it is not authoritative for changing-tempo marching arrangements.
- The built-in assistant is deterministic. Cloud AI proposals will plug into the validated project command layer later.
- “In front of back hash” means toward the audience from the far hash; “behind front hash” means away from the audience from the near hash.
- GLB ingestion is architected through a validated semantic catalog; finished Blender meshes, retargeted clips, cinematic rendering remain production milestones.
- The undo stack is active during a session; cross-session version browsing will be added with the production project archive format.
# Apple-studio workspace refinement

- The MarchCraft wordmark in the editor is a keyboard-focusable Home control. Returning Home preserves the current project and exposes Resume.
- New Project uses a single inline setup screen instead of opening a creation dialog.
- Global project controls and contextual formation controls are separated into two calm command layers; formation shapes live in a labeled palette.
- Shared motion tokens animate navigation, popovers, buttons, and state changes, while honoring the Windows reduced-animation preference.
- A dedicated application icon and warmer three-note launch cue replace generic executable branding and notification-like sound.

## Application overhaul

- The model delegates domain implementation to focused persistence, music, formation,
  transition, and Clinic units, supported by stateless geometry and storage interfaces.
- Shared transition tables avoid rebuilding paths for every rendering/analysis query.
- Transition previews are isolated from project data, update only the selected subset,
  expose live distance/step/timing/collision metrics, and commit as one undoable action.
- Schema 12 adds optional `pathStartCount` and `pathDurationCounts`; missing values use
  full-transition timing and legacy delayed paths retain the former 25% start.
- Asynchronous MIDI imports and formation previews are discarded after intervening
  project changes. Generated previews still require Apply.
- Playback updates only motion roles; performer editing is one undoable operation.
- Clinic dismissal safely handles issues with attached metadata; timed collisions
  remain warnings while geometric crossings alone remain intentional choreography.
- The authorized human source, conditioned runtime GLBs, count-driven gait, validators, and preview tooling are active. Instrument-specific carriage meshes, modular uniforms, cinematic rendering remain production milestones.

### Shape drawer reliability

- The drawer exposes all eleven shapes: line, rectangle, circle, triangle, arc, ellipse, diamond, block, polygon, star, and spiral.
- Drag guides and performer dots use the same field-fitted geometry as placement. Circle/arc drags start at the center; other shapes use the dragged bounds (regular polygons retain their proportions).
- Releasing a drag opens an asynchronous assignment preview. Apply creates one undoable formation; Cancel leaves the project unchanged.
- Escape, Enter, right-click, selection changes, and switching drawing tools discard an unfinished drag. Choosing a shape opens the 2D field.
- Editing any advanced-builder option invalidates the previous preview, including assignment mode and grouping. The builder scrolls within the supported minimum window size.

Verification: `shapeDrawerGeometryAndTransactions` covers every shape with all six assignment modes and 1/7/24 performers, field fitting, preview immutability, Apply, cancellation, and undo/redo. `shapeDrawerMouseGestures` exercises real Qt input in both directions at three zoom levels. `tst_shapedrawer.qml` covers all drag option mappings and each advanced control's preview invalidation. Use `--qa-formation --screenshot output.png` for the advanced builder, optionally with `--qa-minimum`.

- Performer assignment preference is saved across builder reopenings and app restarts, and is also used by drawn shapes. Changing the preference still requires a fresh preview before Apply.
- The advanced builder and formation-review dialogs can be moved by dragging their title bars. The field stays undimmed; moving a dialog retains the pending preview. Escape/Close still cancels it.

### Field presentation

The field-style selector beside 2D/3D switches both views between Realistic field and Editor (8 to 5). This saved editor preference does not modify drill data or undo history. Editor mode uses a neutral graph with one square per 22.5-inch marching step: eight intervals (seven interior lines between boundary lines) per five yards, in both directions. Major horizontal grid lines are anchored at the front sideline. Grid overlays and snapping preferences remain independent; the editor graph always stays at one step.

Realistic mode uses project turf and venue colors, ten-yard end zones, regulation markings, and alternating mowing bands outdoors. High school and bowl venues include tiered seating, a press box, scoreboard and floodlight models in 3D, with seating bands in the 2D plan. Choose the stadium from the 3D venue selector; rehearsal and indoor venues remain available. Editor mode hides venue scenery. These are procedural prototype stadium models.

Screenshot QA accepts `--field-style realistic` or `--field-style editor` in either 2D or 3D.

### Movement editors and playback reliability

- Independent movement tabs hold their own pages/variants, MIDI or MusicXML, rehearsal audio, tempo/meter map, loop range, and opening behavior; the show shares its roster and field.
- Creation, duplication, renaming, reordering, and deletion are undoable. Existing shows migrate to a single movement; save/recovery includes all movements.
- MIDI rendering runs away from GUI/animation work. Volume and loop controls do not restart synthesis.
- Music measure clicks seek and select the corresponding editing page. Rectangle/lasso tools select the performers at their displayed positions after any timeline seek; field selection pauses playback.

### Drill chart and animation export

The native export workspace adds full-field vector PDF charts, configurable coordinate sheets, PNG pages, analytics CSV, Windows printing, and deterministic 2D/3D MP4 capture. It supports movement/set/variant/performer selection, embedded company branding plus optional monochrome MarchCraft branding, instruction continuation pages, paper/crop/layer controls, preview, local presets, and staged output publication. Video encoding uses an external user-selected FFmpeg; audio can be silent, an attached recording, or offline FluidSynth MIDI. This implements drill playback video export; cinematic camera sequencing remains outside the prototype.

The Export tab stays alongside Editor and provides automatic format-specific previews, show-based filenames, built-in and saved presets, paired chart/coordinate PDFs, independent performer label sizing, and an export-only black-and-white vector mark. Export jobs use immutable snapshots; progress remains visible while editing.
