# Editor And UI

Read this when:

- changing editor services, editor layout, selection/transform interaction,
  tool-world/game-world behavior, UI contexts, editor viewport binding, or
  Play-In-Editor flow

Related context:

- `../ARCHITECTURE.md`
- `RUNTIME_CORE.md`
- `RENDERING.md`

## Core Model

The editor is a GameFramework host built on the same world model as runtime, but
with editor-specific services, UI layout, tool-world/game-world separation, and
deferred bootstrap behavior.

Editor services should own editor policy. Nodes/components should not need to
know about editor bootstrap ordering unless they expose explicit editor-facing
contracts.

## UI Ownership

`UISystem` owns root and child UI contexts. `UIRenderViewport` is layout-driven
and may lazily create a render viewport as layout/paint establishes the target
region.

GameRuntime and World can bind UI contexts to render viewports through explicit
GameFramework APIs. Avoid implicit renderer/UI coupling.

## Editor Bootstrap

Editor startup must support deferred node/component `OnCreate` callbacks while
the UI viewport and render path are being constructed. This prevents
render-facing nodes from running backend work before the viewport exists.

Do not force eager viewport creation from constructors. Route through editor
services, layout, and viewport binding.

## Interaction Services

Selection, transform, asset, project, build, and viewport services should remain
focused and testable. Prefer service-owned state over global editor state.

## Target Module Layout

Editor public contracts should move under an editor module `Public/` root.
Editor service implementations, UI helpers, asset browser logic, and
SDL/platform integration details should move under that module's `Private/`
tree.
