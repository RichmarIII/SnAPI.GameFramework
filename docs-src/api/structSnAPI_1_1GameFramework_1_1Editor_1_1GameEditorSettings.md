# SnAPI::GameFramework::Editor::GameEditorSettings

Bootstrap settings for `GameEditor`.

`GameEditorSettings` is intentionally small because the editor host delegates most runtime concerns to `GameRuntimeSettings`. The editor-specific layer mainly decides which world flavor to create and which editor services should run around it.

Ownership:
- `Runtime` is copied into the editor host during `Initialize()`.

## Public Members

<div class="snapi-api-card" markdown="1">
### `GameRuntimeSettings SnAPI::GameFramework::Editor::GameEditorSettings::Runtime`

Runtime bootstrap settings used to create the editor world and optional subsystems.
</div>
