# SnAPI::GameFramework::Editor::EditorGameViewportOverlayService

Renders game-viewport overlays inside the viewport-owned UI context.

The service creates lightweight HUD elements in the `UIRenderViewport` overlay context rather than in the root editor shell. That keeps overlay rendering spatially scoped to the viewport and allows it to survive layout sync without coupling the overlay widgets to the main shell tree.

Current behavior:
- The HUD graph is active and samples frame-time / FPS data.
- Profiler-panel state exists but is currently kept collapsed unless the active tab changes to the dedicated profiler view.
- Overlay elements are rebuilt when the viewport-owned context id changes.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_overlayContextId`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_hudPanel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_hudGraph`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_hudFrameLabel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_hudFpsLabel`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_hudFrameSeries`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_hudFpsSeries`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_profilerPanel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_profilerGraph`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_profilerFrameLabel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementId SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_profilerFpsLabel`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_profilerFrameSeries`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::m_profilerFpsSeries`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::type_index > SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::Dependencies() const override`

Depends on `EditorLayoutService` because the viewport UI element is sourced from the layout.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::Initialize(EditorServiceContext &Context) override`

Reset overlay bookkeeping for a fresh session.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Ensure overlay widgets exist in the current viewport context and refresh sampled data.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::Shutdown(EditorServiceContext &Context) override`

Destroy overlay widget handles and forget the bound overlay context.

**Parameters**

- `Context`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::ResetOverlayState()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::EnsureOverlayElements(SnAPI::UI::UIContext &OverlayContext)`

**Parameters**

- `OverlayContext`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::UpdateOverlayVisibility(SnAPI::UI::UIContext &OverlayContext, int32_t ActiveTabIndex)`

**Parameters**

- `OverlayContext`: 
- `ActiveTabIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorGameViewportOverlayService::UpdateOverlaySamples(SnAPI::UI::UIContext &OverlayContext, float DeltaSeconds)`

**Parameters**

- `OverlayContext`: 
- `DeltaSeconds`:
</div>
