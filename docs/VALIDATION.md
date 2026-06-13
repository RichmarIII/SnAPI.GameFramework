# Validation

Use the smallest validation that proves the change, then broaden when touching
shared behavior.

## Docs-Only Changes

For Markdown-only or agent-framework docs changes:

```bash
git diff --check
```

For generated documentation changes, configure docs and build the docs target:

```bash
cmake -S . -B build/debug -DSNAPI_GF_BUILD_DOCS=ON
cmake --build build/debug --target SnAPI.GameFramework.Docs -j18
```

The docs target uses Doxygen, `tools/GenerateMkDocsApi.py`, and MkDocs. It writes
generated site output under `docs/site/`.

## Configure

Default development configure:

```bash
cmake -S . -B build/debug \
  -DSNAPI_GF_BUILD_TESTS=ON \
  -DSNAPI_GF_BUILD_EXAMPLES=ON \
  -DSNAPI_GF_BUILD_DOCS=ON
```

Renderer.New workspace override when needed:

```bash
cmake -S . -B build/debug \
  -DSNAPI_GF_RENDERER_NEW_SOURCE_DIR=/mnt/Dev/CodeProjects/SnAPI.Renderer.New
```

## Build

Core library:

```bash
cmake --build build/debug --target SnAPI.GameFramework -j18
```

Runtime/editor/build tools:

```bash
cmake --build build/debug --target SnAPI.GameFramework.Runtime -j18
cmake --build build/debug --target SnAPI.GameFramework.Editor -j18
cmake --build build/debug --target SnAPI.GameFramework.Build -j18
```

Examples:

```bash
cmake --build build/debug --target FeatureShowcase -j18
cmake --build build/debug --target MultiplayerExample -j18
```

## Tests

```bash
ctest --test-dir build/debug --output-on-failure
```

## Broaden Validation When

- Public headers or runtime contracts under `Modules/<Module>/Public/` change.
- `GameRuntime`, `World`, node/component storage, handles, lifecycle, or
  threading behavior changes.
- Asset source/cooked payload contracts or AssetPipeline integration changes.
- Renderer.New, UI, input, networking, physics, audio, profiler, or scripting
  integration changes.
- Editor bootstrap, viewport, selection, transform, or Play-In-Editor behavior
  changes.
- Project creation, plugin creation, module creation, build, cook, or packaging
  behavior changes.
- A change claims to advance `docs/ARCHITECTURE.md`.

For these cases, build the library, build relevant tools/examples, run tests,
and build docs when public contracts or documentation changed. If validation
cannot run, report the exact command and reason.
