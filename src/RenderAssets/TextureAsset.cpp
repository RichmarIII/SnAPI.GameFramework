#include "RenderAssets/TextureAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    TextureAsset,
    (TTypeBuilder<TextureAsset>(TextureAsset::kTypeName)
        .Base<IAsset>()
        .Field("Image", &TextureAsset::Image, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("ImportSettings", &TextureAsset::ImportSettings, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Hidden)
        .Field("Provenance", &TextureAsset::Provenance, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Advanced)
        .Constructor<>()
        .Register()));

Result TextureAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

TExpected<void> SerializeTextureSourcePayload(const TextureAsset& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<TextureAsset> DeserializeTextureSourcePayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<TextureAsset>(Bytes, Size, "Null source payload bytes");
}

} // namespace SnAPI::GameFramework
