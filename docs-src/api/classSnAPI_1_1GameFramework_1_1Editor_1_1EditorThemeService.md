# SnAPI::GameFramework::Editor::EditorThemeService

Service that owns and exposes the active editor theme.

This service centralizes theme lifetime so layout code can safely borrow a single `EditorTheme` instance during build and rebuild operations.

## Private Members

<div class="snapi-api-card" markdown="1">
### `EditorTheme SnAPI::GameFramework::Editor::EditorThemeService::m_theme`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorThemeService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorThemeService::Initialize(EditorServiceContext &Context) override`

Initialize the owned `EditorTheme`.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorThemeService::Shutdown(EditorServiceContext &Context) override`

Shutdown hook.

The owned theme object remains value-owned by the service.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `EditorTheme & SnAPI::GameFramework::Editor::EditorThemeService::Theme()`

Access the owned editor theme.

**Returns:** Borrowed theme reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const EditorTheme & SnAPI::GameFramework::Editor::EditorThemeService::Theme() const`

Access the owned editor theme.

**Returns:** Borrowed theme reference.
</div>
