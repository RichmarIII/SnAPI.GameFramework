#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Expected.h"
#include "RenderAssetPayloads.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Source-side mesh stream payload before cooking into bulk-data chunks.
 */
struct MeshStreamSourcePayload
{
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position; /**< @brief Meaning of the source stream. */
    uint32_t SubIndex = 0; /**< @brief Substream index used when one semantic appears multiple times. */
    std::string Uri{}; /**< @brief Optional source URI for traceability or deferred loading. */
    std::vector<uint8_t> Bytes{}; /**< @brief Inline raw stream bytes when source data is embedded. */
    uint32_t ElementCount = 0; /**< @brief Number of logical elements in the stream. */
    uint32_t StrideBytes = 0; /**< @brief Per-element stride in bytes. */
    bool Compress = true; /**< @brief `true` when cooker stages should attempt to compress the stored byte payload. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Source-import policy flags for mesh assets.
 */
struct MeshImportSettingsPayload
{
    bool GenerateNormals = true; /**< @brief Generate normals when the source data does not provide them. */
    bool GenerateTangents = true; /**< @brief Generate tangent frames when needed. */
    bool FlipUVs = false; /**< @brief Flip V texture coordinates during import. */
    bool OptimizeMeshes = true; /**< @brief Run mesh optimization passes when supported by the importer. */
    bool ForceSkeletal = false; /**< @brief Force skeletal import even if the source could be treated as static. */
    bool ForceStatic = false; /**< @brief Force static import even if the source contains skeletal data. */
    bool ImportMaterials = true; /**< @brief Import source materials. */
    bool ImportTextures = true; /**< @brief Import referenced textures. */
    bool ImportAnimations = true; /**< @brief Import animation clips. */
    bool ImportSkeleton = true; /**< @brief Import or generate a skeleton asset. */
    uint32_t MaxBonesPerVertex = 4; /**< @brief Maximum number of bone influences retained per vertex. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Source-intermediate payload for a static mesh asset.
 */
struct StaticMeshSourcePayload
{
    StaticMeshPayload Mesh{}; /**< @brief Logical mesh metadata and material references. */
    std::vector<MeshStreamSourcePayload> Streams{}; /**< @brief Raw source streams that will be cooked into bulk data. */
    MeshImportSettingsPayload ImportSettings{}; /**< @brief Import settings captured at source-processing time. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Source-intermediate payload for a skeletal mesh asset.
 */
struct SkeletalMeshSourcePayload
{
    StaticMeshSourcePayload BaseMesh{}; /**< @brief Embedded static-mesh source payload shared with non-skinned geometry data. */
    std::vector<SkeletalBonePayload> Bones{}; /**< @brief Imported bone list. */
    AssetRefPayload Skeleton{}; /**< @brief Referenced or generated skeleton asset. */
    std::vector<AssetRefPayload> Animations{}; /**< @brief Referenced or generated animation assets. */
    std::string SkeletonAnimationUri{}; /**< @brief Optional source URI for packed skeleton-animation helper data. */
    std::vector<uint8_t> SkeletonAnimationBytes{}; /**< @brief Inline packed skeleton-animation helper bytes when embedded. */
    uint32_t SkeletonAnimationSubIndex = 0; /**< @brief Substream index for skeleton-animation helper data. */
    bool CompressSkeletonAnimation = true; /**< @brief `true` when cooker stages should compress the packed skeleton-animation helper bytes. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `StaticMeshSourcePayload` into binary bytes.
 */
TExpected<void> SerializeStaticMeshSourcePayload(const StaticMeshSourcePayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `StaticMeshSourcePayload` from binary bytes.
 */
TExpected<StaticMeshSourcePayload> DeserializeStaticMeshSourcePayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialize a `SkeletalMeshSourcePayload` into binary bytes.
 */
TExpected<void> SerializeSkeletalMeshSourcePayload(const SkeletalMeshSourcePayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Deserialize a `SkeletalMeshSourcePayload` from binary bytes.
 */
TExpected<SkeletalMeshSourcePayload> DeserializeSkeletalMeshSourcePayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
