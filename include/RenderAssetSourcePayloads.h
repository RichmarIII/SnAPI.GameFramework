#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Expected.h"
#include "RenderAssetPayloads.h"

namespace SnAPI::GameFramework
{

struct MeshStreamSourcePayload
{
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position;
    uint32_t SubIndex = 0;
    std::string Uri{};
    std::vector<uint8_t> Bytes{};
    uint32_t ElementCount = 0;
    uint32_t StrideBytes = 0;
    bool Compress = true;
};

struct MeshImportSettingsPayload
{
    bool GenerateNormals = true;
    bool GenerateTangents = true;
    bool FlipUVs = false;
    bool OptimizeMeshes = true;
    bool ForceSkeletal = false;
    bool ForceStatic = false;
    uint32_t MaxBonesPerVertex = 4;
};

struct StaticMeshSourcePayload
{
    StaticMeshPayload Mesh{};
    std::vector<MeshStreamSourcePayload> Streams{};
    MeshImportSettingsPayload ImportSettings{};
};

struct SkeletalMeshSourcePayload
{
    StaticMeshSourcePayload BaseMesh{};
    std::vector<SkeletalBonePayload> Bones{};
    AssetRefPayload Skeleton{};
    std::vector<AssetRefPayload> Animations{};
    std::string SkeletonAnimationUri{};
    std::vector<uint8_t> SkeletonAnimationBytes{};
    uint32_t SkeletonAnimationSubIndex = 0;
    bool CompressSkeletonAnimation = true;
};

TExpected<void> SerializeStaticMeshSourcePayload(const StaticMeshSourcePayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<StaticMeshSourcePayload> DeserializeStaticMeshSourcePayload(const uint8_t* Bytes, size_t Size);

TExpected<void> SerializeSkeletalMeshSourcePayload(const SkeletalMeshSourcePayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<SkeletalMeshSourcePayload> DeserializeSkeletalMeshSourcePayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
