#pragma once

#include <vector>

#include "Expected.h"
#include "IAsset.h"
#include "RenderAssetImportSettings.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"
#include "RenderAssets/MeshStreamSourcePayload.h"
#include "RenderAssets/StaticMeshPayload.h"

namespace SnAPI::GameFramework
{

SnType()
struct StaticMeshAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshAsset";

    SnField(SnKey("Mesh"))
    StaticMeshPayload Mesh{};
    SnField(SnKey("Streams"), SnReadOnly, SnAdvanced)
    std::vector<MeshStreamSourcePayload> Streams{};
    SnField(SnKey("ImportSettings"), SnHidden)
    AssimpImporterSettings ImportSettings{};
    SnField(SnKey("Provenance"), SnAdvanced)
    ImportedAssetProvenancePayload Provenance{};

    [[nodiscard]] std::string_view DisplayName() const override { return "Static Mesh"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".staticmesh"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] bool CanCreate() const override { return false; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindStaticMesh(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadStaticMeshSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindStaticMesh(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override { return PayloadStaticMesh(); }

    bool operator==(const StaticMeshAsset& Other) const
    {
        return Mesh == Other.Mesh &&
               Streams == Other.Streams &&
               ImportSettings == Other.ImportSettings &&
               Provenance == Other.Provenance;
    }
};

TExpected<void> SerializeStaticMeshSourcePayload(const StaticMeshAsset& Payload, std::vector<uint8_t>& OutBytes);
TExpected<StaticMeshAsset> DeserializeStaticMeshSourcePayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
