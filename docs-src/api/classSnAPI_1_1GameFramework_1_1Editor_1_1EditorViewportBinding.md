# SnAPI::GameFramework::Editor::EditorViewportBinding

Keeps the root editor UI context bound to the renderer surface viewport.

`EditorViewportBinding` is the small stateful adapter that keeps the editor shell's root UI context synchronized with the runtime window and bound to the renderer's default surface viewport.

Core semantics:
- Initialization enables the default renderer viewport (`1`) and binds it to the UI root context.
- Embedded game/editor viewports remain explicit offscreen render viewports owned by `UIRenderViewport`.
- `SyncToWindow()` recreates the root binding when needed and keeps the logical UI size synchronized with the current renderer window size.

Ownership and lifetime:
- The class stores only ids and cached UI size state.
- The default renderer viewport and UI binding are owned by the runtime subsystems.
- Cached ids become invalid after `Shutdown()` or runtime teardown.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorViewportBinding::m_viewportId`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorViewportBinding::m_rootContextId`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::EditorViewportBinding::m_lastWidth`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::EditorViewportBinding::m_lastHeight`
</div>
<div class="snapi-api-card" markdown="1">
## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorViewportBinding::Initialize(GameRuntime &Runtime, std::string ViewportName)`

Bind the root editor UI context to the renderer surface viewport.

**Parameters**

- `Runtime`: Initialized runtime that owns renderer and UI systems.
- `ViewportName`: Reserved diagnostic name for callers that still pass one.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorViewportBinding::Shutdown(GameRuntime *Runtime)`

Clear cached binding state.

**Parameters**

- `Runtime`: Reserved runtime pointer for service lifecycle symmetry.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorViewportBinding::SyncToWindow(GameRuntime &Runtime)`

Synchronize the bound viewport with the current window and UI state.

**Parameters**

- `Runtime`: Initialized runtime.

**Returns:** `true` when the binding remains valid and the requested sync work succeeded, otherwise `false`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorViewportBinding::IsInitialized() const`

Query whether both the viewport id and root UI context id are known.

**Returns:** `true` when initialization has established a live binding.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorViewportBinding::ViewportId() const`

Access the bound renderer viewport id.

**Returns:** Viewport id, or `0` when uninitialized.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorViewportBinding::ContextId() const`

Access the bound root UI context id.

**Returns:** Context id, or `0` when uninitialized.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorViewportBinding::ResolveViewportSize(GameRuntime &Runtime, float &OutWidth, float &OutHeight) const`

**Parameters**

- `Runtime`: 
- `OutWidth`: 
- `OutHeight`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorViewportBinding::EnsureUiBinding(GameRuntime &Runtime) const`

**Parameters**

- `Runtime`:
</div>
