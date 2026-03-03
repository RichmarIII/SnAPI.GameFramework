#pragma once

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include "AssetRef.h"
#include "RenderAssetPayloads.h"

namespace SnAPI::GameFramework
{

struct TextureAssetRuntime
{
};

struct MaterialAssetRuntime
{
    std::string ShaderModule{};
    std::string ShadingModel{};
};

struct MaterialInstanceAssetRuntime
{
    TAssetRef<MaterialAssetRuntime> ParentMaterial{};
    std::vector<MaterialScalarParamPayload> Scalars{};
    std::vector<MaterialVectorParamPayload> Vectors{};
    std::vector<std::string> TextureSlots{};
    std::vector<TAssetRef<TextureAssetRuntime>> Textures{};
};

struct StaticMeshAssetRuntime
{
    ::SnAPI::AssetPipeline::AssetId SourceAssetId{};
    std::string Name{};
    std::vector<StaticSubMeshPayload> SubMeshes{};
    std::vector<MeshStreamChunkRef> Streams{};
    std::vector<TAssetRef<MaterialInstanceAssetRuntime>> MaterialInstances{};
    std::function<std::expected<std::vector<uint8_t>, std::string>(uint32_t)> LoadBulk{};
};

struct SkeletonAssetRuntime
{
    std::string Name{};
    std::vector<SkeletalBonePayload> Bones{};
};

struct AnimationAssetRuntime
{
    std::string Name{};
    float DurationSeconds = 0.0f;
    float TicksPerSecond = 0.0f;
    std::vector<AnimationTrackPayload> Tracks{};
};

struct SkeletalMeshAssetRuntime : public StaticMeshAssetRuntime
{
    std::vector<SkeletalBonePayload> Bones{};
    TAssetRef<SkeletonAssetRuntime> Skeleton{};
    std::vector<TAssetRef<AnimationAssetRuntime>> Animations{};
    uint32_t SkeletonAnimationBulkIndex = std::numeric_limits<uint32_t>::max();
};

} // namespace SnAPI::GameFramework
