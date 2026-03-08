#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "Expected.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serializable asset-reference payload used inside cooked render-asset payloads.
 */
struct AssetRefPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.AssetRefPayload";

    std::string AssetName{}; /**< @brief Asset catalog name used as a human-readable and fallback identifier. */
    std::string AssetId{}; /**< @brief Canonical asset-id string used for stable cooked references. */

    bool operator==(const AssetRefPayload&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Semantic tag for a mesh bulk-data stream.
 */
enum class EMeshStreamSemantic : uint32_t
{
    Position = 0, /**< @brief Vertex position stream. */
    Normal = 1, /**< @brief Vertex normal stream. */
    Tangent = 2, /**< @brief Vertex tangent stream. */
    UV0 = 3, /**< @brief Primary UV coordinate stream. */
    UV1 = 4, /**< @brief Secondary UV coordinate stream. */
    Color = 5, /**< @brief Vertex color stream. */
    BoneIndices = 6, /**< @brief Skeletal bone-index stream. */
    BoneWeights = 7, /**< @brief Skeletal bone-weight stream. */
    Index = 8, /**< @brief Triangle index stream. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reference to one cooked bulk-data mesh stream chunk.
 */
struct MeshStreamChunkRef
{
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position; /**< @brief Meaning of the stored stream. */
    uint32_t BulkIndex = 0; /**< @brief Bulk-data slot index within the cooked asset. */
    uint32_t ElementCount = 0; /**< @brief Number of elements stored in the stream. */
    uint32_t StrideBytes = 0; /**< @brief Per-element byte stride. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One static submesh range within a cooked static mesh payload.
 */
struct StaticSubMeshPayload
{
    uint32_t IndexOffset = 0; /**< @brief Starting index within the shared index stream. */
    uint32_t IndexCount = 0; /**< @brief Number of indices used by this submesh. */
    uint32_t MaterialSlot = 0; /**< @brief Material-slot index used to map runtime material instances. */
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f}; /**< @brief Axis-aligned local-space minimum bounds corner. */
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f}; /**< @brief Axis-aligned local-space maximum bounds corner. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Cooked payload for a static mesh asset.
 */
struct StaticMeshPayload
{
    std::string Name{}; /**< @brief Source or logical mesh name. */
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f}; /**< @brief Aggregate local-space minimum bounds corner. */
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f}; /**< @brief Aggregate local-space maximum bounds corner. */
    std::vector<StaticSubMeshPayload> SubMeshes{}; /**< @brief Submesh ranges and per-submesh material-slot mapping. */
    std::vector<AssetRefPayload> MaterialInstances{}; /**< @brief Material-instance asset references indexed by material slot. */
    std::vector<MeshStreamChunkRef> Streams{}; /**< @brief Cooked bulk-data stream references for vertex and index data. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One skeletal bone definition.
 */
struct SkeletalBonePayload
{
    std::string Name{}; /**< @brief Bone name used for skeleton and animation track matching. */
    int32_t ParentIndex = -1; /**< @brief Parent bone index, or `-1` for the root bone. */
    std::array<float, 16> BindPose{}; /**< @brief Bind-pose matrix stored in a flat 4x4 float array. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Cooked payload for a skeleton asset.
 */
struct SkeletonPayload
{
    std::string Name{}; /**< @brief Skeleton name. */
    std::vector<SkeletalBonePayload> Bones{}; /**< @brief Ordered bone list. Parent indices reference this array. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One animation keyframe transform sample.
 */
struct AnimationKeyFramePayload
{
    float Time = 0.0f; /**< @brief Sample time in animation ticks. */
    std::array<float, 3> Translation{0.0f, 0.0f, 0.0f}; /**< @brief Local translation sample. */
    std::array<float, 4> Rotation{0.0f, 0.0f, 0.0f, 1.0f}; /**< @brief Local rotation quaternion sample in `(x, y, z, w)` order. */
    std::array<float, 3> Scale{1.0f, 1.0f, 1.0f}; /**< @brief Local scale sample. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One per-bone animation track.
 */
struct AnimationTrackPayload
{
    std::string BoneName{}; /**< @brief Target bone name. */
    std::vector<AnimationKeyFramePayload> KeyFrames{}; /**< @brief Ordered keyframes for the target bone. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Cooked payload for an animation asset.
 */
struct AnimationPayload
{
    std::string Name{}; /**< @brief Animation clip name. */
    float DurationSeconds = 0.0f; /**< @brief Clip duration in seconds. */
    float TicksPerSecond = 0.0f; /**< @brief Tick-to-seconds conversion rate used by the source animation. */
    std::vector<AnimationTrackPayload> Tracks{}; /**< @brief Per-bone animation tracks. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Cooked payload for a skeletal mesh asset.
 */
struct SkeletalMeshPayload
{
    StaticMeshPayload BaseMesh{}; /**< @brief Static-mesh portion shared with non-skinned mesh rendering data. */
    std::vector<SkeletalBonePayload> Bones{}; /**< @brief Embedded skeleton bone list for skinning. */
    AssetRefPayload Skeleton{}; /**< @brief Referenced skeleton asset. */
    std::vector<AssetRefPayload> Animations{}; /**< @brief Referenced animation assets associated with the mesh. */
    uint32_t SkeletonAnimationBulkIndex = std::numeric_limits<uint32_t>::max(); /**< @brief Bulk-data slot containing packed skeleton-animation helper data, or `max` when absent. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Cooked payload describing a base material contract.
 */
struct MaterialPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialPayload";

    std::string ShaderModule{}; /**< @brief Renderer shader module name. */
    std::string ShadingModel{}; /**< @brief Renderer shading-model name. */
    bool FeatureAlbedoMap = false; /**< @brief Enables albedo-map sampling features. */
    bool FeatureNormalMap = false; /**< @brief Enables normal-map sampling features. */
    bool FeatureRoughnessMap = false; /**< @brief Enables roughness-map sampling features. */
    bool FeatureMetalnessMap = false; /**< @brief Enables metalness-map sampling features. */
    bool FeatureOcclusionMap = false; /**< @brief Enables occlusion-map sampling features. */
    bool FeatureAlphaTest = false; /**< @brief Enables alpha-test behavior. */
    bool FeatureAlphaBlend = false; /**< @brief Enables alpha-blend behavior. */
    bool FeatureDoubleSided = false; /**< @brief Disables backface culling when supported. */
    bool FeatureInstancing = false; /**< @brief Enables per-instance data support. */

    bool operator==(const MaterialPayload&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One scalar material-instance override.
 */
struct MaterialScalarParamPayload
{
    std::string Name{}; /**< @brief Runtime parameter name. */
    float Value = 0.0f; /**< @brief Scalar override value. */

    bool operator==(const MaterialScalarParamPayload&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One vector material-instance override.
 */
struct MaterialVectorParamPayload
{
    std::string Name{}; /**< @brief Runtime parameter name. */
    std::array<float, 4> Value{0.0f, 0.0f, 0.0f, 0.0f}; /**< @brief Four-component override value. */

    bool operator==(const MaterialVectorParamPayload&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief One texture material-instance override.
 */
struct MaterialTextureParamPayload
{
    std::string SlotName{}; /**< @brief Runtime texture-slot or resource name. */
    AssetRefPayload Texture{}; /**< @brief Referenced texture asset. */
    bool SRGB = true; /**< @brief `true` when the texture should be sampled as sRGB data. */

    bool operator==(const MaterialTextureParamPayload&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Cooked payload for a material-instance asset.
 */
struct MaterialInstancePayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialInstancePayload";

    AssetRefPayload ParentMaterial{}; /**< @brief Referenced parent base material asset. */
    std::vector<MaterialScalarParamPayload> Scalars{}; /**< @brief Scalar parameter overrides. */
    std::vector<MaterialVectorParamPayload> Vectors{}; /**< @brief Vector parameter overrides. */
    std::vector<MaterialTextureParamPayload> Textures{}; /**< @brief Texture parameter overrides. */

    bool operator==(const MaterialInstancePayload&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `StaticMeshPayload` into binary bytes.
 */
TExpected<void> SerializeStaticMeshPayload(const StaticMeshPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `StaticMeshPayload` from binary bytes.
 */
TExpected<StaticMeshPayload> DeserializeStaticMeshPayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `SkeletalMeshPayload` into binary bytes.
 */
TExpected<void> SerializeSkeletalMeshPayload(const SkeletalMeshPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `SkeletalMeshPayload` from binary bytes.
 */
TExpected<SkeletalMeshPayload> DeserializeSkeletalMeshPayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `SkeletonPayload` into binary bytes.
 */
TExpected<void> SerializeSkeletonPayload(const SkeletonPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `SkeletonPayload` from binary bytes.
 */
TExpected<SkeletonPayload> DeserializeSkeletonPayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize an `AnimationPayload` into binary bytes.
 */
TExpected<void> SerializeAnimationPayload(const AnimationPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize an `AnimationPayload` from binary bytes.
 */
TExpected<AnimationPayload> DeserializeAnimationPayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `MaterialPayload` into binary bytes.
 */
TExpected<void> SerializeMaterialPayload(const MaterialPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `MaterialPayload` from binary bytes.
 *
 * Legacy v1 payloads containing only `ShaderModule` and `ShadingModel` are upgraded automatically.
 */
TExpected<MaterialPayload> DeserializeMaterialPayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `MaterialInstancePayload` into binary bytes.
 */
TExpected<void> SerializeMaterialInstancePayload(const MaterialInstancePayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `MaterialInstancePayload` from binary bytes.
 */
TExpected<MaterialInstancePayload> DeserializeMaterialInstancePayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
