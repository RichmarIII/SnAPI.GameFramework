#include "RenderAssets/MeshVertexStreamPayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MeshVertexStreamPayload,
    (TTypeBuilder<MeshVertexStreamPayload>(MeshVertexStreamPayload::kTypeName)
        .Field("Semantic", &MeshVertexStreamPayload::Semantic, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("SubIndex", &MeshVertexStreamPayload::SubIndex, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("Uri", &MeshVertexStreamPayload::Uri, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "Bytes",
            &MeshVertexStreamPayload::Bytes,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::Hidden | EFieldEditorFlagBits::HeavyData)
        .Field(
            "ElementCount",
            &MeshVertexStreamPayload::ElementCount,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "StrideBytes",
            &MeshVertexStreamPayload::StrideBytes,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field("Compress", &MeshVertexStreamPayload::Compress, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
