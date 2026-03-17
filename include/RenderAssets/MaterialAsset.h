#pragma once

#include <string>
#include <vector>

#include "Expected.h"
#include "IAsset.h"
#include "ReflectionAnnotations.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"

namespace SnAPI::GameFramework
{

SnType()
struct MaterialAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialAsset";

    SnField(SnKey("ShaderModule"))
    std::string ShaderModule{};
    SnField(SnKey("ShadingModel"))
    std::string ShadingModel{};
    SnField(SnKey("FeatureAlbedoMap"))
    bool FeatureAlbedoMap = false;
    SnField(SnKey("FeatureNormalMap"))
    bool FeatureNormalMap = false;
    SnField(SnKey("FeatureRoughnessMap"))
    bool FeatureRoughnessMap = false;
    SnField(SnKey("FeatureMetalnessMap"))
    bool FeatureMetalnessMap = false;
    SnField(SnKey("FeatureOcclusionMap"))
    bool FeatureOcclusionMap = false;
    SnField(SnKey("FeatureAlphaTest"))
    bool FeatureAlphaTest = false;
    SnField(SnKey("FeatureAlphaBlend"))
    bool FeatureAlphaBlend = false;
    SnField(SnKey("FeatureDoubleSided"))
    bool FeatureDoubleSided = false;
    SnField(SnKey("FeatureInstancing"))
    bool FeatureInstancing = false;
    SnField(SnKey("Provenance"))
    ImportedAssetProvenancePayload Provenance{};

    bool operator==(const MaterialAsset&) const = default;

    [[nodiscard]] std::string_view DisplayName() const override { return "Material"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".material"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindMaterial(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadMaterial(); }
};

TExpected<void> SerializeMaterialPayload(const MaterialAsset& Payload, std::vector<uint8_t>& OutBytes);
TExpected<MaterialAsset> DeserializeMaterialPayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
