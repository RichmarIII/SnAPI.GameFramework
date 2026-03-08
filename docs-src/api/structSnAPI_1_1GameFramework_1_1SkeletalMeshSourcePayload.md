# SnAPI::GameFramework::SkeletalMeshSourcePayload

Source-intermediate payload for a skeletal mesh asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `StaticMeshSourcePayload SnAPI::GameFramework::SkeletalMeshSourcePayload::BaseMesh`

Embedded static-mesh source payload shared with non-skinned geometry data.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<SkeletalBonePayload> SnAPI::GameFramework::SkeletalMeshSourcePayload::Bones`

Imported bone list.
</div>
<div class="snapi-api-card" markdown="1">
### `AssetRefPayload SnAPI::GameFramework::SkeletalMeshSourcePayload::Skeleton`

Referenced or generated skeleton asset.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<AssetRefPayload> SnAPI::GameFramework::SkeletalMeshSourcePayload::Animations`

Referenced or generated animation assets.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::SkeletalMeshSourcePayload::SkeletonAnimationUri`

Optional source URI for packed skeleton-animation helper data.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::SkeletalMeshSourcePayload::SkeletonAnimationBytes`

Inline packed skeleton-animation helper bytes when embedded.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::SkeletalMeshSourcePayload::SkeletonAnimationSubIndex`

Substream index for skeleton-animation helper data.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::SkeletalMeshSourcePayload::CompressSkeletonAnimation`

`true` when cooker stages should compress the packed skeleton-animation helper bytes.
</div>
