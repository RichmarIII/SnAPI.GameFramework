#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "Expected.h"

namespace SnAPI::GameFramework
{

struct AssetRefPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.AssetRefPayload";

    std::string AssetName{};
    std::string AssetId{};

    bool operator==(const AssetRefPayload&) const = default;
};

enum class EMeshStreamSemantic : uint32_t
{
    Position = 0,
    Normal = 1,
    Tangent = 2,
    UV0 = 3,
    UV1 = 4,
    Color = 5,
    BoneIndices = 6,
    BoneWeights = 7,
    Index = 8,
};

struct MeshStreamChunkRef
{
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position;
    uint32_t BulkIndex = 0;
    uint32_t ElementCount = 0;
    uint32_t StrideBytes = 0;
};

struct StaticSubMeshPayload
{
    uint32_t IndexOffset = 0;
    uint32_t IndexCount = 0;
    uint32_t MaterialSlot = 0;
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f};
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f};
};

struct StaticMeshPayload
{
    std::string Name{};
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f};
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f};
    std::vector<StaticSubMeshPayload> SubMeshes{};
    std::vector<AssetRefPayload> MaterialInstances{};
    std::vector<MeshStreamChunkRef> Streams{};
};

struct SkeletalBonePayload
{
    std::string Name{};
    int32_t ParentIndex = -1;
    std::array<float, 16> BindPose{};
};

struct SkeletonPayload
{
    std::string Name{};
    std::vector<SkeletalBonePayload> Bones{};
};

struct AnimationKeyFramePayload
{
    float Time = 0.0f;
    std::array<float, 3> Translation{0.0f, 0.0f, 0.0f};
    std::array<float, 4> Rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> Scale{1.0f, 1.0f, 1.0f};
};

struct AnimationTrackPayload
{
    std::string BoneName{};
    std::vector<AnimationKeyFramePayload> KeyFrames{};
};

struct AnimationPayload
{
    std::string Name{};
    float DurationSeconds = 0.0f;
    float TicksPerSecond = 0.0f;
    std::vector<AnimationTrackPayload> Tracks{};
};

struct SkeletalMeshPayload
{
    StaticMeshPayload BaseMesh{};
    std::vector<SkeletalBonePayload> Bones{};
    AssetRefPayload Skeleton{};
    std::vector<AssetRefPayload> Animations{};
    uint32_t SkeletonAnimationBulkIndex = std::numeric_limits<uint32_t>::max();
};

struct MaterialPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialPayload";

    std::string ShaderModule{};
    std::string ShadingModel{};
    bool FeatureAlbedoMap = false;
    bool FeatureNormalMap = false;
    bool FeatureRoughnessMap = false;
    bool FeatureMetalnessMap = false;
    bool FeatureOcclusionMap = false;
    bool FeatureAlphaTest = false;
    bool FeatureAlphaBlend = false;
    bool FeatureDoubleSided = false;
    bool FeatureInstancing = false;

    bool operator==(const MaterialPayload&) const = default;
};

struct MaterialScalarParamPayload
{
    std::string Name{};
    float Value = 0.0f;

    bool operator==(const MaterialScalarParamPayload&) const = default;
};

struct MaterialVectorParamPayload
{
    std::string Name{};
    std::array<float, 4> Value{0.0f, 0.0f, 0.0f, 0.0f};

    bool operator==(const MaterialVectorParamPayload&) const = default;
};

struct MaterialTextureParamPayload
{
    std::string SlotName{};
    AssetRefPayload Texture{};
    bool SRGB = true;

    bool operator==(const MaterialTextureParamPayload&) const = default;
};

struct MaterialInstancePayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialInstancePayload";

    AssetRefPayload ParentMaterial{};
    std::vector<MaterialScalarParamPayload> Scalars{};
    std::vector<MaterialVectorParamPayload> Vectors{};
    std::vector<MaterialTextureParamPayload> Textures{};

    bool operator==(const MaterialInstancePayload&) const = default;
};

TExpected<void> SerializeStaticMeshPayload(const StaticMeshPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<StaticMeshPayload> DeserializeStaticMeshPayload(const uint8_t* Bytes, size_t Size);

TExpected<void> SerializeSkeletalMeshPayload(const SkeletalMeshPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<SkeletalMeshPayload> DeserializeSkeletalMeshPayload(const uint8_t* Bytes, size_t Size);

TExpected<void> SerializeSkeletonPayload(const SkeletonPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<SkeletonPayload> DeserializeSkeletonPayload(const uint8_t* Bytes, size_t Size);

TExpected<void> SerializeAnimationPayload(const AnimationPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<AnimationPayload> DeserializeAnimationPayload(const uint8_t* Bytes, size_t Size);

TExpected<void> SerializeMaterialPayload(const MaterialPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<MaterialPayload> DeserializeMaterialPayload(const uint8_t* Bytes, size_t Size);

TExpected<void> SerializeMaterialInstancePayload(const MaterialInstancePayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<MaterialInstancePayload> DeserializeMaterialInstancePayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
