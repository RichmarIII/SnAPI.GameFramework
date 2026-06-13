# Editor UI Text Ordering Plan

Status: completed
Branch: codex/module-cmake-layout

## Problem

Editor UI text is routed through Renderer.New's separate text overlay path while
panels, buttons, images, and other UI geometry are routed through the UI overlay
packet path. Renderer.New records `UiOverlayPass` before `TextOverlayPass`, so
UI text is always drawn after every UI quad instead of preserving the order
emitted by SnAPI.UI. This lets earlier scroll/body text appear over later modal
footers and controls.

## Scope

- Keep SnAPI.UI's packet order as the source of truth for editor UI rendering.
- Convert UI `TextInstance` packets to ordered Renderer.New UI glyph draw
  packets in the GameFramework renderer bridge.
- Use Renderer.New `TextSystem` for glyph shaping and atlas texture ownership
  rather than adding a parallel text renderer.
- Decode ordered UI glyph packets with a Renderer.New glyph UI material instead
  of the image material, so MSDF/MTSDF atlas channels become text coverage
  rather than visible RGB color.
- Leave non-UI text submissions on Renderer.New's existing text overlay path.

## Validation

- Build `SnAPI.GameFramework.Editor` in release configuration.
- Run the editor and verify project modal text no longer appears above later
  footer/buttons or foreground controls.
- Verify ordered UI text renders as normal tinted text, without exposing atlas
  color channels.

## Progress

- Converted GameFramework UI text bridge output from Renderer.New text overlay
  submissions to ordered Renderer.New UI glyph packets.
- Added a Renderer.New UI glyph fragment material so ordered UI glyph packets
  decode MTSDF atlas coverage instead of rendering atlas RGB channels as image
  color.
- 2026-06-13: `cmake --build build/release --target
  SnAPI.GameFramework.Editor -j18` succeeded.
- 2026-06-13: Brief release editor launch reached Wayland renderer host
  creation and queued ordered UI packets with `text=0`; no editor process was
  left running after timeout.
- 2026-06-13: User confirmed the text ordering/layering fix worked, and the
  follow-up glyph material correction fixed the colored atlas-channel text.

## Closeout

- Completed on branch `codex/module-cmake-layout`.
- Ordered UI text is converted into Renderer.New UI glyph draw packets so
  SnAPI.UI packet order is preserved for editor UI.
