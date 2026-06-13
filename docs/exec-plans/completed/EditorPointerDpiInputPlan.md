# Editor Pointer DPI Input Plan

Status: completed
Branch: codex/module-cmake-layout

## Problem

The editor can highlight or click UI controls offset from the visible mouse
cursor when UI DPI scaling is active. The UI module owns DPI-scaled layout, but
input coordinates must be normalized into the same UI screen coordinate space
as hit testing. A mismatch between native window coordinates, renderer output
extent, and UI viewport extent causes the selection target to drift farther from
the visible cursor as the pointer moves down and right.

## Scope

- Keep UI-owned input-coordinate normalization for pointer and wheel positions.
- Ensure DPI scale affects UI layout and rendering metrics without duplicating
  editor-specific pointer math.
- Convert runtime native window coordinates into UI screen coordinates once,
  using the live viewport/output ratio and root UI DPI scale.
- Use that conversion at GameFramework UI routing/query boundaries instead of
  duplicating DPI math in editor callers.
- Add focused coverage/logging that proves pointer input and UI hit testing use
  the same coordinate space.

## Validation

- Build the affected UI/GameFramework/editor targets.
- Run focused UI/GameFramework tests that cover pointer routing.

## Validation Notes

- `cmake --build /mnt/Dev/CodeProjects/SnAPI.UI/build --target SnAPI.UITests -j18`
  succeeded.
- `/mnt/Dev/CodeProjects/SnAPI.UI/build/tests/SnAPI.UITests "UITabs does not apply DPI twice to pointer input"`
  passed.
- `cmake --build build/release --target SnAPI.GameFramework.Editor -j18`
  succeeded and produced `build/release/Modules/Editor/SnAPI.GameFramework.Editor`.

After user retests continued showing the same approximately 2x pointer offset,
the SDL-only theory was rejected. A diagnostic release-editor run with
`SNAPI_INPUT_SDL3_LOG_EVENTS=1`, `SNAPI_GF_LOG_INPUT_EVENTS=1`,
`SNAPI_GF_RENDERER_NEW_LOG_UI=1`, and `SNAPI_RENDERER_NEW_LOG_UI_OVERLAY=1`
reported SDL host/window/pixel size all as `1920x1080` with display scale and
pixel density both `1`, so the active path does not expose a useful SDL
window-to-pixel ratio to apply.

The definitive live-path issue was in GameFramework's input bridge:
`UiViewportTransform` stored output and UI sizes but returned only
`WindowX - OutputX` / `WindowY - OutputY`, so width/height factoring was
diagnostic-only. `UISystem::PushInput` also chose dispatch contexts using the
root-mapped point but then pushed the original event into each `UIContext`, so
root normalization could be bypassed before actual widget hit testing. The fix
now maps platform/window coordinates to UI screen coordinates in `GameRuntime`
using viewport ratio and root UI DPI scale, then dispatches that normalized
pointer/wheel event through `UISystem`.

- `cmake --build build/release --target SnAPI.GameFramework.Editor -j18`
  succeeded again after the runtime bridge correction and copied editor assets.

Known warning noise remains in existing generated/framework/editor and third-party
code paths; no build failure was introduced by the pointer DPI changes.

Follow-up: the embedded game viewport's owned child UI context also needs to
keep logical viewport units separate from arranged DPI-scaled screen pixels.
Using the arranged `UIRenderViewport` rect directly as the child viewport size
causes the child context to apply DPI a second time, making the game-viewport
overlay cover roughly half the visible viewport at 2x UI scale.

- 2026-06-13: Updated `UIRenderViewport` to derive the owned child UI
  context's logical viewport size by dividing the arranged viewport rect by the
  child context DPI scale while preserving the arranged screen origin.
- 2026-06-13: `cmake --build build/release --target
  SnAPI.GameFramework.Editor -j18` succeeded after the overlay sizing fix.
- 2026-06-13: Brief release editor launch reached Wayland renderer host
  creation and queued UI packets; no editor process was left running after
  timeout.
- 2026-06-13: User retest showed the game viewport overlay was still half-size.
  Added render-target scaling when translating SnAPI.UI packets into
  Renderer.New UI overlay packets for non-root render viewports, so embedded
  viewport overlays are mapped from child context UI coordinates into the
  actual offscreen render target extent.
- 2026-06-13: `cmake --build build/release --target
  SnAPI.GameFramework.Editor -j18` succeeded after the Renderer.New UI packet
  scaling change.
- 2026-06-13: User retest showed no visible change. Reverted the child UI
  context DPI division in `UIRenderViewport`; SnAPI.UI should own DPI scaling,
  and the owned viewport context should receive the arranged viewport size
  directly. Added `SNAPI_GF_LOG_VIEWPORT_OVERLAY_SIZES=1` diagnostics to print
  the arranged viewport rect, child context screen/viewport size, child DPI,
  render target extent, and UI packet source bounds so the remaining half-size
  source can be proven numerically.
- 2026-06-13: Diagnostic release-editor run showed the embedded viewport
  arranged rect, owned UI context viewport, and offscreen render target all
  matched at about `1062x498`, with child UI DPI `1` and source UI packet
  bounds covering the full viewport. The remaining half-size source is not
  SnAPI.UI DPI; Renderer.New's default frame pipeline is configured for the
  main surface before the frame opens, while embedded render targets are drawn
  inside the same frame and cannot reconfigure the default pipeline mid-frame.
  Therefore the Renderer.New UI overlay pass projects embedded overlay pixels
  through the current frame-pipeline output extent. Updated the GameFramework
  Renderer.New bridge to convert non-root UI overlay packets into that active
  overlay projection extent instead of the offscreen texture extent.
- 2026-06-13: User clarified that Renderer.New already supports rendering
  different `FrameGraphProfileId` values independently; no new profile-instance
  class or type is needed. GameFramework should construct semantic profile IDs
  such as editor UI-only, game-view deferred, and pared-down preview profiles,
  then render those profile IDs independently with their own `RenderView`
  extents. Renderer.New passes should use the submitted `RenderView`/target
  extents for viewport-dependent projection instead of hardcoded create-time
  extents.
- 2026-06-13: `cmake --build build/release --target
  SnAPI.GameFramework.Editor -j18` succeeded after the final branch updates and
  copied editor assets.

## Closeout

- Completed on branch `codex/module-cmake-layout`.
- Pointer DPI/input routing and UI text ordering fixes are in this branch.
- The remaining game-viewport overlay architecture concern is recorded as
  follow-up Renderer.New/GameFramework profile-id work, not a blocker for this
  CMake layout branch closeout.
