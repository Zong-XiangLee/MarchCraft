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
- Meter-based, right-handed/Y-up 3D world with grounded, height-scaled human performers using count-driven modern corps-style marching technique
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
- Tabbed performer/field configuration, optional step grid, step/yard display units, shape resize/rotation handles, and a time-scaled drill timeline with dedicated reorder handles and an insertion marker
- New and imported shows begin with a zero-count opening set, matching coordinate sheets. Optional legacy opening holds remain available; ordinary holds use consecutive identical-coordinate sets.
- Freehand formation drawing for letters and organic forms, with cleanup/recognition, equal-spacing distribution, collision-aware placement, and shortest/balanced/expressive move assignment
- High-school, college, professional, and indoor field geometry
- Native MIDI/MusicXML measure, meter, tempo, and track import; unified drill/music/audio timeline; bundled FluidSynth MIDI playback; anchored rehearsal audio
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

For automated visual verification, pass `--screenshot output.png`; existing screenshot QA opens the bundled sample directly so scene baselines remain stable. Add `--qa-home` for the welcome workspace, `--qa-new-project` for the inline setup screen, or `--qa-shapes` for the editor shape palette. Combine any state with `--qa-minimum` to render the supported 1120 × 720 layout. Add `--3d` to capture the 3D viewport, or use `--3d-view overhead` / `--3d-view field`. Scene QA can also set `--venue venue.high_school`, `--lighting lighting.sunset`, `--graphics-profile presentation`. The native playback smoke check is `--qa-current-transition`; add `--qa-set-drag-preview` to capture the timeline insertion line and reorder preview. Use `--midi score.mid --audio rehearsal.wav` with screenshot QA to inspect aligned music and waveform lanes. Automated QA never plays the launch sound.

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
- The timeline has aligned **Drill**, **Music**, and **Audio** lanes on one elapsed-time ruler. Transition blocks end at their destination page; the opening hold appears before the music begins.
- Click a page to jump to its exact formation. Playback keeps running when already playing; stopped navigation displays the page for editing. Double-click pauses and opens the page editor. The editing page stays separate from the animated transition destination.
- Click or drag the ruler/playhead to scrub; release resumes only if playback was running. **Play** / Ctrl+Space resumes at the playhead. **Page tools → Play from selection** starts at the selected range's first page.
- Wheel-scroll all lanes together; Ctrl+wheel zooms around the pointer. **Fit show** displays the whole sequence. **Follow** scrolls with playback; manual scrolling disables it until re-enabled.
- Shift-click pages to select a loop range. Seeking within an enabled loop preserves its range; seeking outside disables looping without adding an undo entry. Navigation exits single-transition preview mode.
- Drag a page's dotted handle while stopped to reorder it with insertion feedback and undo. This does not trim durations; timing edits remain in the existing dialogs.
- Use **Music tools** to import MIDI/MusicXML, attach rehearsal audio, adjust offsets/synchronization, map pages, manage movements/parts, and open the MIDI track mixer. Drag or Shift-click musical measures to select them; double-click a measure to seek to its start. Page generation retains its preview and explicit **Create sets** step.
- MIDI plays through bundled FluidSynth and the GeneralUser GS bank. The waveform uses decoded audio timestamps and the same offset/anchor mapping as playback. The sequence spans the longest of drill, imported music, and rehearsal audio: final formations hold after drill ends and drill continues silently after audio ends.

## Licensing note

OpenMarch was consulted only as a public behavioral reference. MarchCraft is a clean-room implementation. A final project license should be selected before public distribution.

## Application architecture and human rendering

`DrillProject` remains the QML model and transaction coordinator. Its persistence,
formation, Clinic, transition, and music adapters have separate implementation units.
`ProjectStorage` owns SQLite I/O, `ProjectAlgorithms` provides stateless geometry and
assignment functions, and `TransitionPath` supplies reusable arc-length tables.
Path tables are invalidated by project edits and live geometry changes; playhead
updates notify only position, facing, and animation roles. Background imports are revision-checked,
and formation workers do not instantiate multimedia resources.

The welcome workspace, command bars, roster, inspector, timeline, and settings dialogs
are separate QML components with explicit dependencies supplied by the application shell.
Performer dialog submissions retain name/notes and undo as one action.

The 3D preview uses the authorized rigged LowPolyBoy source conditioned to the
`marchcraft.canonical.v1` skeleton. The runtime gait is count-driven and supports
forward heel-to-toe technique, backward forefoot technique, slides, diagonal travel,
planted direction changes, and a controlled close to attention. Body, uniform, skin,
and height fields remain project-compatible and feed the renderer; semantic assets
continue to resolve through the catalog rather than source filenames.

The 3D controls offer **Horns up / Horns down** carriage and optional **Mark time on
holds**. These are session preview choices; stationary sets otherwise remain at
attention. Instrument meshes and per-set horn choreography are not included.
Stride is scaled to performer height and limited to the validated 0.70-meter
canonical reach; moves outside that range retain their authored coordinates and
the existing Clinic stride warnings. The mesh uses the source character's appearance;
uniform IDs remain compatible but do not yet select modular clothing.

Human QA uses the actual Qt skin and is available with `--qa-human --screenshot
output.png`. Add `--qa-human-count 2.5` to sample marching, `--qa-human-heading 90`
for Side 2 travel, `--qa-human-horns-down`, or `--qa-human-mark-time`. Use
`--3d-view performer-side` for a close side view. The QA performer stays centered
in the camera at the requested count. `tst_humanrig.qml` checks actual Qt joint
positions, carriage, mark time, height scaling, and world-space planted ankles.

Run `marchcraft_tests samplePerformance -o timings.txt,txt` for repeatable sample-show
load/save, set-switching, edit, formation-preview, and position-evaluation timings.
These diagnostics are measurements, not hardware-independent performance thresholds.
