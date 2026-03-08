# SnAPI::GameFramework::StaticMeshPayload

Cooked payload for a static mesh asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::StaticMeshPayload::Name`

Source or logical mesh name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<float, 3> SnAPI::GameFramework::StaticMeshPayload::BoundsMin`

Aggregate local-space minimum bounds corner.
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<float, 3> SnAPI::GameFramework::StaticMeshPayload::BoundsMax`

Aggregate local-space maximum bounds corner.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<StaticSubMeshPayload> SnAPI::GameFramework::StaticMeshPayload::SubMeshes`

Submesh ranges and per-submesh material-slot mapping.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<AssetRefPayload> SnAPI::GameFramework::StaticMeshPayload::MaterialInstances`

Material-instance asset references indexed by material slot.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MeshStreamChunkRef> SnAPI::GameFramework::StaticMeshPayload::Streams`

Cooked bulk-data stream references for vertex and index data.
</div>
