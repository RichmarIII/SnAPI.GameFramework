# SnAPI::GameFramework::SkeletalMeshAssetRuntime

Runtime representation of a skeletal mesh asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::vector<SkeletalBonePayload> SnAPI::GameFramework::SkeletalMeshAssetRuntime::Bones`

Embedded bone list used for skinning.
</div>
<div class="snapi-api-card" markdown="1">
### `TAssetRef<SkeletonAssetRuntime> SnAPI::GameFramework::SkeletalMeshAssetRuntime::Skeleton`

Referenced skeleton asset.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TAssetRef<AnimationAssetRuntime> > SnAPI::GameFramework::SkeletalMeshAssetRuntime::Animations`

Referenced animation assets.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::SkeletalMeshAssetRuntime::SkeletonAnimationBulkIndex`

Bulk-data slot for packed skeleton-animation helper data, or `max` when absent.
</div>
