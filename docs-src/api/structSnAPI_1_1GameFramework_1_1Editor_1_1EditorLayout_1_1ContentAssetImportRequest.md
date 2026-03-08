# SnAPI::GameFramework::Editor::EditorLayout::ContentAssetImportRequest

Request payload emitted when the import-asset modal is confirmed.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetImportRequest::SourcePath`

Source file path chosen by the user for import.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetImportRequest::FolderPath`

Destination content folder path inside the current project.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::string, std::string> SnAPI::GameFramework::Editor::EditorLayout::ContentAssetImportRequest::BuildOptions`

Normalized string build options derived from the selected import profile.
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AssetImportSettingsPtr SnAPI::GameFramework::Editor::EditorLayout::ContentAssetImportRequest::ImportSettings`

Owning pointer to the typed import-settings object selected in the modal.
</div>
