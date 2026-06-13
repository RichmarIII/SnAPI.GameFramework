#include "RenderAssets/StaticMeshPayload.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MeshStreamChunkRef,
    (TTypeBuilder<MeshStreamChunkRef>(MeshStreamChunkRef::kTypeName)
        .Field("Semantic", &MeshStreamChunkRef::Semantic, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("BulkIndex", &MeshStreamChunkRef::BulkIndex, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "ElementCount",
            &MeshStreamChunkRef::ElementCount,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "StrideBytes",
            &MeshStreamChunkRef::StrideBytes,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    StaticSubMeshPayload,
    (TTypeBuilder<StaticSubMeshPayload>(StaticSubMeshPayload::kTypeName)
        .Field("IndexOffset", &StaticSubMeshPayload::IndexOffset, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("IndexCount", &StaticSubMeshPayload::IndexCount, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "MaterialSlot",
            &StaticSubMeshPayload::MaterialSlot,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field("BoundsMin", &StaticSubMeshPayload::BoundsMin, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("BoundsMax", &StaticSubMeshPayload::BoundsMax, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    StaticMeshPayload,
    (TTypeBuilder<StaticMeshPayload>(StaticMeshPayload::kTypeName)
        .Field("Name", &StaticMeshPayload::Name, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("BoundsMin", &StaticMeshPayload::BoundsMin, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("BoundsMax", &StaticMeshPayload::BoundsMax, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("SubMeshes", &StaticMeshPayload::SubMeshes, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "MaterialInstances",
            &StaticMeshPayload::MaterialInstances,
            EFieldFlagBits::Serialized)
        .Field("Streams", &StaticMeshPayload::Streams, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
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
