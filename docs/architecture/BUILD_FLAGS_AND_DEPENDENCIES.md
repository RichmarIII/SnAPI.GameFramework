# Build Flags And Dependencies

Read this when:

- changing CMake options, dependency discovery, feature gates, optional
  integrations, shipping behavior, or stripability

Related context:

- `../ARCHITECTURE.md`
- `../WORKFLOW.md`
- `PROJECTS_BUILD_AND_PACKAGING.md`

## Current CMake Options

Important GameFramework options include:

- `SNAPI_ENABLE_IPO`
- `SNAPI_GF_BUILD_TESTS`
- `SNAPI_GF_BUILD_EXAMPLES`
- `SNAPI_GF_BUILD_DOCS`
- `SNAPI_GF_BUILD_EDITOR`
- `SNAPI_GF_BUILD_RUNTIME`
- `SNAPI_GF_BUILD_BUILDCLI`
- `SNAPI_GF_BUILD_REFLECTION_GEN`
- `SNAPI_GF_ENABLE_LUA`
- `SNAPI_GF_ENABLE_SWIG`
- `SNAPI_GF_ENABLE_PROFILER`
- `SNAPI_GF_INCLUDE_RENDERER_NEW_WORKSPACE`
- `SNAPI_PROJECT_ROOT_DIR`

Important dependency override paths include:

- `SNAPI_GF_MATH_SOURCE_DIR`
- `SNAPI_GF_PROFILER_SOURCE_DIR`
- `SNAPI_GF_ASSETPipeline_SOURCE_DIR`
- `SNAPI_GF_AUDIO_SOURCE_DIR`
- `SNAPI_GF_NETWORKING_SOURCE_DIR`
- `SNAPI_GF_PHYSICS_SOURCE_DIR`
- `SNAPI_GF_INPUT_SOURCE_DIR`
- `SNAPI_GF_UI_SOURCE_DIR`
- `SNAPI_GF_RENDERER_NEW_SOURCE_DIR`

## Dependency Direction

GameFramework may depend on lower-level SnAPI modules. Lower-level SnAPI modules
must not depend on GameFramework.

GameFramework currently integrates:

- SnAPI.Math
- SnAPI.AssetPipeline
- SnAPI.Audio
- SnAPI.Networking
- SnAPI.Physics
- SnAPI.Input
- SnAPI.UI
- SnAPI.Renderer.New
- SnAPI.Profiler
- third-party libraries such as stduuid, nlohmann_json, Lua, SWIG, Catch2,
  Doxygen, and MkDocs dependencies

## Dependency Resolution Policy

Dependency resolution order:

1. explicit user-provided source directory, installed SDK root, or package root
   passed through a documented cache variable;
2. system/package-manager or SDK installation discovered by `find_package`,
   `pkg-config`, or equivalent config-mode packages;
3. an existing project-approved CMake `FetchContent` fallback.

Do not add new fallback downloaders, vendored source directories, or submodules
without explicit user approval and an active plan.

## Module Build Target

After source-layout migration, each compiled GameFramework module should own its
target and dependency application in module-local `CMakeLists.txt` and
`Dependencies.cmake`. The root CMake file should keep global options, shared
coordination, and top-level additions only.
