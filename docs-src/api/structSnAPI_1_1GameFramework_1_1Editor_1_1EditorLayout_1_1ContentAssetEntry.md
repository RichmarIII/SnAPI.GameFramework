# SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry

One content-browser card entry.

This structure carries the normalized display data needed to render a single asset entry in the content browser grid or list.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::Key`

Stable asset key used to route selection, placement, save, delete, and rename callbacks.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::Name`

User-facing asset display name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::Type`

Short reflected type/category label shown on the asset card.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::Variant`

Additional subtype or variant label for disambiguation.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::IconSource`

Logical fallback icon identifier used when no thumbnail texture is available.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::IconTextureId`

External UI texture id for thumbnail rendering, or `0` when no texture preview exists.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::IconWidth`

Thumbnail width in pixels.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint32_t SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::IconHeight`

Thumbnail height in pixels.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::IsRuntime`

`true` when the asset currently lives only in runtime/editor memory rather than persisted project content.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry::IsDirty`

`true` when the asset has unsaved runtime or import-setting changes.
</div>
