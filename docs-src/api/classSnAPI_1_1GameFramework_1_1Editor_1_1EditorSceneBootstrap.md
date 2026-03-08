# SnAPI::GameFramework::Editor::EditorSceneBootstrap

Creates and tracks the default editor scene bootstrap.

`EditorSceneBootstrap` is a convenience helper that ensures an editor session starts with something useful to render and manipulate. In renderer-enabled builds it is responsible for creating or refreshing:
- an editor level
- an editor camera node and camera component
- baseline scene content such as lighting and sample primitives

Core semantics:
- `Initialize()` destroys stale bootstrap-owned nodes and recreates the tracked scene.
- `EnsureEditorCamera()` only guarantees a camera for editor or PIE worlds.
- `SyncActiveCamera()` keeps the runtime renderer pointed at a live camera component when possible.
- In non-renderer builds the methods degrade to inert or no-op behavior.

Ownership and lifetime:
- The class stores only non-owning handles and pointers into the runtime world.
- Tracked handles become invalid when the world is cleared, the nodes are destroyed, or `Shutdown()` is called.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorSceneBootstrap::m_levelNode`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorSceneBootstrap::m_cameraNode`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::Editor::EditorSceneBootstrap::m_sceneNodes`
</div>
<div class="snapi-api-card" markdown="1">
### `CameraComponent* SnAPI::GameFramework::Editor::EditorSceneBootstrap::m_cameraComponent`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorSceneBootstrap::Initialize(GameRuntime &Runtime)`

Create or refresh the editor bootstrap scene in the runtime world.

**Parameters**

- `Runtime`: Initialized runtime whose world receives the bootstrap content.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSceneBootstrap::Shutdown(GameRuntime *Runtime)`

Destroy tracked bootstrap nodes when they still exist.

**Parameters**

- `Runtime`: Optional runtime used for world access during teardown. May be null during late shutdown.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorSceneBootstrap::EnsureEditorCamera(World &WorldRef)`

Ensure that an editor camera exists in the supplied world.

**Parameters**

- `WorldRef`: Target world.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSceneBootstrap::SyncActiveCamera(World &WorldRef)`

Synchronize the renderer's active camera choice with the world.

**Parameters**

- `WorldRef`: Target world.
</div>
<div class="snapi-api-card" markdown="1">
### `CameraComponent * SnAPI::GameFramework::Editor::EditorSceneBootstrap::ActiveCameraComponent() const`

Access the currently tracked active camera component.

**Returns:** Non-owning pointer or `nullptr` when no active camera component is known.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Graphics::ICamera * SnAPI::GameFramework::Editor::EditorSceneBootstrap::ActiveRenderCamera() const`

Access the currently tracked render-camera interface.

**Returns:** Non-owning pointer or `nullptr` when no active camera is available.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `CameraComponent * SnAPI::GameFramework::Editor::EditorSceneBootstrap::ResolveActiveCameraComponent(World &WorldRef) const`

**Parameters**

- `WorldRef`:
</div>
