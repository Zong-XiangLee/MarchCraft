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
| Animation | Set-to-set or continuous whole-show playback with visible marcher paths and per-set count/BPM timing |
| Roster | Add, edit, remove, batch-create, search, multi-select, custom name/section/instrument/notes |
| Sets | Add, edit, delete, duplicate, batch-create, and mark subsets |
| Drill editing | Drag, keyboard nudge, axis lock, grid snap, grouping/context actions, compact group-colored dot markers plus circle/square/diamond markers, per-set facing with animated turns, persistent shape library, adaptive spirals, and equal-distance distribution |
| Saving | Atomically replaced local project file, per-project crash recovery prompt, 20-version adjacent history, undo/redo |
| Music | Native MIDI/MusicXML timing and track import, compact wheel-scrollable measure timeline with set-range highlighting, persistent color-coded movements and parts, built-in FluidSynth playback, count-based set generation, step-mode overrides, waveform audio and synchronization anchors |
| Coordinates | Audience-perspective coordinates: Side 1 left, Side 2 right; front sideline/hash toward the audience, back hash/sideline away from it; redesigned landscape performer sheets with movement analytics, repeated headers, pagination, and full CSV export |
| Analytics | Per-move and total distance, ensemble average, longest move, collision and step-size warnings |
| 3D | Meter-based Y-up world with grounded colored directional markers, authored facing and selection; rehearsal/stadium/gym/arena environments, lighting/quality presets, props, and press-box/overhead/field cameras. Human models and gait are deferred; old appearance values remain compatible. |
| Platforms | Qt/CMake architecture supports Windows first and portable macOS/Linux builds |
| Bundled test show | The supplied Rancho Bernardo data opens by default with 204 performers and 97 sets |

## Prototype boundaries

- Transition paths are currently direct interpolations. The domain boundary is ready for curved, follow-the-leader, gate, and counter-march path implementations.
- Audio can be attached, previewed, offset, and aligned with anchors; automatic beat/phrase inference remains outside the prototype because it is not authoritative for changing-tempo marching arrangements.
- The built-in assistant is deterministic. Cloud AI proposals will plug into the validated project command layer later.
- “In front of back hash” means toward the audience from the far hash; “behind front hash” means away from the audience from the near hash.
- GLB ingestion is architected through a validated semantic catalog; finished Blender meshes, retargeted clips, cinematic rendering, and video export remain production milestones.
- The undo stack is active during a session; adjacent saved-version history is available through **File → Restore version…** and restored states remain undoable until saved.
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
- Asynchronous MIDI imports and formation previews are discarded after intervening
  project changes. Generated previews still require Apply.
- Playback updates only motion roles; performer editing is one undoable operation.
- Clinic dismissal safely handles issues with attached metadata; timed collisions
  remain warnings while geometric crossings alone remain intentional choreography.
- Human source references and authoring tools are retained but inactive.
