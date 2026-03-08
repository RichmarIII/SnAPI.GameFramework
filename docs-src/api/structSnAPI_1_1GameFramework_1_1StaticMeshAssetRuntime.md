# SnAPI::GameFramework::StaticMeshAssetRuntime

Runtime representation of a static mesh asset.

Bulk stream bytes are loaded lazily through `LoadBulk`.

## Public Members

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AssetId SnAPI::GameFramework::StaticMeshAssetRuntime::SourceAssetId`

Source asset id used for cache keys and diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::StaticMeshAssetRuntime::Name`

Logical mesh name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<StaticSubMeshPayload> SnAPI::GameFramework::StaticMeshAssetRuntime::SubMeshes`

Submesh index ranges and material-slot mapping.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MeshStreamChunkRef> SnAPI::GameFramework::StaticMeshAssetRuntime::Streams`

Bulk stream references for vertex and index data.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TAssetRef<MaterialInstanceAssetRuntime> > SnAPI::GameFramework::StaticMeshAssetRuntime::MaterialInstances`

Material-instance references indexed by material slot.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<std::expected<std::vector<uint8_t>, std::string>(uint32_t)> SnAPI::GameFramework::StaticMeshAssetRuntime::LoadBulk`

Callback that loads one bulk-data slot by index on demand.
</div>
