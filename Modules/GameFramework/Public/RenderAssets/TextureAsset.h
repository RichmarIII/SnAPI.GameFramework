#pragma once

#include <vector>

#include "Expected.h"
#include "IAsset.h"
#include "RenderAssetImportSettings.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"
#include "RenderAssets/TextureSourceImagePayload.h"

#include <TextureCompressorIds.h>

namespace TextureCompressorPlugin
{
struct ImageIntermediate;
}

namespace SnAPI::GameFramework
{

SnType()
struct TextureAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TextureAsset";

    SnField(SnKey("Image"), SnReadOnly)
    TextureSourceImagePayload Image{};
    SnField(SnKey("ImportSettings"), SnHidden)
    TextureImporterSettings ImportSettings{};
    SnField(SnKey("Provenance"), SnAdvanced)
    ImportedAssetProvenancePayload Provenance{};

    [[nodiscard]] std::string_view DisplayName() const override { return "Texture"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".texture"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] bool CanCreate() const override { return false; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override
    {
        return TextureCompressorPlugin::AssetKind_CompressedTexture;
    }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadTextureSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override
    {
        return TextureCompressorPlugin::AssetKind_CompressedTexture;
    }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override
    {
        return TextureCompressorPlugin::Payload_CompressorCookedInfo;
    }

    bool operator==(const TextureAsset& Other) const
    {
        return Image == Other.Image &&
               ImportSettings == Other.ImportSettings &&
               Provenance == Other.Provenance;
    }
};

TExpected<void> SerializeTextureSourcePayload(const TextureAsset& Payload, std::vector<uint8_t>& OutBytes);
TExpected<TextureAsset> DeserializeTextureSourcePayload(const uint8_t* Bytes, size_t Size);
TExpected<void> PopulateTextureSourceImageFromIntermediate(
    TextureSourceImagePayload& OutImage,
    const TextureCompressorPlugin::ImageIntermediate& Intermediate,
    const std::vector<std::uint8_t>* PreferredEncodedBytes = nullptr);
TExpected<void> EnsureTextureSourceImageEncoded(TextureSourceImagePayload& Image);
TExpected<void> DecodeTextureSourceImageToIntermediate(const TextureSourceImagePayload& Image,
                                                       TextureCompressorPlugin::ImageIntermediate& OutIntermediate);
TExpected<void> DecodeTextureSourceImageToRgba(const TextureSourceImagePayload& Image,
                                               std::vector<std::uint8_t>& OutPixels);

} // namespace SnAPI::GameFramework
