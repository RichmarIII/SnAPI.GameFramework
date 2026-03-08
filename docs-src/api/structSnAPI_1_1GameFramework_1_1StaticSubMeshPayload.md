# SnAPI::GameFramework::StaticSubMeshPayload

One static submesh range within a cooked static mesh payload.

## Public Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::StaticSubMeshPayload::IndexOffset`

Starting index within the shared index stream.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::StaticSubMeshPayload::IndexCount`

Number of indices used by this submesh.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::StaticSubMeshPayload::MaterialSlot`

Material-slot index used to map runtime material instances.
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<float, 3> SnAPI::GameFramework::StaticSubMeshPayload::BoundsMin`

Axis-aligned local-space minimum bounds corner.
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<float, 3> SnAPI::GameFramework::StaticSubMeshPayload::BoundsMax`

Axis-aligned local-space maximum bounds corner.
</div>
