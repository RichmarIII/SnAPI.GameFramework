# SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState

State payload for the asset-inspector modal.

The inspector modal can edit a runtime asset object, optional import settings, and an optional node hierarchy view for hierarchical assets. `TargetObject` and `ImportSettingsObject` are borrowed raw pointers; they must remain valid until a new state payload is pushed or the modal is closed.

## Contents

- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::NodeEntry

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::Open`

`true` when the asset-inspector modal should be visible.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::AssetKey`

Stable asset key of the asset being inspected.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::Title`

Modal title text.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::Status`

Human-readable status line for save/import/runtime state.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::TargetType`

Reflected type of `TargetObject`.
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::TargetObject`

Borrowed pointer to the runtime asset object currently bound into the property panel.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::ImportSettingsType`

Reflected type of `ImportSettingsObject`, if any.
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::ImportSettingsObject`

Borrowed pointer to the editable import-settings object, if any.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeEntry> SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::Nodes`

Flattened hierarchy view shown for hierarchical assets.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::SelectedNode`

Currently selected hierarchy node within the inspected asset.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::CanEditHierarchy`

`true` when hierarchy add/remove actions should be enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::HasImportSettings`

`true` when the asset exposes import settings for editing.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::RuntimeDirty`

`true` when the runtime asset object has unsaved edits.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::ImportSettingsDirty`

`true` when import settings have unsaved edits.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::IsDirty`

Aggregate dirty flag used by the modal save affordances.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::CanSave`

`true` when the save button should be enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::CanReimport`

`true` when the reimport action is available.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::PreviewIconSource`

Logical fallback icon for the preview panel.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::PreviewTextureId`

External UI texture id for the preview image, or `0` when no preview texture exists.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::PreviewWidth`

Preview texture width in pixels.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::PreviewHeight`

Preview texture height in pixels.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::PreviewStatsPrimary`

Primary preview statistics line.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::PreviewStatsSecondary`

Secondary preview statistics line.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState::SessionRevision`

Revision token used to avoid rebinding unchanged asset-editor sessions.
</div>
