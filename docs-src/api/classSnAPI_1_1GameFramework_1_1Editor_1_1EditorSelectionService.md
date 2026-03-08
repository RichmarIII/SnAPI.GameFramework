# SnAPI::GameFramework::Editor::EditorSelectionService

Service that owns the shared editor selection model.

The service keeps the logical selection valid against runtime world churn. When the current selection no longer resolves, it falls back to the active editor camera owner before clearing.

## Private Members

<div class="snapi-api-card" markdown="1">
### `EditorSelectionModel SnAPI::GameFramework::Editor::EditorSelectionService::m_selection`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorSelectionService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::type_index > SnAPI::GameFramework::Editor::EditorSelectionService::Dependencies() const override`

Hard dependency on `EditorSceneService` so camera fallback is available.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorSelectionService::Initialize(EditorServiceContext &Context) override`

Reset selection and validate an initial fallback selection.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Keep the stored selection handle synchronized with the live world.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionService::Shutdown(EditorServiceContext &Context) override`

Clear the selection during shutdown.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `EditorSelectionModel & SnAPI::GameFramework::Editor::EditorSelectionService::Model()`

Access the owned selection model.

**Returns:** Borrowed reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const EditorSelectionModel & SnAPI::GameFramework::Editor::EditorSelectionService::Model() const`

Access the owned selection model.

**Returns:** Borrowed reference.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionService::EnsureSelectionValid(EditorServiceContext &Context, CameraComponent *ActiveCamera)`

**Parameters**

- `Context`: 
- `ActiveCamera`:
</div>
