# SnAPI::GameFramework::Editor::StaticMeshAssetEditorPayload

Editor-facing view of static mesh payload fields that are directly editable.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::Editor::StaticMeshAssetEditorPayload::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::StaticMeshAssetEditorPayload::Name`

Logical mesh name stored in the cooked payload.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<AssetRefPayload> SnAPI::GameFramework::Editor::StaticMeshAssetEditorPayload::MaterialInstances`

Ordered material-instance overrides referenced by the mesh sections.
</div>
