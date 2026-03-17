#include "RenderAssets/StaticMeshPayload.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MeshStreamChunkRef,
    (TTypeBuilder<MeshStreamChunkRef>(MeshStreamChunkRef::kTypeName)
        .Field("Semantic", &MeshStreamChunkRef::Semantic, EFieldFlagBits::Serialized)
        .Field("BulkIndex", &MeshStreamChunkRef::BulkIndex, EFieldFlagBits::Serialized)
        .Field("ElementCount", &MeshStreamChunkRef::ElementCount, EFieldFlagBits::Serialized)
        .Field("StrideBytes", &MeshStreamChunkRef::StrideBytes, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    StaticSubMeshPayload,
    (TTypeBuilder<StaticSubMeshPayload>(StaticSubMeshPayload::kTypeName)
        .Field("IndexOffset", &StaticSubMeshPayload::IndexOffset, EFieldFlagBits::Serialized)
        .Field("IndexCount", &StaticSubMeshPayload::IndexCount, EFieldFlagBits::Serialized)
        .Field("MaterialSlot", &StaticSubMeshPayload::MaterialSlot, EFieldFlagBits::Serialized)
        .Field("BoundsMin", &StaticSubMeshPayload::BoundsMin, EFieldFlagBits::Serialized)
        .Field("BoundsMax", &StaticSubMeshPayload::BoundsMax, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    StaticMeshPayload,
    (TTypeBuilder<StaticMeshPayload>(StaticMeshPayload::kTypeName)
        .Field("Name", &StaticMeshPayload::Name, EFieldFlagBits::Serialized)
        .Field("BoundsMin", &StaticMeshPayload::BoundsMin, EFieldFlagBits::Serialized)
        .Field("BoundsMax", &StaticMeshPayload::BoundsMax, EFieldFlagBits::Serialized)
        .Field("SubMeshes", &StaticMeshPayload::SubMeshes, EFieldFlagBits::Serialized)
        .Field("MaterialInstances", &StaticMeshPayload::MaterialInstances, EFieldFlagBits::Serialized)
        .Field("Streams", &StaticMeshPayload::Streams, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

TExpected<void> SerializeStaticMeshPayload(const StaticMeshPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<StaticMeshPayload> DeserializeStaticMeshPayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<StaticMeshPayload>(Bytes, Size, "Null payload bytes");
}

} // namespace SnAPI::GameFramework
