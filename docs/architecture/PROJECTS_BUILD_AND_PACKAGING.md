# Projects, Build, And Packaging

Read this when:

- changing project/plugin/module creation, project descriptors, build profiles,
  build graph planning, package/cook/stage flow, CLI build, or editor build UI

Related context:

- `../ARCHITECTURE.md`
- `ASSET_PIPELINE.md`
- `BUILD_FLAGS_AND_DEPENDENCIES.md`

## Product Surface

GameFramework owns project, plugin, and module creation services plus build and
package orchestration for GameFramework-based games.

Primary surfaces:

- project creation
- plugin creation
- module creation and management
- build profiles
- package/cook/stage flow
- CLI build automation
- editor-integrated build/package UI

## Module Creation Target

Newly generated project/plugin modules should follow the SnAPI module layout:

```text
Modules/<ModuleName>/
  Public/
  Private/
  Dependencies.cmake
  CMakeLists.txt
```

If project templates still emit older `Code/<Module>/include` and
`Code/<Module>/src` shapes, record that as compatibility debt and migrate
templates through an active plan.

## Descriptor And Build Boundaries

Project and plugin descriptors are authored source. Generated build fragments
belong in intermediate/generated output paths and must be safe to regenerate.

CMake remains the native build toolchain integration point. The editor and CLI
should gather intent and invoke explicit build services rather than inventing a
parallel build system.

## Validation Expectations

Creation/build/package changes should validate:

- descriptor serialization
- generated file layout
- CMake configure/build behavior
- asset cooking and staging where relevant
- CLI/editor parity for shared build services
