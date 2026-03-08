# SnAPI::GameFramework::Editor::EditorAssetService

Asset-discovery, import, editing, and instantiation backend for the editor.

`EditorAssetService` is the editor module's central asset workflow service. It owns the editor-facing `AssetManager`, maintains the live discovery index shown to asset browsers, tracks selection and placement intent, manages project-level asset roots, and hosts the temporary state used by the asset inspector.

Core responsibilities:
- discover mounted runtime and packed assets and expose a stable editor-facing list
- create, rename, delete, save, and import assets
- manage project files, startup level packs, and default render settings assets
- host a temporary asset-editor session for node, level, world, texture, mesh, and material assets
- instantiate placeable assets into the active editor world

Core semantics:
- The service owns one `AssetManager` instance and rebuilds it when project roots change.
- Discovery results are snapshots stored in internal vectors; references and pointers returned by query functions are invalidated by `RefreshDiscovery()` and many mutating operations.
- Packed-asset rename edits are staged in editor-only override maps until saved.
- Runtime payload edits in the asset inspector are tracked through cooked-byte diffs and are not persisted until `SaveActiveAssetEditor()` or `SaveAssetByKey()` succeeds.
- Import settings edits are metadata only and typically require `ReimportActiveAsset()` to affect cooked output.

Ownership and lifetime:
- Owned by `GameEditor` through the `IEditorService` contract.
- The service owns the asset manager and any temporary asset-editor world it creates.
- Raw pointers exposed through `AssetEditorSessionView` are borrowed pointers into service-owned state and become invalid when the session closes, the asset editor switches targets, discovery rebuilds affected state, or the service shuts down.

Threading model:
- Main-thread only.
- The service does not synchronize public API access.
- Some operations may perform file I/O and may block.

## Contents

- **Type:** SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset
- **Type:** SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView
- **Type:** SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo
- **Type:** SnAPI::GameFramework::Editor::EditorAssetService::AssetImportMetadataEntry

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::AssetManager> SnAPI::GameFramework::Editor::EditorAssetService::m_assetManager`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<DiscoveredAsset> SnAPI::GameFramework::Editor::EditorAssetService::m_assets`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::string, std::size_t> SnAPI::GameFramework::Editor::EditorAssetService::m_assetIndexByKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<::SnAPI::AssetPipeline::AssetId, std::string, ::SnAPI::AssetPipeline::UuidHash> SnAPI::GameFramework::Editor::EditorAssetService::m_assetRenameOverrides`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<::SnAPI::AssetPipeline::AssetId, ::SnAPI::AssetPipeline::TypedPayload, ::SnAPI::AssetPipeline::UuidHash> SnAPI::GameFramework::Editor::EditorAssetService::m_assetPayloadOverrides`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_selectedAssetKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_placementAssetKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_previewSummary`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_statusMessage`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::EditorAssetService::m_editorTemplateAssetDirectory`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::EditorAssetService::m_editorStarterLevelTemplatePackPath`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::EditorAssetService::m_editorStarterScriptTemplatePath`
</div>
<div class="snapi-api-card" markdown="1">
### `ProjectInfo SnAPI::GameFramework::Editor::EditorAssetService::m_currentProject`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorAssetService::m_loadedDefaultRenderSettingsNode`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_defaultRenderSettingsApplyPending`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorAssetService::m_defaultRenderSettingsLastPassGraphRevision`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::GameFramework::World> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorWorld`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorRootHandle`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorAssetKey`
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AssetId SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorAssetId`
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorAssetKind`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorTargetType`
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorTargetObject`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorDirty`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorCanSave`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorCanEditHierarchy`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<MaterialPayload> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorMaterialPayload`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<MaterialInstancePayload> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorMaterialInstancePayload`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<TextureCompressorPlugin::TextureCompressorCookedInfo> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorTextureCookedInfo`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<Editor::TextureAssetEditorPayload> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorTexturePayload`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<StaticMeshPayload> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorStaticMeshPayload`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<Editor::StaticMeshAssetEditorPayload> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorStaticMeshEditorPayload`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<Editor::AssimpImportSettings> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorAssimpImportSettings`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<Editor::TextureImportSettings> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorTextureImportSettings`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorImportSettingsType`
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorImportSettingsObject`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorImportSettingsDirty`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorCanReimport`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<AssetImportMetadataEntry> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorImportMetadataBaseline`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorMaterialInstanceDescriptorParentKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorBaselineCookedBytes`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorTitle`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorSelectedNode`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<AssetEditorSessionView::NodeEntry> SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorHierarchy`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorHierarchyDirty`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorDirtyCheckCooldownSeconds`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorAssetService::m_assetEditorSessionRevision`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<::SnAPI::AssetPipeline::AssetId, AssetImportMetadataEntry, ::SnAPI::AssetPipeline::UuidHash> SnAPI::GameFramework::Editor::EditorAssetService::m_assetImportMetadata`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::EditorAssetService::m_assetImportMetadataPath`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::m_assetImportMetadataDirty`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorAssetService::Name() const override`

