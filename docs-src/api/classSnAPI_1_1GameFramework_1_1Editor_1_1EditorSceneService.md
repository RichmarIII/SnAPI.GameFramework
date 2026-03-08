# SnAPI::GameFramework::Editor::EditorSceneService

Service that owns bootstrap scene creation and active camera tracking.

The service wraps `EditorSceneBootstrap` so other editor services can depend on a stable API for:
- ensuring an editor camera exists
- querying the active camera component
- keeping the renderer's active camera synchronized with the world

## Private Members

<div class="snapi-api-card" markdown="1">
### `EditorSceneBootstrap SnAPI::GameFramework::Editor::EditorSceneService::m_scene`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorSceneService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorSceneService::Initialize(EditorServiceContext &Context) override`

Build or refresh the editor bootstrap scene.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSceneService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Refresh active-camera tracking each frame.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSceneService::Shutdown(EditorServiceContext &Context) override`

Destroy tracked bootstrap nodes.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorSceneService::EnsureEditorCamera(EditorServiceContext &Context)`

Ensure an editor camera exists in the current runtime world.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `CameraComponent * SnAPI::GameFramework::Editor::EditorSceneService::ActiveCameraComponent() const`

Access the currently tracked active camera component.

**Returns:** Non-owning pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Graphics::ICamera * SnAPI::GameFramework::Editor::EditorSceneService::ActiveRenderCamera() const`

Access the currently tracked render camera interface.

**Returns:** Non-owning pointer or `nullptr`.
</div>
