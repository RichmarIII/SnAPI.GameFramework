# SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails

Detail-pane payload for the currently selected content asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::Name`

User-facing asset name shown in the details pane.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::Type`

Reflected type/category label.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::Variant`

Variant or subtype label.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::AssetId`

Stable serialized asset identifier, if the asset has one.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::Status`

Human-readable status text such as load/save/import state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::IsRuntime`

`true` when the asset is runtime-only and not yet persisted into project content.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::IsDirty`

`true` when the selected asset has unsaved changes.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::CanPlace`

`true` when the asset may be instantiated or placed into the world from the browser.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails::CanSave`

`true` when a save action should be enabled for the selected asset.
</div>
