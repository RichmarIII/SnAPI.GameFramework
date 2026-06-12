# Rendering Integration

Read this when:

- changing Renderer.New integration, renderer subsystem lifecycle, viewports,
  render objects, lights, cameras, render assets, editor overlays, UI packets, or
  text rendering

Related context:

- `../ARCHITECTURE.md`
- `ASSET_PIPELINE.md`
- `EDITOR_AND_UI.md`

## Dependency Direction

GameFramework depends on Renderer.New. Renderer.New must not depend on
GameFramework and must not contain GameFramework-shaped workaround code.

GameFramework owns the gameplay/editor rendering API:

- render components
- render object registration and lifetime
- viewport binding
- camera/light extraction
- render asset source/cooked mapping
- editor overlay policy
- UI packet conversion into Renderer.New inputs

Renderer.New owns renderer-native runtime contracts, passes, profiles,
glyph/text systems, resources, devices, windows, surfaces, and backend behavior.

## Native Renderer.New Flow

GameFramework rendering should be centered around Renderer.New concepts rather
than old renderer limitations. The target direction is:

- GameFramework render objects map to Renderer.New scene submissions and retained
  renderer objects using Renderer.New-owned handles/contracts.
- GameFramework windows and viewports map to Renderer.New platform/window,
  surface, render-view, and output abstractions.
- Editor and game viewports should support multiple real windows and render
  targets naturally through Renderer.New.
- UI packets should be translated into Renderer.New UI/text submission formats.
- Text should use Renderer.New's glyph/text system instead of a temporary
  GameFramework-side text path.
- Pass and profile configuration should use Renderer.New-owned pass settings
  rather than duplicating settings in GameFramework.

## Current Migration State

The codebase is actively moving from old renderer assumptions to Renderer.New.
Temporary migration adapters should be treated as debt unless an active plan
promotes them into native GameFramework API.

## Target Module Layout

GameFramework-owned rendering abstractions should move to a focused rendering
module or `GameFramework` module subfolder with public contracts under `Public/`
and implementation under `Private/`. Renderer.New include/link details should be
owned by module-local CMake and dependency files.

## Validation Expectations

Rendering changes require build validation and, when editor/viewports/UI are
affected, screenshot or visual checks of the editor. Text and UI changes should
verify actual rendered output, not only compile.
