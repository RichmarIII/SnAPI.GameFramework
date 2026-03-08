# SnAPI::GameFramework::SkeletalMeshPayload

Cooked payload for a skeletal mesh asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `StaticMeshPayload SnAPI::GameFramework::SkeletalMeshPayload::BaseMesh`

Static-mesh portion shared with non-skinned mesh rendering data.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<SkeletalBonePayload> SnAPI::GameFramework::SkeletalMeshPayload::Bones`

Embedded skeleton bone list for skinning.
</div>
<div class="snapi-api-card" markdown="1">
### `AssetRefPayload SnAPI::GameFramework::SkeletalMeshPayload::Skeleton`

Referenced skeleton asset.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<AssetRefPayload> SnAPI::GameFramework::SkeletalMeshPayload::Animations`

Referenced animation assets associated with the mesh.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::SkeletalMeshPayload::SkeletonAnimationBulkIndex`

Bulk-data slot containing packed skeleton-animation helper data, or `max` when absent.
</div>
