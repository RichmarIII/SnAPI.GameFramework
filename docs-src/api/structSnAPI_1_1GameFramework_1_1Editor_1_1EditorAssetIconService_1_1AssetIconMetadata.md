# SnAPI::GameFramework::Editor::EditorAssetIconService::AssetIconMetadata

Resolved icon payload for one content-browser entry.

`IconSource` names the logical fallback icon while the texture fields describe an optional external UI texture binding for thumbnail rendering.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetIconService::AssetIconMetadata::IconSource`

Logical icon identifier used when no texture thumbnail is available.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetIconService::AssetIconMetadata::TextureId`

UI-context-local external texture id, or `0` when no thumbnail binding exists.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetIconService::AssetIconMetadata::TextureWidth`

Width of the external thumbnail texture in pixels.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorAssetIconService::AssetIconMetadata::TextureHeight`

Height of the external thumbnail texture in pixels.
</div>
