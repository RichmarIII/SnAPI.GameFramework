# SnAPI::GameFramework::Editor::EditorTheme

Theme for editor shell widgets and chrome.

`EditorTheme` centralizes the default visual styling used by the editor module's UI. The theme exists so editor widgets can share one consistent palette, typography choice, spacing language, and widget-state styling instead of each service defining ad-hoc values.

Ownership and lifetime:
- Same as `SnAPI::UI::Theme`; the caller owns the theme object and registers it with the UI layer.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::EditorTheme::EditorTheme()`

Construct the editor theme object.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTheme::Initialize() override`

Populate theme colors, fonts, and widget style rules.
</div>
