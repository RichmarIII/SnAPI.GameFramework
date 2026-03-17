#include "RenderAssets/MeshStreamSourcePayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MeshStreamSourcePayload,
    (TTypeBuilder<MeshStreamSourcePayload>(MeshStreamSourcePayload::kTypeName)
        .Field("Semantic", &MeshStreamSourcePayload::Semantic, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("SubIndex", &MeshStreamSourcePayload::SubIndex, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("Uri", &MeshStreamSourcePayload::Uri, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "Bytes",
            &MeshStreamSourcePayload::Bytes,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::Hidden | EFieldEditorFlagBits::HeavyData)
        .Field(
            "ElementCount",
            &MeshStreamSourcePayload::ElementCount,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "StrideBytes",
            &MeshStreamSourcePayload::StrideBytes,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field("Compress", &MeshStreamSourcePayload::Compress, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
