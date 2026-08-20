# AGENTS.md

## Repository overview

MarchCraft is a native Windows-first marching-band drill editor prototype. The application is written in C++20 with Qt 6 and QML; it does not use a web UI, Electron, or an embedded browser.

- `native/src/`: C++ domain model, playback/music, asset catalog, and scene helpers.
- `native/qml/`: Qt Quick UI, field/3D views, dialogs, and gait logic.
- `native/tests/`: QtTest C++ tests and QML tests.
- `native/assets/`: runtime GLB assets and the semantic asset catalog.
- `native/scripts/assets/`: asset generation and validation scripts.
- `data/coordinates.json`: bundled sample show data.
- `Models/`: source Blender/GLB files and attribution/license notes.
- `native/README.md` and `native/FEATURES.md`: product behavior and prototype boundaries.

## Build and test

Use a Qt developer shell on Windows with Qt 6.6+ (the checked-in helper script targets a local Qt 6.8.3/MinGW toolchain), CMake, and a supported C++ compiler.

From the repository root, the usual worktree build/run flow is:

```powershell
.\native\scripts\build-and-run.ps1
```

This configures `native/build-worktree-mingw`, builds the `marchcraft` target, deploys Qt runtime files, and launches the executable. It intentionally disables production-preview synchronization for worktree builds.

For a conventional CMake build from `native/`:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite contains the `marchcraft_tests` QtTest executable and the `human_gait_qml` QML test. If Qt is not available in the current shell, report that limitation rather than changing toolchain configuration or vendoring dependencies.

## Development conventions

- Keep the C++ standard at C++20 and preserve CMake's Qt automoc/autorcc/QML-module setup.
- Put domain and persistence behavior in `native/src`; keep presentation and interaction wiring in `native/qml`.
- Expose C++ state to QML through the existing `QObject` properties, signals, invokable methods, and model roles. Preserve notification behavior when adding or changing properties.
- Use Qt types and Qt containers consistently with nearby code. Follow the existing brace style, `QStringLiteral` usage, and `m_` member naming.
- Preserve the separation between semantic asset IDs and filenames. Runtime code should resolve assets through `native/assets/catalog.json` rather than hard-coding Blender source filenames.
- Keep coordinate and scene conventions intact: audience perspective has Side 1 on the left and Side 2 on the right; the 3D world is meter-based, Y-up, and grounded at `Y=0`.
- Treat project mutations as commands/transactions so undo/redo, dirty state, and recovery behavior remain correct. Background formation/freehand optimization must continue to require explicit Apply behavior.
- Prefer small, focused changes. Avoid broad formatting churn, unrelated refactors, and changes to generated or bundled third-party files.

## Assets

When changing performer rigs, gait constants, or runtime GLBs, update the semantic catalog and run the relevant validators:

```powershell
python native/scripts/assets/validate_human_performer.py native/assets/performer/human_performer.glb
python native/scripts/assets/validate_human_gait.py native/assets/performer/human_performer.glb
python native/scripts/assets/render_human_performer_preview.py native/assets/performer/human_performer.glb human-gait-preview.bmp
```

Follow `native/assets/blender/README.md` for rig, socket, clip, scale, ground-contact, LOD, and licensing requirements. Preserve license/attribution files when adding or replacing models. Do not add user-supplied GLBs directly to the runtime catalog without validation metadata.

## Verification expectations

For C++ or QML behavior changes, run the relevant Qt tests, ideally the full `ctest` suite. For field geometry, playback, persistence, undo/redo, asset catalog, or gait changes, add or update a focused test alongside the implementation. For UI or 3D changes, use the executable's screenshot QA flags documented in `native/README.md` when the required Qt runtime is available.

Before handoff, inspect `git diff` and `git status`; do not commit generated `native/build*`, `native/dist`, `native/output`, temporary files, Qt toolchains, or other ignored artifacts.

## Licensing and project boundaries

MarchCraft is a clean-room native implementation. OpenMarch was consulted only as a public behavioral reference; do not copy source code. Keep the existing prototype boundaries and document intentional behavior changes in `native/FEATURES.md` or `native/README.md` when appropriate.

# Git workflow

- main is the canonical production branch.
- Never commit directly to main.
- Work only on the current task branch/worktree.
- Do not create additional branches unless necessary.
- Do not switch branches without explicit reason.
- Before coding, fetch the latest main.
- Keep changes scoped to the current feature.
- Run the relevant build/tests before committing.
- When the task is complete:
  - review the diff
  - create a clean commit
  - push the current branch
  - prepare a pull request into main
- Never force-push main.
- Never delete main.
- Never merge another feature branch into the current branch unless explicitly requested.
- Do not commit secrets, .env files, build outputs, or temporary files.

# Development behavior

- Start large changes by inspecting the relevant code and proposing a short plan.
- Prefer small, incremental changes over broad rewrites.
- Reuse existing patterns in the repository.
- Fix build/test errors caused by the task before declaring completion.
- If unrelated issues are found, report them rather than changing them automatically.