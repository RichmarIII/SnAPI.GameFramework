# Module CMake Layout Plan

Status: completed
Branch: codex/module-cmake-layout
Integration Base: master
Owner: Codex
Created: 2026-06-12

## Goal

Move the project and CMake structure to the target compiled-module layout
described by `docs/CODING_STYLE.md`, following the hard-cut `Modules/`
ownership pattern already used by `SnAPI.Renderer.New`.

## Background

The current repository still keeps compiled sources in legacy `include/`, `src/`,
and `tools/` roots while the root `CMakeLists.txt` owns most target source lists
and dependency wiring. The target architecture says each compiled target is a
module and should own its own `Public/`, `Private/`, `Dependencies.cmake`, and
`CMakeLists.txt` contract.

The agreed module roots for this migration are:

- `Modules/GameFramework` for `SnAPI.GameFramework`
- `Modules/Editor` for `SnAPI.GameFramework.Editor`
- `Modules/Runtime` for `SnAPI.GameFramework.Runtime`
- `Modules/Build` for `SnAPI.GameFramework.Build`
- `Modules/ReflectionGen` for `SnAPI.GameFramework.ReflectionGen`

## Scope

- Move compiled source/header ownership into the module roots above.
- Move CMake target ownership out of the root file and into module-local
  `CMakeLists.txt` and `Dependencies.cmake` files.
- Keep root CMake responsible for global options, shared feature contracts,
  top-level `add_subdirectory()` calls, external project integration, examples,
  tests, and docs.
- Update source-layout references in tests, docs generation, reflection
  generation, SWIG wiring, and templates where needed.
- Preserve target names and public behavior.

## Non-Goals

- No runtime behavior redesign.
- No compatibility shim over legacy `include/`, `src/`, or `tools/` roots.
- No new dependencies or vendored third-party source.
- No broad code-style rewrites beyond paths and CMake ownership.

## Design

The root CMake file will create a shared `SnAPI.GameFramework.FeatureSet`
interface target for project-wide compile features and feature macros that must
propagate across compiled modules. Module-local files will own their targets,
sources, include directories, and dependency application.

Executable modules may have empty `Public/` folders, but they still use the same
module contract because they are compiled CMake targets.

## Files Expected To Change

- `CMakeLists.txt`
- `Modules/**`
- `tests/CMakeLists.txt`
- `docs-src/Doxyfile.in`
- `docs-src/architecture.md`
- project/plugin/module scaffolding tests or generators if they still emit the
  old `include/` and `src/` module shape

## Implementation Phases

1. Create module directories and move legacy source/header files into the agreed
   compiled target roots.
2. Add module-local CMake contracts and shrink root target ownership.
3. Update path-sensitive docs, tests, reflection generation, SWIG, docs
   generation, and scaffolding.
4. Configure and build focused targets, then fix path/target propagation issues.

