# SnAPI::GameFramework::Editor::EditorViewportBinding

Owns the root editor render viewport and binds it to the root UI context.

`EditorViewportBinding` is the small stateful adapter that keeps the editor shell's root render viewport synchronized with the runtime window and the root UI context.

Core semantics:
- Initialization creates an explicit renderer viewport instead of using the renderer's implicit default viewport.
- The created viewport is bound to the UI root context and assigned the `UiPresentOnly` pass-graph preset.
- `SyncToWindow()` preserves explicit-viewport mode, recreates missing bindings when possible, and keeps logical UI size and render extent in sync with the current window.
- Render-extent resize is intentionally deferred while the left mouse button is held to avoid resizing the render target during active drag operations.

Ownership and lifetime:
- The class stores only ids and cached size state.
- The underlying viewport and UI binding are owned by the runtime subsystems, not by this object.
- Cached ids become invalid after `Shutdown()` or runtime teardown.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorViewportBinding::m_viewportName`
</div>
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
### `std::uint32_t SnAPI::GameFramework::Editor::EditorViewportBinding::m_appliedRenderWidth`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorViewportBinding::m_appliedRenderHeight`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorViewportBinding::m_pendingRenderWidth`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorViewportBinding::m_pendingRenderHeight`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorViewportBinding::m_hasPendingRenderExtentResize`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorViewportBinding::Initialize(GameRuntime &Runtime, std::string ViewportName)`

Create and bind the root editor viewport.

**Parameters**

- `Runtime`: Initialized runtime that owns renderer and UI systems.
- `ViewportName`: Optional logical viewport name. Empty input keeps the current/default name.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorViewportBinding::Shutdown(GameRuntime *Runtime)`

Tear down the current viewport binding if it exists.

**Parameters**

- `Runtime`: Optional runtime used to unbind and destroy the live viewport. May be null during late teardown.
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
