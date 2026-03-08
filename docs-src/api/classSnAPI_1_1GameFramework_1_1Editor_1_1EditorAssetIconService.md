# SnAPI::GameFramework::Editor::EditorAssetIconService

Resolves icon metadata and UI texture bindings for content browser assets.

This service converts `EditorAssetService::DiscoveredAsset` records into lightweight icon metadata that `EditorLayoutService` can hand directly to UI widgets. For plain asset kinds it returns fallback icon identifiers. For texture-backed assets it also manages transient external UI texture registrations scoped to one `UIContext`.

Core semantics:
- Texture thumbnail bindings are created lazily when an asset icon is resolved inside a specific UI context.
- Changing the bound UI context invalidates every existing thumbnail binding because external texture ids are context-local.
- `Revision()` increments whenever icon bindings change so higher-level UI code can cheaply decide whether it needs to refresh rendered content.

Ownership and lifetime:
- The service owns the external texture bindings it allocates.
- Returned `AssetIconMetadata` values are copies; their `TextureId` remains meaningful only while the same UI context is current and the underlying binding has not been invalidated.

Threading model:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::Editor::EditorAssetIconService::AssetIconMetadata
- **Type:** SnAPI::GameFramework::Editor::EditorAssetIconService::TextureBinding

## Private Members

<div class="snapi-api-card" markdown="1">
### `const SnAPI::UI::UIContext* SnAPI::GameFramework::Editor::EditorAssetIconService::m_boundContext`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::string, std::shared_ptr<TextureBinding> > SnAPI::GameFramework::Editor::EditorAssetIconService::m_textureBindingsByAssetKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetIconService::m_nextTextureId`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorAssetIconService::m_revision`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::EditorAssetIconService::~EditorAssetIconService() override`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorAssetIconService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::type_index > SnAPI::GameFramework::Editor::EditorAssetIconService::Dependencies() const override`

Hard dependency on `EditorAssetService` so discovered assets and load helpers are available.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorAssetIconService::Initialize(EditorServiceContext &Context) override`

Reset cached bindings for a fresh editor session.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetIconService::Shutdown(EditorServiceContext &Context) override`

Release all external texture bindings.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetIconService::Synchronize(EditorServiceContext &Context, const std::vector< EditorAssetService::DiscoveredAsset > &Assets, const SnAPI::UI::UIContext *UiContext)`

Synchronize the cached icon-binding set with the currently visible asset list.

The service drops bindings for assets that are no longer present and fully resets when the UI context changes.

**Parameters**

- `Context`: Borrowed editor-service context.
- `Assets`: Borrowed view of the asset entries that should remain icon-resolvable.
- `UiContext`: Borrowed UI context that will consume the resulting texture ids, or `nullptr` to force fallback-only behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetIconService::InvalidateAsset(EditorServiceContext &Context, std::string_view AssetKey)`

Invalidate one asset's cached icon data.

Use this after saves, reimports, or deletes that can change the asset's preview.

**Parameters**

- `Context`: Borrowed editor-service context.
- `AssetKey`: Stable discovered-asset key.
</div>
<div class="snapi-api-card" markdown="1">
### `EditorAssetIconService::AssetIconMetadata SnAPI::GameFramework::Editor::EditorAssetIconService::ResolveAssetIcon(EditorServiceContext &Context, const EditorAssetService::DiscoveredAsset &Asset, const SnAPI::UI::UIContext *UiContext)`

Resolve icon metadata for one discovered asset.

**Parameters**

- `Context`: Borrowed editor-service context.
- `Asset`: Borrowed discovered-asset description.
- `UiContext`: Borrowed UI context that will render the icon.

**Returns:** A value payload containing fallback icon source and optional thumbnail texture binding.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorAssetIconService::Revision() const`

Monotonic revision counter for icon-binding invalidation.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `EditorAssetIconService::AssetIconMetadata SnAPI::GameFramework::Editor::EditorAssetIconService::BuildFallbackIcon(const EditorAssetService::DiscoveredAsset &Asset) const`

**Parameters**

- `Asset`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetIconService::AllocateTextureId()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetIconService::RemoveBinding(EditorServiceContext &Context, std::string_view AssetKey)`

**Parameters**

- `Context`: 
- `AssetKey`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorAssetIconService::ResetAllBindings(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
