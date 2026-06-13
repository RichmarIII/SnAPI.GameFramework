#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Expected.h"
#include "RenderAssets/MaterialInstanceAsset.h"
#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

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

SnType()
struct MeshStreamChunkRef
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::MeshStreamChunkRef";

    SnField(SnKey("Semantic"), SnReadOnly)
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position;
    SnField(SnKey("BulkIndex"), SnReadOnly)
    uint32_t BulkIndex = 0;
    SnField(SnKey("ElementCount"), SnReadOnly)
    uint32_t ElementCount = 0;
    SnField(SnKey("StrideBytes"), SnReadOnly)
    uint32_t StrideBytes = 0;

    bool operator==(const MeshStreamChunkRef&) const = default;
};

SnType()
struct StaticSubMeshPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticSubMeshPayload";

    SnField(SnKey("IndexOffset"), SnReadOnly)
    uint32_t IndexOffset = 0;
    SnField(SnKey("IndexCount"), SnReadOnly)
    uint32_t IndexCount = 0;
    SnField(SnKey("MaterialSlot"), SnReadOnly)
    uint32_t MaterialSlot = 0;
    SnField(SnKey("BoundsMin"), SnReadOnly)
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f};
    SnField(SnKey("BoundsMax"), SnReadOnly)
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f};

    bool operator==(const StaticSubMeshPayload&) const = default;
};

SnType()
struct StaticMeshPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshPayload";

    SnField(SnKey("Name"), SnReadOnly)
    std::string Name{};
    SnField(SnKey("BoundsMin"), SnReadOnly)
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f};
    SnField(SnKey("BoundsMax"), SnReadOnly)
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f};
    SnField(SnKey("SubMeshes"), SnReadOnly)
    std::vector<StaticSubMeshPayload> SubMeshes{};
    SnField(SnKey("MaterialInstances"))
    std::vector<MaterialInstanceAssetRef> MaterialInstances{};
    SnField(SnKey("Streams"), SnReadOnly)
    std::vector<MeshStreamChunkRef> Streams{};

    bool operator==(const StaticMeshPayload&) const = default;
};

TExpected<void> SerializeStaticMeshPayload(const StaticMeshPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<StaticMeshPayload> DeserializeStaticMeshPayload(const uint8_t* Bytes, size_t Size);

SNAPI_DEFINE_TYPE_NAME(std::vector<MeshStreamChunkRef>, "std::vector<SnAPI::GameFramework::MeshStreamChunkRef>")
SNAPI_DEFINE_TYPE_NAME(std::vector<StaticSubMeshPayload>, "std::vector<SnAPI::GameFramework::StaticSubMeshPayload>")

} // namespace SnAPI::GameFramework
