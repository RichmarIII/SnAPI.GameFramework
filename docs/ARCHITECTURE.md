# SnAPI.GameFramework Architecture

Durable architecture context for `SnAPI.GameFramework`.

Version: 1.0 Draft
Target Audience: Gameplay Framework, Editor, Asset Pipeline, Rendering
Integration, Build System, Host Integration
Status: Working Design
Primary Language: C++23
Primary Design Goal: Provide a world-owned gameplay framework with stable
identity, reflection-driven authoring, source/cooked asset flow, editor/runtime
parity, and explicit subsystem adapters for input, UI, networking, physics,
audio, rendering, and scripting.

Agent entry point: see [`AGENTS.md`](../AGENTS.md) for the short repo map and
[`docs/VALIDATION.md`](VALIDATION.md) for canonical local checks.

## Purpose

This file is the architecture context router. Use this page first, then load only
the focused document needed for the task.

## Core Thesis

`SnAPI.GameFramework` owns the gameplay/session model above lower-level SnAPI
modules. `GameRuntime` drives startup, frame update, and shutdown. `World` owns
nodes, components, dense runtime storage, and optional subsystem adapters.
Renderer, UI, input, networking, physics, audio, asset pipeline, and scripting
modules remain separate dependencies with explicit integration boundaries.

The framework should make editor, runtime, Play-In-Editor, asset cooking,
project build, and packaged game flows share one coherent model without turning
GameFramework into the owner of lower-level module internals.

## Context Routing

- Read [`architecture/VISION_AND_ROADMAP.md`](architecture/VISION_AND_ROADMAP.md)
  for product direction, preserved feature inventory, migration strategy,
  roadmap, risks, and recommendations.
- Read [`CODING_STYLE.md`](CODING_STYLE.md) before source layout, naming, file
  granularity, public header, helper-type, or module-boundary changes.
- Read [`CODE_DOCUMENTATION.md`](CODE_DOCUMENTATION.md) before adding or changing
  public API docs, tutorials, generated docs, or docs layout.
- Read [`architecture/RUNTIME_CORE.md`](architecture/RUNTIME_CORE.md) for
  `GameRuntime`, `World`, node/component storage, handles, lifecycle, threading,
  and subsystem ownership.
- Read [`architecture/ASSET_PIPELINE.md`](architecture/ASSET_PIPELINE.md) for
  authored assets, source assets, cooked payloads, AssetPipeline integration, and
  render asset import/cook flow.
- Read [`architecture/EDITOR_AND_UI.md`](architecture/EDITOR_AND_UI.md) for
  editor service ownership, tool-world/game-world split, UI contexts, and editor
  viewport behavior.
- Read [`architecture/RENDERING.md`](architecture/RENDERING.md) for
  Renderer.New integration, render object flow, viewport ownership, and
  GameFramework-owned render abstractions.
- Read [`architecture/CONDUIT_AND_SCRIPTING.md`](architecture/CONDUIT_AND_SCRIPTING.md)
  for Conduit graph runtime, authored graph editor, scripting, reflection, and
  codegen boundaries.
- Read [`architecture/PROJECTS_BUILD_AND_PACKAGING.md`](architecture/PROJECTS_BUILD_AND_PACKAGING.md)
  for project/plugin/module creation, build profiles, packaging, and CLI/editor
  build flow.
- Read [`architecture/BUILD_FLAGS_AND_DEPENDENCIES.md`](architecture/BUILD_FLAGS_AND_DEPENDENCIES.md)
  for feature gates, dependency policy, shipping behavior, and stripability.
- Read [`architecture/TESTING_VALIDATION_AND_DIAGNOSTICS.md`](architecture/TESTING_VALIDATION_AND_DIAGNOSTICS.md)
  for validation, diagnostics, and regression expectations.
- Read [`architecture/API_SKETCHES.md`](architecture/API_SKETCHES.md) for public
  API sketches and example contracts.

## Non-Negotiable Invariants

- Keep GameFramework above lower-level SnAPI modules. GameFramework can depend
  on renderer, UI, input, networking, physics, audio, profiler, and asset
  pipeline APIs; those modules must not depend on GameFramework.
- Keep compiled framework source under `Modules/` after the source-layout
  migration. Every compiled module owns its own `Public/`, `Private/`,
  `Dependencies.cmake`, and `CMakeLists.txt` contract.
- Until source is migrated, treat `include/` and `src/` as legacy current-state
  roots, not target architecture.
- Keep public gameplay/editor/build contracts in module `Public/` roots and
  implementation details in module `Private/` roots.
- `World` owns nodes, components, dense runtime storage, and subsystem adapter
  instances for the active session.
- Handles are durable identity. Borrowed pointers are temporary views and must
  not become ownership.
- Node/component constructors and destructors stay side-effect light; runtime
  setup and teardown belong in lifecycle callbacks and world-owned services.
- Editor bootstrap must support deferred node/component `OnCreate` where the
  viewport or subsystem path is layout-driven.
- Asset source documents and cooked runtime payloads are different contracts.
- Generated docs, generated reflection output, cooked assets, and build output
  are not source-of-truth architecture.

## Historical Context

Older notes live under `Docs/GameFramework/`. Preserve them as domain history
unless a focused doc in this `docs/` tree supersedes them. When older notes
conflict with this router or focused architecture docs, update the focused docs
or create an execution plan rather than letting contradictory guidance persist.
