#pragma once

#include <array>
#include <string>
#include <vector>

#include "Expected.h"
#include "IAsset.h"
#include "ReflectionAnnotations.h"
#include "RenderAssets/AssetRefPayload.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

using Float4Value = std::array<float, 4>;

SnType()
struct MaterialScalarParamPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialScalarParamPayload";

    SnField(SnKey("Name"))
    std::string Name{};
    SnField(SnKey("Value"))
    float Value = 0.0f;

    bool operator==(const MaterialScalarParamPayload&) const = default;
};

SnType()
struct MaterialVectorParamPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialVectorParamPayload";

    SnField(SnKey("Name"))
    std::string Name{};
    SnField(SnKey("Value"))
    Float4Value Value{0.0f, 0.0f, 0.0f, 0.0f};

    bool operator==(const MaterialVectorParamPayload&) const = default;
};

SnType()
struct MaterialTextureParamPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialTextureParamPayload";

    SnField(SnKey("SlotName"))
    std::string SlotName{};
    SnField(SnKey("Texture"))
    AssetRefPayload Texture{};
    SnField(SnKey("SRGB"))
    bool SRGB = true;

    bool operator==(const MaterialTextureParamPayload&) const = default;
};

SnType()
struct MaterialInstanceAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.MaterialInstanceAsset";

    SnField(SnKey("ParentMaterial"))
    AssetRefPayload ParentMaterial{};
    SnField(SnKey("Scalars"))
    std::vector<MaterialScalarParamPayload> Scalars{};
    SnField(SnKey("Vectors"))
    std::vector<MaterialVectorParamPayload> Vectors{};
    SnField(SnKey("Textures"))
    std::vector<MaterialTextureParamPayload> Textures{};
    SnField(SnKey("Provenance"))
    ImportedAssetProvenancePayload Provenance{};

    bool operator==(const MaterialInstanceAsset&) const = default;

    [[nodiscard]] std::string_view DisplayName() const override { return "Material Instance"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".materialinstance"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindMaterialInstance(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadMaterialInstance(); }
};

TExpected<void> SerializeMaterialInstancePayload(const MaterialInstanceAsset& Payload, std::vector<uint8_t>& OutBytes);
TExpected<MaterialInstanceAsset> DeserializeMaterialInstancePayload(const uint8_t* Bytes, size_t Size);

using MaterialInstanceAssetRef = TAssetRef<MaterialInstanceAsset, void>;
SNAPI_DEFINE_TYPE_NAME(MaterialInstanceAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::MaterialInstanceAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<MaterialInstanceAssetRef>, "std::vector<SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::MaterialInstanceAsset>>")

SNAPI_DEFINE_TYPE_NAME(Float4Value, "std::array<float,4>")
SNAPI_DEFINE_TYPE_NAME(std::vector<MaterialScalarParamPayload>, "std::vector<SnAPI::GameFramework::MaterialScalarParamPayload>")
SNAPI_DEFINE_TYPE_NAME(std::vector<MaterialVectorParamPayload>, "std::vector<SnAPI::GameFramework::MaterialVectorParamPayload>")
SNAPI_DEFINE_TYPE_NAME(std::vector<MaterialTextureParamPayload>, "std::vector<SnAPI::GameFramework::MaterialTextureParamPayload>")

} // namespace SnAPI::GameFramework
