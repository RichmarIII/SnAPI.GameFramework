# Renderer.New FPS Regression Plan

Status: completed
Branch: codex/renderer-new-fps-regression
Integration base: master

## Goal

Find and fix the Renderer.New editor/runtime FPS regression where the
GameFramework editor currently runs around 18 FPS when the expected rate is in
the hundreds of FPS for the same simple scene.

## Scope

- Measure release editor/runtime frame time using existing diagnostics before
  changing behavior.
- Identify whether the cost is in GameFramework frame orchestration, UI packet
  conversion, input/UI integration, Renderer.New frame submission, renderer
  pass/profile setup, synchronization, or frame pacing.
- Patch only the confirmed owning module. If the confirmed issue is in
  Renderer.New, SnAPI.UI, or SnAPI.Input, record that lower-level module change
  explicitly and validate it from GameFramework.
- Preserve the architecture boundary: GameFramework may configure and submit
  Renderer.New work, but Renderer.New owns renderer-native pass/profile/device
  behavior.

## Non-Goals

- No broad renderer redesign unless measurements prove the current frame model
  is the bottleneck.
- No speculative removal of rendering features without measured attribution.
- No dependency changes.

## Likely Files

- `Modules/GameFramework/Private/RendererSystemRendererNew.cpp`
- `Modules/GameFramework/Private/GameRuntime.cpp`
- `Modules/GameFramework/Private/UISystem.cpp`
- `Modules/Editor/Private/Editor/GameEditor.cpp`
- Lower-level sibling repos only if measurements prove the bottleneck is there.

## Validation

```bash
git diff --check
cmake --build build/release --target SnAPI.GameFramework.Editor -j18
cmake --build build/release --target SnAPI.GameFramework.Runtime -j18
```

Runtime validation will include the focused release editor/runtime diagnostic
commands used during investigation and their results.

## Findings

- Release editor timing with
  `SNAPI_RENDERER_NEW_LOG_FRAMEGRAPH_CPU_TIMINGS=1` and
  `SNAPI_RENDERER_NEW_LOG_DEFAULT_PROFILE_CPU_TIMINGS=1` showed the first frame
  doing shader/resource warmup work, then steady-state Renderer.New profile
  timings commonly around `0.6ms` to `1.3ms` and occasional frames around
  `5ms` to `6ms`.
- The steady-state renderer pass/profile timings are not consistent with an
  18 FPS renderer workload bottleneck.
- The confirmed GameFramework issue was frame pacing configuration: the
  Renderer.New surface was hardcoded with `VSync = true`, so the runtime's
  `MaxFpsWhenVSyncOff` pacing path could not reflect editor intent. The editor
  also explicitly capped VSync-off pacing to 120 FPS.
- The fix exposes VSync through `RendererBootstrapSettings`, passes that value
  into `RenderSurfaceCreateInfo`, and configures the editor for VSync off with
  no software max-FPS cap.
- The editor FPS HUD also had its graph legend disabled. `UIRealtimeGraph`
  renders the latest series values through that legend, so the overlay now
  enables it and keeps the graph on intrinsic `Auto` sizing so the
  DPI-scaled graph measurement remains owned by the UI control.
- Window-close shutdown exposed a Renderer.New ownership gap: GameFramework
  created Renderer.New render targets/textures and the main presentation
  surface, but shutdown relied on whole-runtime teardown. Renderer shutdown now
  explicitly destroys viewport outputs and the presentation surface before
  resetting the Renderer.New runtime/window state.
- The editor close freeze was a runtime loop bug: `RendererSystem::EndFrame()`
  marks the renderer uninitialized when the window closes, but
  `GameRuntime::ShouldContinueRunning()` treated an uninitialized renderer as
  keep-running. A configured renderer becoming uninitialized now terminates the
  auto-exit loop.
- Nsight showed the remaining editor GPU cost came from two full frame
  submissions: the game viewport render and the editor main surface render.
  The main editor surface was still using `DeferredProfile::Id`, so it ran the
  same deferred world pass set as the game viewport. The editor now enables a
  GameFramework-owned UI-only Renderer.New profile for the main surface, with
  only `UiOverlayPass` and `TextOverlayPass` left enabled after each frame
  pipeline configuration.
- UI drop shadows were confirmed as a lower-level Renderer.New UI pass issue,
  not a missing GameFramework/UI packet bridge. SnAPI.UI emitted shadow color,
  blur, spread, and expansion data, and GameFramework forwarded it, but
  Renderer.New's `UiOverlayPass` rendered each shadow as an opaque solid rect
  and ignored blur/falloff. The GameFramework bridge also stopped folding
  packet expansion back into renderer spread because the UI shadow rect is
  already expanded by blur/spread before submission. Renderer.New now builds a
  vertex-alpha soft shadow mesh for those packets.

## Validation Results

- `git diff --check`: passed.
- `cmake --build build/release --target SnAPI.GameFramework.Editor -j18`:
  passed.
- `cmake --build build/release --target SnAPI.GameFramework.Runtime -j18`:
  passed.
- After adding the UI-only editor main-surface profile,
  `git diff --check`,
  `cmake --build build/release --target SnAPI.GameFramework.Editor -j18`,
  and `cmake --build build/release --target SnAPI.GameFramework.Runtime -j18`
  passed.
- After the UI shadow fix, `git diff --check`,
  `git -C /mnt/Dev/CodeProjects/SnAPI.Renderer.New diff --check`, and
  `cmake --build build/release --target SnAPI.GameFramework.Editor -j18`
  passed.
- `timeout 4s ./build/release/Modules/Editor/SnAPI.GameFramework.Editor`:
  startup smoke passed; command ended with timeout code `124` as expected.
- Existing build warnings remain from generated reflection/SWIG, Jolt ODR
  warnings, and Lua `stringop-overflow`; no new build failure was introduced.

## Completion Criteria

- Bottleneck is identified with concrete timing/log evidence.
- FPS regression is fixed or the remaining blocker is documented with exact
  lower-level ownership.
- Relevant release targets build.
- Plan is updated with validation results before closeout.
