#include "RenderAssets/MaterialInstanceAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MaterialScalarParamPayload,
    (TTypeBuilder<MaterialScalarParamPayload>(MaterialScalarParamPayload::kTypeName)
        .Field("Name", &MaterialScalarParamPayload::Name, EFieldFlagBits::Serialized)
        .Field("Value", &MaterialScalarParamPayload::Value, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    MaterialVectorParamPayload,
    (TTypeBuilder<MaterialVectorParamPayload>(MaterialVectorParamPayload::kTypeName)
        .Field("Name", &MaterialVectorParamPayload::Name, EFieldFlagBits::Serialized)
        .Field("Value", &MaterialVectorParamPayload::Value, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    MaterialTextureParamPayload,
    (TTypeBuilder<MaterialTextureParamPayload>(MaterialTextureParamPayload::kTypeName)
        .Field("SlotName", &MaterialTextureParamPayload::SlotName, EFieldFlagBits::Serialized)
        .Field("Texture", &MaterialTextureParamPayload::Texture, EFieldFlagBits::Serialized)
        .Field("SRGB", &MaterialTextureParamPayload::SRGB, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    MaterialInstanceAsset,
    (TTypeBuilder<MaterialInstanceAsset>(MaterialInstanceAsset::kTypeName)
        .Base<IAsset>()
        .Field("ParentMaterial", &MaterialInstanceAsset::ParentMaterial, EFieldFlagBits::Serialized)
        .Field("Scalars", &MaterialInstanceAsset::Scalars, EFieldFlagBits::Serialized)
        .Field("Vectors", &MaterialInstanceAsset::Vectors, EFieldFlagBits::Serialized)
        .Field("Textures", &MaterialInstanceAsset::Textures, EFieldFlagBits::Serialized)
        .Field("Provenance", &MaterialInstanceAsset::Provenance, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

Result MaterialInstanceAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

TExpected<void> SerializeMaterialInstancePayload(const MaterialInstanceAsset& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<MaterialInstanceAsset> DeserializeMaterialInstancePayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<MaterialInstanceAsset>(Bytes, Size, "Null payload bytes");
}

} // namespace SnAPI::GameFramework