Stable service name for diagnostics.

**Returns:** Borrowed static string view.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::Initialize(EditorServiceContext &Context) override`

Initialize asset-service state for the current editor session.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Per-frame asset-service maintenance tick.

**Parameters**

- `Context`: Borrowed editor-service context.
- `DeltaSeconds`: Variable-step frame delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::Shutdown(EditorServiceContext &Context) override`

Shutdown the asset service and release owned temporary state.

**Parameters**

- `Context`: Borrowed editor-service context.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< DiscoveredAsset > & SnAPI::GameFramework::Editor::EditorAssetService::Assets() const`

Access the current discovered-asset snapshot.

**Returns:** Borrowed vector reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const EditorAssetService::DiscoveredAsset * SnAPI::GameFramework::Editor::EditorAssetService::SelectedAsset() const`

Access the currently selected asset snapshot.

**Returns:** Non-owning pointer into the discovery array, or `nullptr` when no selection exists.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::IsPlacementArmed() const`

Query whether placement mode is currently armed.

**Returns:** `true` when `InstantiateArmedAsset()` would attempt to place an asset.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::Editor::EditorAssetService::PlacementAssetKey() const`

Access the key of the currently placement-armed asset.

**Returns:** Borrowed string reference. Empty when placement is not armed.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::SelectAssetByKey(std::string_view Key)`

Select one discovered asset by key.

**Parameters**

- `Key`: Discovery key of the asset to select.

**Returns:** `true` when the asset exists and selection was updated, otherwise `false`.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::ArmPlacementByKey(std::string_view Key)`

Arm one asset for scene placement.

**Parameters**

- `Key`: Discovery key of the asset to place.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::ClearPlacement()`

Clear placement mode.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::RefreshDiscovery()`

Rebuild the discovered-asset list from the current asset manager and editor override state.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::OpenSelectedAssetPreview()`

Load a temporary preview of the currently selected asset.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::SaveSelectedAssetUpdate()`

Save the currently selected asset.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::SaveAssetByKey(std::string_view Key)`

Persist one asset's current editor-visible state.

**Parameters**

- `Key`: Discovery key of the asset to save.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::DeleteAssetByKey(std::string_view Key)`

Delete one asset.

**Parameters**

- `Key`: Discovery key of the asset to delete.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::DeleteSelectedAsset()`

Delete the currently selected asset.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::RenameAssetByKey(std::string_view Key, std::string_view NewName)`

Rename one asset.

**Parameters**

- `Key`: Discovery key of the asset to rename.
- `NewName`: New logical asset name.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::RenameSelectedAsset(std::string_view NewName)`

Rename the currently selected asset.

**Parameters**

- `NewName`: New logical asset name.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::CreateRuntimePrefabFromNode(EditorServiceContext &Context, const NodeHandle &SourceHandle)`

Create a runtime prefab or level asset from an existing world node.

**Parameters**

- `Context`: Borrowed editor-service context.
- `SourceHandle`: Source node handle.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::CreateRuntimeNodeAssetByType(EditorServiceContext &Context, const TypeId &NodeType, std::string_view AssetName, std::string_view FolderPath)`

Create a new runtime asset from a node type.

**Parameters**

- `Context`: Borrowed editor-service context.
- `NodeType`: Reflected node type to instantiate into a scratch world.
- `AssetName`: Preferred logical asset name.
- `FolderPath`: Logical destination folder inside the asset root.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::CreateRuntimeMaterialAsset(EditorServiceContext &Context, std::string_view AssetName, std::string_view FolderPath)`

Create a runtime material asset with default payload values.

**Parameters**

