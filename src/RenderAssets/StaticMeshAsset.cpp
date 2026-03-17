#include "RenderAssets/StaticMeshAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    StaticMeshAsset,
    (TTypeBuilder<StaticMeshAsset>(StaticMeshAsset::kTypeName)
        .Base<IAsset>()
        .Field("Mesh", &StaticMeshAsset::Mesh, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "Streams",
            &StaticMeshAsset::Streams,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly | EFieldEditorFlagBits::Advanced)
        .Field("ImportSettings", &StaticMeshAsset::ImportSettings, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Hidden)
        .Field("Provenance", &StaticMeshAsset::Provenance, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Advanced)
        .Constructor<>()
        .Register()));

Result StaticMeshAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

TExpected<void> SerializeStaticMeshSourcePayload(const StaticMeshAsset& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<StaticMeshAsset> DeserializeStaticMeshSourcePayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<StaticMeshAsset>(Bytes, Size, "Null source payload bytes");
}

} // namespace SnAPI::GameFramework