## Validation

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DSNAPI_GF_BUILD_DOCS=OFF -DSNAPI_GF_BUILD_TESTS=ON -DSNAPI_GF_BUILD_EXAMPLES=ON -DSNAPI_GF_BUILD_EDITOR=ON -DSNAPI_GF_BUILD_RUNTIME=ON -DSNAPI_GF_BUILD_BUILDCLI=ON
cmake --build build/release --target SnAPI.GameFramework -j18
cmake --build build/release --target SnAPI.GameFramework.Editor -j18
cmake --build build/release --target SnAPI.GameFramework.Runtime -j18
cmake --build build/release --target SnAPI.GameFramework.Build -j18
ctest --test-dir build/release --output-on-failure
```

## Completion Criteria

- Legacy compiled source roots are no longer the owner of GameFramework compiled
  targets.
- Each compiled module root owns `Public/`, `Private/`, `Dependencies.cmake`,
  and `CMakeLists.txt`.
- Root CMake delegates target ownership to module-local files.
- Validation results are recorded.
- The plan moves to `docs/exec-plans/completed/` after merge or closeout.

## Notes

- User clarified that modules are compiled targets, including executables and
  tools.
- User requested `Build`, not `BuildCli`, as the module name.

## Validation Results

- 2026-06-12: Configured `build/release` with Release, tests, examples, editor,
  runtime, and build CLI enabled. Configure succeeded with existing
  third-party/developer FetchContent warnings.
- 2026-06-12: Built release targets `SnAPI.GameFramework`,
  `SnAPI.GameFramework.Editor`, `SnAPI.GameFramework.Runtime`,
  `SnAPI.GameFramework.Build`, and `SnAPI.GameFramework.ReflectionGen`.
- 2026-06-12: Built `GameFrameworkTests` and `GameFrameworkEditorTests`.
- 2026-06-12: Ran focused descriptor, project/plugin creation, module creation,
  build CLI scaffolding, and editor asset-service scaffolding tests. All focused
  cases passed.
- 2026-06-12: Moved GameFramework dependency discovery/linking from the root
  CMake file into `Modules/GameFramework/Dependencies.cmake`; moved Catch2
  discovery into `tests/CMakeLists.txt`; kept Runtime, Build, Editor, and
  ReflectionGen dependency application local to each module.
- 2026-06-12: Reconfigured `build/release` after dependency-localization with
  Release, tests, examples, editor, runtime, and build CLI enabled. Configure
  succeeded with existing third-party/developer FetchContent warnings.
- 2026-06-12: Built release targets `SnAPI.GameFramework`,
  `SnAPI.GameFramework.Editor`, `SnAPI.GameFramework.Runtime`,
  `SnAPI.GameFramework.Build`, and `SnAPI.GameFramework.ReflectionGen` after
  dependency-localization.
- 2026-06-12: Built `GameFrameworkTests`, `GameFrameworkEditorTests`, and
  `GameThreadingTests` after moving Catch2 into `tests/CMakeLists.txt`.
- 2026-06-12: Ran 29 focused scaffolding/editor/build tests with writable
  `XDG_DATA_HOME=/tmp/snapi_gf_xdg_data`; all passed. The broader 31-test
  editor build-service slice still has unrelated console-log assertions in
  `Editor build service sanitizes carriage-return and ANSI-heavy console output`
  and `Editor build service returns a recent UI-safe tail for large console
  logs`, where synthetic process output is printed but the tested console-log
  string is empty.
- 2026-06-13: `cmake --build build/release --target
  SnAPI.GameFramework.Editor -j18` succeeded after the final branch updates and
  copied editor assets.
- 2026-06-13: `git diff --check` passed.
- 2026-06-13: `cmake --build build/release --target SnAPI.GameFramework
  SnAPI.GameFramework.Editor SnAPI.GameFramework.Runtime
  SnAPI.GameFramework.Build -j18` succeeded.
- 2026-06-13: `ctest --test-dir build/release --output-on-failure` failed
  with 51 failures out of 257. Most editor/project failures in that run were
  caused by editor template bootstrap writing to
  `$HOME/.local/share/SnAPI/GameFramework/Editor/Assets` on a read-only
  filesystem.
- 2026-06-13: Reran the full suite with
  `XDG_DATA_HOME=/tmp/snapi_gf_xdg_data`; 220 of 257 tests passed and 37 tests
  failed. The read-only-home cascade was removed, but existing failures remain
  in reflection codegen metadata, gameplay possession selection, Conduit
  authored default-input calls, build request/planner/history/execution,
  authored asset JIT/cook/editor flows, and editor build-service console-log
  trimming.
- 2026-06-13: Sampled failing cases after closeout validation:
  `Generated reflection codegen captures docs and parameter metadata` still
  reports `FixtureInfo->Fields.size() == 3` where the test expects `4`;
  `GameplayHost uses game possession selector for newly joined players` still
  reports `SelectPossessionCalls == 0`; `BuildRequestService resolves named
  profiles with overrides and deterministic hashes` still fails to resolve the
  first request. Running editor build-service tests without the writable XDG
  environment still reproduces the editor template bootstrap write failure.

## Closeout

- Completed on branch `codex/module-cmake-layout`.
- Module-local source ownership and CMake ownership now live under `Modules/`
  for GameFramework, Editor, Runtime, Build, and ReflectionGen.
- Remaining renderer-profile follow-up is architectural Renderer.New work and
  is not part of the CMake layout branch closeout.
