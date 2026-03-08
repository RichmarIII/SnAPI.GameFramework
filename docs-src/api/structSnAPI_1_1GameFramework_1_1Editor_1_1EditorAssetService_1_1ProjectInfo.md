# SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo

Snapshot of the currently loaded editor project.

Paths are stored as strings exactly as the editor currently tracks them. Some values are logical project-relative fields, while the `*Directory` fields are resolved filesystem paths.

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::IsLoaded`

`true` when a project file has been loaded successfully.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::Name`

Project display name from the project file.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::ProjectFilePath`

Absolute or normalized path to the loaded project file.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::ProjectRootDirectory`

Resolved filesystem directory containing the project file.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::AssetRoot`

Asset-root field stored in project settings, potentially project-relative or URI-based.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::AssetRootDirectory`

Resolved filesystem directory used as the live asset root.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::StartupLevelPack`

Startup level pack field stored in project settings.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::ProjectInfo::DefaultRenderSettingsAssetId`

Asset id string for the project's default `WorldRenderSettings` node, if any.
</div>
