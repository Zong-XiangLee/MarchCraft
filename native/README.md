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

For automated visual verification, pass `--screenshot output.png`; existing screenshot QA opens the bundled sample directly so scene baselines remain stable. Add `--qa-home` for the welcome workspace, `--qa-new-project` for the inline setup screen, or `--qa-shapes` for the editor shape palette. Combine any state with `--qa-minimum` to render the supported 1120 × 720 layout. Add `--3d` to capture the 3D viewport, or use `--3d-view overhead` / `--3d-view field`. Scene QA can also set `--venue venue.high_school`, `--lighting lighting.sunset`, `--graphics-profile presentation`. The native playback smoke check is `--qa-current-transition`; add `--qa-set-drag-preview` to capture the timeline insertion line and reorder preview. Use `--midi score.mid --audio rehearsal.wav` with screenshot QA to inspect aligned music and waveform lanes. Add `--qa-movements` to verify movement tabs with long names. `--midi score.mid --qa-midi-synth` runs four seconds of animated MIDI playback and fails on audio underruns. Automated QA never plays the launch sound.

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
- Use **Music tools** to import MIDI/MusicXML, attach rehearsal audio, adjust offsets/synchronization, map pages, manage colored music sections/parts, and open the MIDI track mixer. Drag or Shift-click musical measures to select them; click a measure to seek to its start. Explicit seeks retarget editing to the most recently reached drill page; rectangular and lasso selection hit-test the displayed positions. Page generation retains its preview and explicit **Create sets** step.
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

### Independent movement editors

The movement bar above the field switches between independent drill and music editors in one show. **+ Movement** starts with the current formation, one opening page, and no imported music or audio. **Manage** offers rename, duplicate, reorder, and undoable deletion. Duplicate copies the active movement's pages, variants, groups, music, timing, and audio mapping.

The roster, field, and scene settings belong to the show. Adding/removing performers updates every movement; their placements remain independent. Switching tabs stops playback, clears performer selection and pending formation previews, restores the movement's editing page and measure range, and scrolls the timeline to its zero origin. Switching does not dirty the show or occupy undo history. Undoing an edit in another movement returns to that movement. PDF/CSV exports and music/coordinate mapping operate on the active movement; full coordinate JSON import still replaces the show.

Project schema 11 stores all movements in the existing `.marchcraft` database. Earlier projects open as Movement 1; older app versions reject schema 11 rather than discard the additional movements. MIDI synthesis and audio delivery run on a dedicated thread with precomputed event sample positions, independently of GUI animation. Playback position uses processed audio time, and gain/loop toggles preserve the active audio stream.

## Export workspace

Use the persistent **Editor / Export** tabs or **File > Export drill charts, sheets, images or video**. Switching tabs keeps the editor mounted and preserves its selection, zoom and movement. The coordinate-sheet and CSV shortcuts open the same workspace with their output type selected.

1. Choose PDF, PNG pages, analytics CSV, native printing, 2D MP4, or 3D MP4. Select the entire show, active movement, or individual movements. Set numbers accept comma-separated entries and inclusive ranges (for example `1-8, 12, 14A`); blank includes every set. Choose active/all/individual variants, include or exclude subsets, and optionally filter performer labels or sections.
2. Choose paper size, orientation, margins, marker/text sizes, grid/label/symbol/prop/instruction layers, color or monochrome, and full-field/fit/custom framing. Custom crops report excluded performers. Charts preserve director perspective and the eight-to-five coordinate system. Instructions use each variant's multiline caption and continue onto additional pages when needed.
3. Import a PNG/JPEG company logo and enter the company name. **Save branding to show** stores normalized image data in the project, supports undo/redo, and travels with the project. The optional monochrome MarchCraft logo sits beside the company logo. The export-only vector MarchCraft mark uses solid black and white; application branding is unchanged. Automatic previews use draft branding without changing the project.
4. Preview updates automatically after changes. PDF/PNG/print show pages with zoom and navigation; CSV shows the exact exported table; 2D/3D video supports silent visual playback and scrubbing with the selected camera. Performer labels default to 6 pt (adjustable 4–18 pt), independently of page text. Review the preview and page list. Choose an output file, or a folder for PNG pages/separate movement PDFs. The Location and Filename controls suggest a show-based name and seed Browse with a complete filename. PNG filename prefixes and 150/300/600 DPI are configurable. Built-in presets include drill diagrams, performer coordinates, paired diagrams + coordinates, PNG, 2D video and 3D video. The paired preset publishes two PDFs (or two per movement when split), with independent page numbering. Existing outputs require **Replace existing files**. Reusable export presets and recent destinations are local application preferences.
5. Export, monitor progress, or cancel. You can return to Editor during export; the header continues showing job progress, and edits do not affect the job snapshot. Files are staged before publication; each destination is replaced atomically. A multi-file publication error reports the files already published. Open the result or destination folder after success. Printing opens the Windows printer dialog.

