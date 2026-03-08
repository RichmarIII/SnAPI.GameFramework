# SnAPI::GameFramework::Editor::EditorLayoutService

Builds and synchronizes the editor shell UI layout.

`EditorLayoutService` is the bridge between service-layer state and the concrete `EditorLayout` widget tree. It constructs the shell once the required editor services exist, translates UI callbacks into queued requests, and applies those requests during its own `Tick()` so the rest of the editor observes deterministic main-thread ordering.

Core semantics:
- Layout event handlers never mutate editor state directly; they queue requests onto this service.
- `Tick()` drains queued project, asset, hierarchy, inspector, and toolbar actions in a stable order.
- Asset browser and inspector state are only pushed into the layout when the relevant signatures or icon/session revisions change, keeping steady-state UI churn low.
- Project load/create success forces an editor-camera refresh, clears selection and command history, and requests a layout rebuild so the shell reflects the new project state.

Ownership and lifetime:
- The service value-owns the `EditorLayout`.
- Returned pointers such as `GameViewportElement()` are borrowed and remain valid only while the layout is built and has not been rebuilt or shut down.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `EditorLayout SnAPI::GameFramework::Editor::EditorLayoutService::m_layout`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingSelectionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingSelectionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingHierarchyActionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::HierarchyActionRequest SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingHierarchyActionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingToolbarAction`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::EToolbarAction SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingToolbarAction`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingProjectActionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::ProjectActionRequest SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingProjectActionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetSelection`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetSelectionDoubleClick`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetSelectionKey`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetPlaceRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetPlaceKey`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetSaveRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetSaveKey`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetDeleteRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetDeleteKey`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetRenameRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetRenameKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetRenameValue`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetRefreshRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetCreateRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::ContentAssetCreateRequest SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetCreateRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetImportRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::ContentAssetImportRequest SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetImportRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetInspectorSaveRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetInspectorReimportRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetInspectorCloseRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetInspectorNodeSelectionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetInspectorNodeSelection`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_hasPendingAssetInspectorHierarchyActionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::HierarchyActionRequest SnAPI::GameFramework::Editor::EditorLayoutService::m_pendingAssetInspectorHierarchyActionRequest`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::m_layoutRebuildRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorLayoutService::m_assetListSignature`
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorLayoutService::m_assetDetailsSignature`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorLayoutService::m_assetInspectorSessionRevision`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorLayoutService::m_assetInspectorIconRevision`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorLayoutService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::type_index > SnAPI::GameFramework::Editor::EditorLayoutService::Dependencies() const override`

Hard dependencies on the theme, scene, selection, PIE, viewport, command, asset, and icon services.

**Returns:** Exact-type dependency list.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorLayoutService::Initialize(EditorServiceContext &Context) override`

Build the initial editor shell and install layout delegates.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayoutService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Drain queued UI requests and synchronize layout state for the current frame.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayoutService::Shutdown(EditorServiceContext &Context) override`

Destroy the layout and clear pending requests.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `UIRenderViewport * SnAPI::GameFramework::Editor::EditorLayoutService::GameViewportElement() const`

Access the live game-viewport UI element.

**Returns:** Non-owning pointer or `nullptr` if no layout is built.
</div>
<div class="snapi-api-card" markdown="1">
### `int32_t SnAPI::GameFramework::Editor::EditorLayoutService::GameViewportTabIndex() const`

Index of the active game-view tab container entry, or a negative value if unavailable.
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::EGizmoSpace SnAPI::GameFramework::Editor::EditorLayoutService::GizmoSpace() const`

Current gizmo-space selection as chosen in the editor tools UI.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayoutService::GizmoSnappingEnabled() const`

Query whether transform snapping is enabled in the tools UI.
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayoutService::MoveSnapStep() const`

Current translation snap step in world units.
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayoutService::RotateSnapStepDegrees() const`

Current rotation snap step in degrees.
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayoutService::ScaleSnapStep() const`

Current scale snap step in scalar units.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayoutService::ApplyAssetBrowserState(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayoutService::QueueLayoutRebuild()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayoutService::RebuildLayout(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
