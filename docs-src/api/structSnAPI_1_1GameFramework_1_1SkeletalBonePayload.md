# SnAPI::GameFramework::SkeletalBonePayload

One skeletal bone definition.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::SkeletalBonePayload::Name`

Bone name used for skeleton and animation track matching.
</div>
<div class="snapi-api-card" markdown="1">
### `int32_t SnAPI::GameFramework::SkeletalBonePayload::ParentIndex`

Parent bone index, or `-1` for the root bone.
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<float, 16> SnAPI::GameFramework::SkeletalBonePayload::BindPose`

Bind-pose matrix stored in a flat 4x4 float array.
</div>
