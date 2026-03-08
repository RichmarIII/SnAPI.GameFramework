# SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset

One discovered asset entry exposed to editor UI.

Instances are rebuilt from the asset manager plus editor override state during `RefreshDiscovery()`. They should be treated as snapshot records rather than long-lived handles.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::Key`

Editor lookup key, currently derived from the asset id text.

Stable until discovery state is rebuilt.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::Name`

Logical asset name used by packs and runtime lookup.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::TypeLabel`

Human-readable asset-kind label for UI display.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::Variant`

Variant key within the owning asset pack entry.

Empty means the default variant.
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AssetId SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::AssetId`

Stable asset identifier used by the asset pipeline and runtime references.
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::AssetKind`

Runtime asset kind describing how the asset loads or instantiates.
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::CookedPayloadType`

Cooked payload serializer type currently associated with the asset.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::SchemaVersion`

Schema version of the currently discovered cooked payload.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::IsRuntime`

`true` when the asset currently lives in the runtime-asset store rather than only in a mounted pack.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::IsDirty`

`true` when the editor currently tracks unsaved runtime payload or metadata overrides for the asset.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::CanSave`

`true` when the current asset state is considered saveable through the editor workflow.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorAssetService::DiscoveredAsset::OwningPackPath`

Best-known path to the mounted pack containing the asset.

Empty for unresolved/runtime-only entries.
</div>