Export works on an isolated snapshot. It does not change the editor's current movement, selection, playback position, or undo history. Changing show branding is a separate undoable project edit. Archived sets and archived variants are not exported.

### Video

Choose overhead 2D charts or the existing 3D scene with director, overhead, or field camera; select 720p/1080p and 30/60 fps. Frame sampling follows the drill timeline, transition paths, gait, and prop paths. Disjoint set selections are cut together in show order; no artificial transition is added between them. Zero-duration opening sets contribute no video frames. Opening holds are silent before movement music begins.

MP4 encoding requires an installed **FFmpeg executable with libx264 and AAC encoders**. Locate it in the export settings or make it available on PATH. MarchCraft does not download an encoder. Document exports do not require FFmpeg. Choose silence, an attached recording (including timeline offsets/audio anchors), or offline MIDI synthesis. MIDI uses the deployed FluidSynth runtime and GeneralUser GS SoundFont and does not require an audio output device. Missing assets/audio/encoder errors are reported without publishing an incomplete output. The 3D rendering window stays open during frame capture; closing it cancels the job.

### Grid and export QA

The standard editor grid has one-step squares, darker gray four-step midlines, and five-yard lines every eight steps. **Preferences > Field > Standard 8-to-5 grid** restores this preset; custom grid spacing and snapping remain separate controls.

- `--qa-export charts.pdf`: export the first two sample sets and exit with status 0 on success.
- `--export-format pdf|png|csv|video2d|video3d`: select the QA output; PNG destination is a folder.
- `--export-content charts|coordinates|both`: select document content; `both` uses a destination folder.
- `--qa-export-preset "Coordinates + drill diagrams" --screenshot packet.png`: inspect a built-in preset; `csv` selects the table preview.
- `--qa-export-fixture`: use a small deterministic two-set fixture.
- `--export-audio silent|recording|midi`, `--midi score.mid`, `--audio recording.wav`, and `--ffmpeg path`: exercise video/audio integration.
- `--qa-export-dialog --screenshot export-dialog.png`: inspect the export workspace.

The export template is configurable, not a freeform page designer. Pyware project import/export is not implemented. Extremely dense formations may still need smaller labels, a larger paper size, or a focused crop for rehearsal readability.

### Appearance and camera navigation

Editor preferences → General → Appearance offers Graphite (blue), Midnight (teal),
and Warm charcoal (amber). The choice applies immediately and is remembered on
this computer independently of project undo/history. Screenshot QA can use
`--qa-theme graphite`, `--qa-theme midnight`, or `--qa-theme warm` without changing
the saved preference. Use `--field-preset indoor` to verify the wood surface.

In 3D, drag to orbit, Shift-drag or right/middle-drag to pan, and scroll to dolly.
Double-click returns to the press-box view. Preset changes ease between views;
manual movement uses a shorter settling time. Windows reduced-motion settings
remove these animations. Orbit elevation and dolly distance are bounded to keep
normal navigation above the ground; pan speed follows distance and viewport size.

Venues use original procedural Qt geometry: rehearsal fencing and benches,
track lanes and segmented stadium terraces, press-box glazing, bowl end seating,
and open-front indoor halls with banners and retractable seating. Turf grain and
wood planks use deterministic mipmapped textures. The audience-side cutaway keeps
the drill readable; these remain stylized venues, not photorealistic Blender assets.
Field coordinates, regulation markings, and performer ground height are unchanged.
