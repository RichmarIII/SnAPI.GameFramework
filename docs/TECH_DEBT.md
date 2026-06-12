# Technical Debt

Small cleanup items that are known but not currently active plan work.

## Source Layout

- Move current public headers from `include/` into module `Public/` roots.
- Move current implementation sources from `src/` into module `Private/` roots.
- Add a root `Modules/` tree with module-local `CMakeLists.txt` and
  `Dependencies.cmake` files.
- Split the current root `CMakeLists.txt` so target ownership and dependency
  application live in module-local files.
- Keep generated site output under `docs/site/` and avoid using `docs/` itself
  as generated output.

## File Granularity

- Split large runtime, renderer-integration, asset-pipeline, editor, and build
  implementation files into one-primary-type-per-file units.
- Replace broad catch-all headers with focused public contracts.
- Remove parent-relative includes during the module-layout migration.

## Rendering Migration

- Finish native Renderer.New flow for render objects, viewports, editor overlays,
  UI packets, text, and asset pipeline output.
- Remove temporary Renderer.New migration adapters once the native GameFramework
  rendering API is settled.
- Keep Renderer.New independent of GameFramework. GameFramework owns any
  GameFramework-shaped rendering abstractions.

## Docs

- Decide whether historical `Docs/GameFramework/` files should be migrated into
  focused `docs/architecture/` files or kept as archival references.
- Keep `docs-src/api/` regeneration out of unrelated code patches when possible.
- Add a lightweight docs check target if a fast agent-docs-only target becomes
  useful.
