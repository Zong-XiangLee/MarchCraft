# MarchCraft

MarchCraft is a native marching-band drill editor prototype written in C++20 with Qt 6. It does not use HTML, Electron, or an embedded browser.

MarchCraft now opens to a native welcome workspace instead of silently loading a show. Start a
new project, open one of up to eight recent projects, resume the current show, or open the pinned
Rancho Bernardo sample as an editable copy. The graphite desktop theme, restrained motion, and
optional launch sound are designed for a focused professional editing workflow. The application
ships with dedicated MarchCraft window/executable branding and a warm three-note launch cue that
avoids resembling a Windows notification.

Version 0.6 expands the offline Drill Clinic with path-sampled performer, instrument,
and moving-prop clearance checks plus previewable fixes. It also adds live set-card
reordering with insertion feedback and timing reconstruction, and configurable opening
sets whose counts match coordinate-sheet rows; a hold is represented by consecutive sets with identical coordinates.
Formation and freehand optimization still run in the background and require an explicit
Apply action before project data changes.

## What is included

- Native 2D drill editor and synchronized stylized 3D preview
- Meter-based, right-handed/Y-up 3D world with the performance surface at `Y=0` and lightweight colored directional performer markers
- Versioned semantic asset catalog for instruments/equipment, props, and venues; equipment assignments remain available for clearance analysis
- Configurable rehearsal field, high-school stadium, bowl, school-gym, and indoor-arena environments with daylight, overcast, sunset, night, and indoor lighting
- Static/movable prop domain model with built-in box, panel, platform, and podium assets
- Drill Clinic collision review accounts for performer equipment footprints and oriented static or moving props, with field highlighting and previewable reroutes or destination shifts
- Regulation front/back field layout with ten-yard end zones, yard numbers, corrected HS/NCAA/NFL hashes, five-yard lines, and exactly four vertical one-yard inserts per five-yard interval
- Audience-perspective coordinates: Side 1 is left, Side 2 is right, front is the near/bottom side of the editor, and back is the far/top side
- Roster, section, label, uniform-color, and instrument assignments
- Set/subset timeline, variant-local performer groups, single-transition and whole-show playback, paths, formations, snapping, axis locking, and undo/redo
- Adaptive equal-distance spirals with half-step turns and direction controls, plus optional shape-created groups
- Persisted colored-symbol, compact-dot, and black-dot marker presets with adjustable sizing
- Constant-speed arc-length playback with uninterrupted whole-show timing
- Tabbed performer/field configuration, optional step grid, step/yard display units, shape resize/rotation handles, and a live horizontally draggable set timeline with an insertion marker
- New and imported shows begin with a zero-count opening set, matching coordinate sheets. Optional legacy opening holds remain available; ordinary holds use consecutive identical-coordinate sets.
- Freehand formation drawing for letters and organic forms, with cleanup/recognition, equal-spacing distribution, collision-aware placement, and shortest/balanced/expressive move assignment
- High-school, college, professional, and indoor field geometry
- Native MIDI/MusicXML measure, meter, tempo, and track import; compact music timeline; bundled FluidSynth MIDI playback; anchored rehearsal audio
- Distance analytics, step-size warnings, spacing/collision checks, and coordinates
- Transactional `.marchcraft` project files with legacy `.drill` import, automatic recovery, redesigned multi-page PDF coordinate sheets, and CSV export
- The supplied `data/coordinates.json` is bundled as the default 204-performer, 97-set test show

See [FEATURES.md](FEATURES.md) for the parity inventory and prototype boundaries.
Blender artists should follow [assets/blender/README.md](assets/blender/README.md); runtime asset identities and validation metadata live in [assets/catalog.json](assets/catalog.json).

## Build on Windows

Install Qt 6.6 or newer with the Qt Quick, Qt Quick 3D, Qt Multimedia, and Qt Shader Tools components, plus CMake and either MSVC 2022 or MinGW 13.

From a Qt developer shell:

```powershell
cmake -S . -B build-worktree-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DMARCHCRAFT_SYNC_PRODUCTION_PREVIEW=OFF
cmake --build build-worktree-mingw
ctest --test-dir build-worktree-mingw --output-on-failure
```

Run `build-worktree-mingw/marchcraft.exe` (or the configuration-specific executable produced by the selected generator).

Worktree builds use `build-worktree-mingw` and disable production-preview synchronization. Use `scripts/build-and-run.ps1` for the configured Windows toolchain; do not create a second build directory.

For automated visual verification, pass `--screenshot output.png`; existing screenshot QA opens the bundled sample directly so scene baselines remain stable. Add `--qa-home` for the welcome workspace, `--qa-new-project` for the inline setup screen, or `--qa-shapes` for the editor shape palette. Combine any state with `--qa-minimum` to render the supported 1120 × 720 layout. Add `--3d` to capture the 3D viewport, or use `--3d-view overhead` / `--3d-view field`. Scene QA can also set `--venue venue.high_school`, `--lighting lighting.sunset`, `--graphics-profile presentation`. The native playback smoke check is `--qa-current-transition`; add `--qa-set-drag-preview` to capture the live insertion line and neighboring-card displacement state. Automated QA never plays the launch sound.

## Controls

- Double-click an empty field location to add a performer.
- Click a grouped performer to select its group; Ctrl-click targets an individual member.
- Click or right-click a grouped performer to select its full group. The context menu offers Group, Remove from group, and Ungroup only when their selection requirements are satisfied.
- Right-click roster entries for the same group controls, or right-click set cards to copy, insert, archive, and edit sets. Unavailable commands remain visible in a muted disabled state.
- Drag selected performers; hold Shift to lock vertical movement or Control to lock horizontal movement.
- Arrow keys move the selection by a quarter-step.
- Use **Shape** to distribute selected performers on the full formation library; enable **Create as group** when desired.
- Select an entire persistent shape to reveal corner resize handles and the rotation handle; resizing preserves the shape and performer spacing.
- Use **Freehand**, choose cleanup and movement behavior, then draw directly on the field. Straight strokes and circles can be recognized automatically; handwriting is smoothed while retaining its form.
- Select a destination set and press **Play** to preview its transition.
- Press **Play show** to animate every set in sequence using each set's counts and tempo.
- Open the **Music** timeline tab and import MIDI to select measures, preview 8/16/32-count subdivisions or one long move, and create synchronized sets.
- Use **Tracks** as a MIDI mixer for mute, solo, and per-track volume. MIDI plays through the bundled FluidSynth and GeneralUser GS bank without requiring rehearsal audio.

## Licensing note

OpenMarch was consulted only as a public behavioral reference. MarchCraft is a clean-room implementation. A final project license should be selected before public distribution.

## Application architecture and deferred human rendering

`DrillProject` remains the QML model and transaction coordinator. Its persistence,
formation, Clinic, transition, and music adapters have separate implementation units.
`ProjectStorage` owns SQLite I/O, `ProjectAlgorithms` provides stateless geometry and
assignment functions, and `TransitionPath` supplies reusable arc-length tables.
Path tables are invalidated by project edits and live geometry changes; playhead
updates notify only position and facing roles. Background imports are revision-checked,
and formation workers do not instantiate multimedia resources.

The welcome workspace, command bars, roster, inspector, timeline, and settings dialogs
are separate QML components with explicit dependencies supplied by the application shell.
Performer dialog submissions retain name/notes and undo as one action.

The human renderer, gait runtime, and bundled human GLBs are intentionally retired.
Old body, uniform, skin, and height fields are retained for project compatibility but
have no rendering controls. Source models, attribution, and authoring scripts remain
as inactive references for a future implementation; they are not part of the build.
The marker preview retains field positions, authored facing, visibility, selection,
venues, props, cameras, and synchronized playback.

Run `marchcraft_tests samplePerformance -o timings.txt,txt` for repeatable sample-show
load/save, set-switching, edit, formation-preview, and position-evaluation timings.
These diagnostics are measurements, not hardware-independent performance thresholds.
