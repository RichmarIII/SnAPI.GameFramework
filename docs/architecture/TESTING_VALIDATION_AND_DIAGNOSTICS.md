# Testing, Validation, And Diagnostics

Read this when:

- changing tests, validation strategy, diagnostics, profiling, editor/runtime
  verification, screenshot checks, or generated docs validation

Related context:

- `../ARCHITECTURE.md`
- `../VALIDATION.md`
- `RUNTIME_CORE.md`

## Test Layers

GameFramework validation should cover:

- pure unit tests for handles, reflection, serialization, storage helpers, and
  value codecs
- runtime tests for world/node/component lifecycle and frame phases
- asset pipeline tests for source/cooked payload contracts and serializers
- editor service tests for asset, selection, viewport, and layout behavior
- rendering integration tests or visual checks for renderer, viewport, UI, and
  text behavior
- project/build/package tests for creation services and descriptors

## Diagnostics

Diagnostics should identify the owning layer. Errors should distinguish:

- user-authored project/content errors
- GameFramework lifecycle/API errors
- AssetPipeline import/cook errors
- Renderer.New integration errors
- lower-level module dependency errors
- generated output/toolchain errors

## Visual Validation

Editor, viewport, UI, text, and rendering changes should use screenshots or
equivalent visual validation when practical. A build that passes but renders the
editor incorrectly is not enough for UI/rendering work.

## Generated Docs Validation

Generated docs use Doxygen, `tools/GenerateMkDocsApi.py`, and MkDocs. The final
site output lives under `docs/site/`. The generated API markdown under
`docs-src/api/` should be regenerated intentionally and not mixed into unrelated
patches when avoidable.
