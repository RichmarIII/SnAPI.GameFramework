# Vision And Roadmap

Read this when:

- planning broad GameFramework direction, migration strategy, roadmap sequencing,
  or large refactors

Related context:

- `../ARCHITECTURE.md`
- `../TECH_DEBT.md`
- `PROJECTS_BUILD_AND_PACKAGING.md`

## Vision

`SnAPI.GameFramework` should be the coherent gameplay/editor framework above the
lower-level SnAPI modules. It should make world ownership, node/component
composition, reflection, serialization, source/cooked assets, editor workflows,
project packaging, and Renderer.New integration feel like one system without
collapsing lower-level module boundaries.

## Near-Term Roadmap

1. Finish Renderer.New-native GameFramework integration.
2. Stabilize editor viewport, UI overlay, text, selection, and transform flows.
3. Install and maintain the agent workflow docs in this `docs/` tree.
4. Plan the source-layout migration to `Modules/`.
5. Split root CMake target/dependency ownership into module-local files.
6. Convert source/cooked asset APIs and docs to the final hard-cut model.
7. Expand validation around editor boot, rendering, asset pipeline, and project
   creation.

## Source Layout Migration

The target is:

```text
Modules/<ModuleName>/
  Public/
  Private/
  Dependencies.cmake
  CMakeLists.txt
```

The current `include/` and `src/` layout is accepted only as existing debt. New
plans should move toward the target rather than formalizing the old roots.

## Risks

- Mixing Renderer.New design into GameFramework or GameFramework assumptions into
  Renderer.New.
- Treating generated docs/API output as hand-authored source.
- Combining broad source movement with behavior changes.
- Letting project/module creation templates generate obsolete layouts.
- Leaving old renderer assumptions in public GameFramework APIs.
