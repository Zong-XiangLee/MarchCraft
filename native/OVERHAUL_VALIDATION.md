# Application overhaul validation

Validated on Windows with the existing Qt 6.8.3 / MinGW 13 toolchain in
`build-worktree-mingw`, with production-preview synchronization disabled.

## Changes reviewed

- Persistence/import/export, formation assignment, transitions, Clinic analysis,
  music/transport/synthesis, workspace state, asset resolution, and QML surfaces.
- Domain adapters now occupy separate translation units. `DrillProject` retains
  state ownership and command coordination; this is not a replacement persistence
  format or a wholesale conversion to independent QObject services.
- Stateless geometry/assignment and SQLite interfaces support those adapters.
  Shared arc-length tables serve playback and analysis, with edit invalidation.
- Human runtime geometry, gait, GLBs, roles, catalog entries, and controls are
  retired. Source assets/scripts/licenses remain inactive references. Existing
  appearance settings survive editing, undo/redo, and save/load.
- Fixed stale asynchronous results, performer-dialog transaction/data loss,
  saved-state undo tracking, Clinic dismissal metadata, malformed MIDI track
  boundaries, synth/device sample-rate mismatch, and playback with no MIDI score.

## Tests and visual checks

- CTest passes both `marchcraft_tests` and `ui_qml`.
- Direct test logs: 55 C++ test entries and 8 QML entries passed (including
  initialization/cleanup entries), with no failures or skips.
- Coverage includes legacy appearance round trips, persistence/export, undo/redo,
  timing, formation Apply/cancel, stale jobs, field transforms, marker grounding,
  equipment/prop clearance, and transport advancement/reset.
- Screenshot inspection covers welcome, setup, editor, selection/shape palette,
  preferences, performer dialog, music, and 3D. Normal 1520 x 940 and minimum
  1120 x 720 layouts were exercised. Narrow inspector and music controls were
  adjusted after inspection.
- Both 2D and 3D current-transition executable smoke checks pass. Reviewed startup
  logs contain no QML binding errors. Screenshots/logs remain ignored build artifacts.
- The baseline suite had one stale expectation for a geometric crossing issue;
  the test now verifies the intentional behavior: report timed collisions without
  separately warning about every geometric crossing.

## Local diagnostic timings

Same bundled 204-performer, 97-set show and test operations; milliseconds:

| Operation | Before | After |
| --- | ---: | ---: |
| Import sample | 251 | 231 |
| Save sample | 204 | 200 |
| Switch through all sets | 5208 | 775 |
| Select all and nudge | 339 | 220 |
| Synchronous formation preview | 8 | 11 |
| Evaluate positions/facing for 120 frames | 14 | 10 |

These are single-run development diagnostics, not statistically controlled
benchmarks. Save and small formation timings should be treated as effectively
unchanged. The position test is model evaluation, not GPU frame rate. Initial
3D launch-to-screenshot was 6590 ms; a later normal-size marker capture took
4797 ms, both including the fixed screenshot delay. No comparative GPU FPS or
long-duration playback claim is made.