- `Context`: Borrowed editor-service context.
- `AssetName`: Preferred logical asset name.
- `FolderPath`: Logical destination folder inside the asset root.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::CreateRuntimeMaterialInstanceAsset(EditorServiceContext &Context, std::string_view AssetName, std::string_view FolderPath)`

Create a runtime material-instance asset with default payload values.

**Parameters**

- `Context`: Borrowed editor-service context.
- `AssetName`: Preferred logical asset name.
- `FolderPath`: Logical destination folder inside the asset root.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::ImportSourceAsset(EditorServiceContext &Context, std::string_view SourcePath, std::string_view DestinationFolderPath, const std::unordered_map< std::string, std::string > &BuildOptions, ::SnAPI::AssetPipeline::AssetImportSettingsPtr ImportSettings={})`

Import one source file into the current asset root.

**Parameters**

- `Context`: Borrowed editor-service context.
- `SourcePath`: Source file path or resolvable URI.
- `DestinationFolderPath`: Logical destination folder inside the asset root.
- `BuildOptions`: Additional pipeline build options. Managed options may be normalized or overridden.
- `ImportSettings`: Optional typed import-settings object. When empty, importer-specific defaults are created.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::OpenAssetEditorByKey(std::string_view Key)`

Open the asset inspector for one asset.

**Parameters**

- `Key`: Discovery key of the asset to inspect.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::CloseAssetEditor()`

Close the active asset-editor session.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::SelectAssetEditorNode(const NodeHandle &Node)`

Change the selected node inside the active asset-editor hierarchy.

**Parameters**

- `Node`: Requested node handle. A null handle selects the root.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::AddAssetEditorNode(const NodeHandle &Parent, const TypeId &NodeType)`

Add a child node inside the active hierarchical asset editor.

**Parameters**

- `Parent`: Parent node handle. A null handle means the current asset-editor root.
- `NodeType`: Reflected node type to create.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::DeleteAssetEditorNode(const NodeHandle &Node)`

Delete one node from the active hierarchical asset editor.

**Parameters**

- `Node`: Node handle to delete.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::AddAssetEditorComponent(const NodeHandle &Owner, const TypeId &ComponentType)`

Add a component to a node in the active hierarchical asset editor.

**Parameters**

- `Owner`: Owner node handle.
- `ComponentType`: Reflected component type to create.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::RemoveAssetEditorComponent(const NodeHandle &Owner, const TypeId &ComponentType)`

Remove a component from a node in the active hierarchical asset editor.

**Parameters**

- `Owner`: Owner node handle.
- `ComponentType`: Reflected component type to remove by type.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::TickAssetEditorSession(float DeltaSeconds=0.0f)`

Advance active asset-editor dirty-state tracking.

**Parameters**

- `DeltaSeconds`: Variable-step frame delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::SaveActiveAssetEditor()`

Save the active asset-editor session.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::ReimportActiveAsset(EditorServiceContext &Context)`

Reimport the asset currently open in the asset editor.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `EditorAssetService::AssetEditorSessionView SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSession() const`

Snapshot the active asset-editor session.

**Returns:** Value snapshot of the current session state.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionRevision() const`

Monotonic revision counter for asset-editor UI invalidation.

**Returns:** Revision number incremented when session-visible state changes.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::InstantiateArmedAsset(EditorServiceContext &Context)`

Instantiate the currently placement-armed asset into the active runtime world.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::InstantiateAssetByKey(EditorServiceContext &Context, std::string_view Key)`

Instantiate one asset into the active runtime world.

**Parameters**

- `Context`: Borrowed editor-service context.
- `Key`: Discovery key of the asset to instantiate.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::CreateProject(EditorServiceContext &Context, std::string_view ProjectName, std::string_view ParentDirectory)`

Create a new project on disk and load it immediately.

**Parameters**

- `Context`: Borrowed editor-service context.
- `ProjectName`: New project name.
- `ParentDirectory`: Parent directory that will contain the project folder.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::LoadProject(EditorServiceContext &Context, std::string_view ProjectFilePath)`

Load an existing project file.

**Parameters**

- `Context`: Borrowed editor-service context.
- `ProjectFilePath`: Project file path or resolvable URI.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::SaveProjectSettings(EditorServiceContext &Context, std::string_view ProjectName, std::string_view StartupLevelPack, std::string_view DefaultRenderSettingsAssetId)`

Persist editable project settings to the loaded project file.

**Parameters**

