# SnAPI::GameFramework::Editor::EditorLayout::ContentAssetCreateRequest

Request payload emitted when the create-asset modal is confirmed.

## Public Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::ContentAssetCreateRequest::Type`

Reflected asset type selected in the create modal.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetCreateRequest::Name`

User-entered asset name after layout-side trimming and validation.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetCreateRequest::FolderPath`

Content-browser folder path that should receive the new asset.
</div>
