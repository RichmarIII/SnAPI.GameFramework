# SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView

Read-only snapshot of the active asset-editor session.

The view is intentionally value-based except for `TargetObject` and `ImportSettingsObject`, which are borrowed pointers to the mutable objects currently edited by inspector UI.

## Contents

- **Type:** SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::NodeEntry

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::IsOpen`

`true` when the service currently has an active asset-editor session.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::AssetKey`

Discovery key of the asset currently being edited.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::Title`

UI-facing session title, typically `<TypeLabel> - <Name>`.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TargetType`

Reflected type of `TargetObject`, or empty when no runtime-editable object is exposed.
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TargetObject`

Borrowed pointer to the mutable runtime-editable object currently exposed in the inspector.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::ImportSettingsType`

Reflected type of `ImportSettingsObject`, or empty when no import settings are available.
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::ImportSettingsObject`

Borrowed pointer to the mutable import-settings object currently exposed in the inspector.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeEntry> SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::Nodes`

Flattened hierarchy snapshot for node or level assets.

Empty for non-hierarchical assets.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::SelectedNode`

Current selection within the asset-editor hierarchy.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::CanEditHierarchy`

`true` when add/remove node and component operations are supported for the active asset.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::HasImportSettings`

`true` when import metadata resolved to an editable reflected settings object.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::RuntimeDirty`

`true` when the runtime-editable payload differs from the session baseline.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::ImportSettingsDirty`

`true` when editable import settings differ from the stored import metadata baseline.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::IsDirty`

Aggregate dirty flag equal to `RuntimeDirty || ImportSettingsDirty`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::CanSave`

`true` when the active session can currently be saved through the editor.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::CanReimport`

`true` when a valid reimport source path and import profile are available.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::HasTexturePreviewStats`

`true` when the texture preview statistics below are populated.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TexturePreviewWidth`

Base-level texture width in texels for the previewed cooked texture.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TexturePreviewHeight`

Base-level texture height in texels for the previewed cooked texture.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TexturePreviewMipCount`

Number of mip levels present in the previewed cooked texture.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TexturePreviewTarget`

Human-readable compression target label for the previewed texture.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TexturePreviewFormat`

Human-readable compression format label for the previewed texture.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorAssetService::AssetEditorSessionView::TexturePreviewGpuSizeBytes`

Estimated GPU memory footprint in bytes for the previewed texture.
</div>
