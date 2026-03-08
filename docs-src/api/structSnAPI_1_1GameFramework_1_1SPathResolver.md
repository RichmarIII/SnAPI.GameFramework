# SnAPI::GameFramework::SPathResolver

Global schema-aware resolver for filesystem-like path strings.

`SPathResolver` translates logical URI-style inputs such as `asset://...` and `editor://...` into normalized filesystem paths. It centralizes schema registration so subsystems can exchange stable logical paths without hard-coding project-relative or install-relative disk layouts.

Core semantics:
- Known schemas dispatch to registered handlers.
- Plain paths without `scheme://` are normalized and treated as native filesystem paths.
- Built-in `asset://` and `editor://` handlers enforce that resolved paths stay within their configured roots.

Ownership and lifetime:
- This is a process-wide singleton.
- Registered schema handlers are copied into internal storage and remain active until explicitly removed.

Threading model:
- Concurrent calls are internally synchronized.

## Contents

- **Type:** SnAPI::GameFramework::SPathResolver::ParsedSchema

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::SPathResolver::SchemaHandler = std::function<TExpected<std::filesystem::path>(std::string_view Remainder)>`

Callback signature for custom schema resolution.

**Returns:** Resolved filesystem path, or an error when unresolved/invalid.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::SPathResolver::m_mutex`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::string, SchemaHandler> SnAPI::GameFramework::SPathResolver::m_handlers`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::m_assetRoot`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::m_editorRoot`
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `SPathResolver & SnAPI::GameFramework::SPathResolver::Instance()`

Access the process-wide resolver singleton.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< std::filesystem::path > SnAPI::GameFramework::SPathResolver::Resolve(std::string_view Value) const`

Resolve path text to a normalized filesystem path.

Resolution behavior:
- `scheme://...` => dispatch to registered schema handler
- otherwise => treat as native filesystem path

**Parameters**

- `Value`: Input path or URI in `scheme://...` form or a native filesystem path.

**Returns:** Resolved filesystem path or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::string > SnAPI::GameFramework::SPathResolver::ResolveToString(std::string_view Value) const`

Resolve a path and return it as a string.

**Parameters**

- `Value`: Input path or URI.

**Returns:** Resolved filesystem path encoded as a string, or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::SPathResolver::RegisterSchemaHandler(std::string_view Scheme, SchemaHandler Handler)`

Register or replace a custom schema handler.

Schema names are normalized to lowercase ASCII and must satisfy the resolver's schema-name rules.

**Parameters**

- `Scheme`: Schema identifier without the `://` delimiter.
- `Handler`: Resolution callback.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::SPathResolver::UnregisterSchemaHandler(std::string_view Scheme)`

Remove a previously registered schema handler.

**Parameters**

- `Scheme`: Schema identifier without delimiter.

**Returns:** `true` if a handler was removed.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::SPathResolver::SetAssetRoot(std::filesystem::path RootPath)`

Set the root directory used by the built-in `asset://` schema.

**Parameters**

- `RootPath`: Filesystem root.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::AssetRoot() const`

Get the current `asset://` root directory.

**Returns:** Copy of the configured asset root path.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::SPathResolver::SetEditorRoot(std::filesystem::path RootPath)`

Set the root directory used by the built-in `editor://` schema.

**Parameters**

- `RootPath`: Filesystem root.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::EditorRoot() const`

Get the current `editor://` root directory.

**Returns:** Copy of the configured editor root path.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::SPathResolver::SPathResolver()`
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::filesystem::path > SnAPI::GameFramework::SPathResolver::ResolveAssetPath(std::string_view Remainder) const`

**Parameters**

- `Remainder`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::filesystem::path > SnAPI::GameFramework::SPathResolver::ResolveEditorPath(std::string_view Remainder) const`

**Parameters**

- `Remainder`:
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `std::optional< SPathResolver::ParsedSchema > SnAPI::GameFramework::SPathResolver::ParseSchema(std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::SPathResolver::IsValidSchemaName(std::string_view Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::SPathResolver::ToLowerAscii(std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::NormalizeForFilesystem(std::filesystem::path Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::ResolveDefaultAssetRoot()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::SPathResolver::ResolveDefaultEditorRoot()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::SPathResolver::IsPathWithin(const std::filesystem::path &Root, const std::filesystem::path &Candidate)`

**Parameters**

- `Root`: 
- `Candidate`:
</div>
