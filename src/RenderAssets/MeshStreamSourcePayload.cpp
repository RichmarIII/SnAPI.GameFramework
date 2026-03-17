#include "RenderAssets/MeshStreamSourcePayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MeshStreamSourcePayload,
    (TTypeBuilder<MeshStreamSourcePayload>(MeshStreamSourcePayload::kTypeName)
        .Field("Semantic", &MeshStreamSourcePayload::Semantic, EFieldFlagBits::Serialized)
        .Field("SubIndex", &MeshStreamSourcePayload::SubIndex, EFieldFlagBits::Serialized)
        .Field("Uri", &MeshStreamSourcePayload::Uri, EFieldFlagBits::Serialized)
        .Field("Bytes", &MeshStreamSourcePayload::Bytes, EFieldFlagBits::Serialized)
        .Field("ElementCount", &MeshStreamSourcePayload::ElementCount, EFieldFlagBits::Serialized)
        .Field("StrideBytes", &MeshStreamSourcePayload::StrideBytes, EFieldFlagBits::Serialized)
        .Field("Compress", &MeshStreamSourcePayload::Compress, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
