# SnAPI::GameFramework::Editor::EditorRootViewportService

Service that owns and resizes the root editor viewport binding.

This is the service wrapper around `EditorViewportBinding`. It keeps the explicit root viewport alive and synchronized with the current runtime window for the duration of the editor session.

## Private Members

<div class="snapi-api-card" markdown="1">
### `EditorViewportBinding SnAPI::GameFramework::Editor::EditorRootViewportService::m_binding`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorRootViewportService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorRootViewportService::Initialize(EditorServiceContext &Context) override`

Create the root editor viewport binding.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorRootViewportService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Propagate window-size and UI-binding changes into the explicit root viewport.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorRootViewportService::Shutdown(EditorServiceContext &Context) override`

Destroy the explicit root viewport binding.

**Parameters**

- `Context`:
</div>
