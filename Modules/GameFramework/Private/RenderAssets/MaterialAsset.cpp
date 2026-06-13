#include "RenderAssets/MaterialAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MaterialAsset,
    (TTypeBuilder<MaterialAsset>(MaterialAsset::kTypeName)
        .Base<IAsset>()
        .Field("ShaderModule", &MaterialAsset::ShaderModule, EFieldFlagBits::Serialized)
        .Field("ShadingModel", &MaterialAsset::ShadingModel, EFieldFlagBits::Serialized)
        .Field("FeatureAlbedoMap", &MaterialAsset::FeatureAlbedoMap, EFieldFlagBits::Serialized)
        .Field("FeatureNormalMap", &MaterialAsset::FeatureNormalMap, EFieldFlagBits::Serialized)
        .Field("FeatureRoughnessMap", &MaterialAsset::FeatureRoughnessMap, EFieldFlagBits::Serialized)
        .Field("FeatureMetalnessMap", &MaterialAsset::FeatureMetalnessMap, EFieldFlagBits::Serialized)
        .Field("FeatureOcclusionMap", &MaterialAsset::FeatureOcclusionMap, EFieldFlagBits::Serialized)
        .Field("FeatureAlphaTest", &MaterialAsset::FeatureAlphaTest, EFieldFlagBits::Serialized)
        .Field("FeatureAlphaBlend", &MaterialAsset::FeatureAlphaBlend, EFieldFlagBits::Serialized)
        .Field("FeatureDoubleSided", &MaterialAsset::FeatureDoubleSided, EFieldFlagBits::Serialized)
        .Field("FeatureInstancing", &MaterialAsset::FeatureInstancing, EFieldFlagBits::Serialized)
        .Field("Provenance", &MaterialAsset::Provenance, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

Result MaterialAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

TExpected<void> SerializeMaterialPayload(const MaterialAsset& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<MaterialAsset> DeserializeMaterialPayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<MaterialAsset>(Bytes, Size, "Null payload bytes");
}

} // namespace SnAPI::GameFramework
