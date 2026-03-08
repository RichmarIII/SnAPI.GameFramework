#pragma once

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include "AssetRef.h"
#include "RenderAssetPayloads.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <Image.hpp>
#endif

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Placeholder runtime texture type used when renderer support is not compiled in.
 */
struct TextureAssetRuntime
{
};

#if defined(SNAPI_GF_ENABLE_RENDERER)
using RuntimeTextureAsset = ::SnAPI::Graphics::IGPUImage;
#else
using RuntimeTextureAsset = TextureAssetRuntime;
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime representation of a base material asset.
 *
 * This is the resolved form consumed by renderer/material binding helpers rather than the on-disk
 * serialized payload object.
 */
struct MaterialAssetRuntime
{
    std::string ShaderModule{}; /**< @brief Renderer shader module name. */
    std::string ShadingModel{}; /**< @brief Renderer shading-model name. */
    bool FeatureAlbedoMap = false; /**< @brief Enables albedo texture features. */
    bool FeatureNormalMap = false; /**< @brief Enables normal texture features. */
    bool FeatureRoughnessMap = false; /**< @brief Enables roughness texture features. */
    bool FeatureMetalnessMap = false; /**< @brief Enables metalness texture features. */
    bool FeatureOcclusionMap = false; /**< @brief Enables occlusion texture features. */
    bool FeatureAlphaTest = false; /**< @brief Enables alpha-test rendering behavior. */
    bool FeatureAlphaBlend = false; /**< @brief Enables alpha-blend rendering behavior. */
    bool FeatureDoubleSided = false; /**< @brief Enables double-sided rendering when supported. */
    bool FeatureInstancing = false; /**< @brief Enables per-instance data support. */
    bool bLegacyInferFeaturesFromTextures = false; /**< @brief Compatibility flag for older material assets that infer features from bound textures. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime representation of a material-instance asset.
 */
struct MaterialInstanceAssetRuntime
{
    TAssetRef<MaterialAssetRuntime> ParentMaterial{}; /**< @brief Referenced parent base material. */
    std::vector<MaterialScalarParamPayload> Scalars{}; /**< @brief Scalar parameter overrides. */
    std::vector<MaterialVectorParamPayload> Vectors{}; /**< @brief Vector parameter overrides. */
    std::vector<std::string> TextureSlots{}; /**< @brief Texture-slot names paired with `Textures` by index. */
    std::vector<TAssetRef<RuntimeTextureAsset>> Textures{}; /**< @brief Texture references paired with `TextureSlots` by index. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime representation of a static mesh asset.
 *
 * Bulk stream bytes are loaded lazily through `LoadBulk`.
 */
struct StaticMeshAssetRuntime
{
    ::SnAPI::AssetPipeline::AssetId SourceAssetId{}; /**< @brief Source asset id used for cache keys and diagnostics. */
    std::string Name{}; /**< @brief Logical mesh name. */
    std::vector<StaticSubMeshPayload> SubMeshes{}; /**< @brief Submesh index ranges and material-slot mapping. */
    std::vector<MeshStreamChunkRef> Streams{}; /**< @brief Bulk stream references for vertex and index data. */
    std::vector<TAssetRef<MaterialInstanceAssetRuntime>> MaterialInstances{}; /**< @brief Material-instance references indexed by material slot. */
    std::function<std::expected<std::vector<uint8_t>, std::string>(uint32_t)> LoadBulk{}; /**< @brief Callback that loads one bulk-data slot by index on demand. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime representation of a skeleton asset.
 */
struct SkeletonAssetRuntime
{
    std::string Name{}; /**< @brief Skeleton name. */
    std::vector<SkeletalBonePayload> Bones{}; /**< @brief Ordered bone list. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime representation of an animation asset.
 */
struct AnimationAssetRuntime
{
    std::string Name{}; /**< @brief Animation clip name. */
    float DurationSeconds = 0.0f; /**< @brief Clip duration in seconds. */
    float TicksPerSecond = 0.0f; /**< @brief Tick rate used by the source animation data. */
    std::vector<AnimationTrackPayload> Tracks{}; /**< @brief Per-bone animation tracks. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime representation of a skeletal mesh asset.
 */
struct SkeletalMeshAssetRuntime : public StaticMeshAssetRuntime
{
    std::vector<SkeletalBonePayload> Bones{}; /**< @brief Embedded bone list used for skinning. */
    TAssetRef<SkeletonAssetRuntime> Skeleton{}; /**< @brief Referenced skeleton asset. */
    std::vector<TAssetRef<AnimationAssetRuntime>> Animations{}; /**< @brief Referenced animation assets. */
    uint32_t SkeletonAnimationBulkIndex = std::numeric_limits<uint32_t>::max(); /**< @brief Bulk-data slot for packed skeleton-animation helper data, or `max` when absent. */
};

} // namespace SnAPI::GameFramework
