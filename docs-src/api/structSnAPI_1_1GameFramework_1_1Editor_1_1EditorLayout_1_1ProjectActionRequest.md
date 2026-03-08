# SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest

Payload describing one project action request.

## Public Members

<div class="snapi-api-card" markdown="1">
### `EProjectAction SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest::Action`

Requested project action.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest::ProjectName`

User-facing project name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest::ProjectDirectory`

Directory that should contain the project when creating a new one.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest::ProjectFilePath`

Absolute project file path used for open/save workflows.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest::StartupLevelPack`

Asset id or pack path for the project's startup level.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest::DefaultRenderSettingsAssetId`

Default render-settings asset id chosen in project settings.
</div>