- `Context`: Borrowed editor-service context.
- `ProjectName`: Updated project name. Empty keeps the current name.
- `StartupLevelPack`: Updated startup level pack field. Empty keeps the current field.
- `DefaultRenderSettingsAssetId`: Updated default render settings asset id. Empty keeps the current value.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `const ProjectInfo & SnAPI::GameFramework::Editor::EditorAssetService::CurrentProject() const`

Access the current project snapshot.

**Returns:** Borrowed project-info reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::Editor::EditorAssetService::PreviewSummary() const`

Access the last preview summary string.

**Returns:** Borrowed string reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::Editor::EditorAssetService::StatusMessage() const`

Access the latest human-readable status message.

**Returns:** Borrowed string reference.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `std::vector< std::string > SnAPI::GameFramework::Editor::EditorAssetService::BuildPackSearchPaths() const`
</div>
<div class="snapi-api-card" markdown="1">
### `const EditorAssetService::DiscoveredAsset * SnAPI::GameFramework::Editor::EditorAssetService::FindAssetByKey(std::string_view Key) const`

**Parameters**

- `Key`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< std::string, std::string > SnAPI::GameFramework::Editor::EditorAssetService::ResolveOwningPackPath(const DiscoveredAsset &Asset) const`

**Parameters**

- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< std::string, std::string > SnAPI::GameFramework::Editor::EditorAssetService::ResolveRuntimeSavePath(const DiscoveredAsset &Asset) const`

**Parameters**

- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string > SnAPI::GameFramework::Editor::EditorAssetService::BuildCookedPayloadForAsset(const DiscoveredAsset &Asset)`

**Parameters**

- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::InstantiateNodeAsset(EditorServiceContext &Context, const DiscoveredAsset &Asset)`

**Parameters**

- `Context`: 
- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::InstantiateLevelAsset(EditorServiceContext &Context, const DiscoveredAsset &Asset)`

**Parameters**

- `Context`: 
- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::InstantiateWorldAsset(EditorServiceContext &Context, const DiscoveredAsset &Asset)`

**Parameters**

- `Context`: 
- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::RebuildAssetManager()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::EnsureEditorTemplateAssets(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::EnsureProjectShaderDirectory(const std::filesystem::path &ProjectAssetRoot)`

**Parameters**

- `ProjectAssetRoot`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::EnsureProjectStarterLevelPack(const std::filesystem::path &ProjectAssetRoot, const std::filesystem::path &StartupPackPath)`

**Parameters**

- `ProjectAssetRoot`: 
- `StartupPackPath`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::LoadProjectStartupLevel(EditorServiceContext &Context, const std::filesystem::path &StartupPackPath)`

**Parameters**

- `Context`: 
- `StartupPackPath`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::LoadProjectDefaultRenderSettings(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string > SnAPI::GameFramework::Editor::EditorAssetService::SerializeAssetEditorPayload() const`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetService::SyncMaterialInstanceEditorPayloadFromDescriptor()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::EditorAssetService::ResolveImportMetadataPath() const`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< void, std::string > SnAPI::GameFramework::Editor::EditorAssetService::LoadAssetImportMetadataDatabase()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< void, std::string > SnAPI::GameFramework::Editor::EditorAssetService::SaveAssetImportMetadataDatabase() const`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::RefreshAssetEditorImportSettingsBinding(const DiscoveredAsset &Asset)`

**Parameters**

- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional< EditorAssetService::AssetImportMetadataEntry > SnAPI::GameFramework::Editor::EditorAssetService::BuildAssetEditorImportMetadataFromCurrentState() const`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::ImportMetadataRecordsEqual(const AssetImportMetadataEntry &Left, const AssetImportMetadataEntry &Right) const`

**Parameters**

- `Left`: 
- `Right`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::AssetPipeline::AssetImportSettingsPtr SnAPI::GameFramework::Editor::EditorAssetService::BuildTypedImportSettingsForRecord(const AssetImportMetadataEntry &Record) const`

**Parameters**

- `Record`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::ClearAssetEditorImportSettingsBinding()`
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::Editor::EditorAssetService::ResolveAssetEditorNode(const NodeHandle &Node) const`

**Parameters**

- `Node`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::RefreshAssetEditorHierarchy()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetService::ClearAssetEditorState()`
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `std::vector< std::string > SnAPI::GameFramework::Editor::EditorAssetService::ParsePackSearchPathEnv(std::string_view Raw)`

**Parameters**

- `Raw`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::AssetKindToLabel(const ::SnAPI::AssetPipeline::TypeId &AssetKind)`

**Parameters**

- `AssetKind`:
</div>
