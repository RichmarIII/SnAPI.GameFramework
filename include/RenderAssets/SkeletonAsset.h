#pragma once

#include "IAsset.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"
#include "RenderAssets/SkeletonPayload.h"

namespace SnAPI::GameFramework
{

SnType()
struct SkeletonAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletonAsset";

    SnField(SnKey("Skeleton"))
    SkeletonPayload Skeleton{};
    SnField(SnKey("Provenance"))
    ImportedAssetProvenancePayload Provenance{};

    [[nodiscard]] std::string_view DisplayName() const override { return "Skeleton"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".skeleton"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] bool CanCreate() const override { return false; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindSkeleton(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadSkeleton(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindSkeleton(); }

    bool operator==(const SkeletonAsset& Other) const
    {
        return Skeleton.Name == Other.Skeleton.Name &&
               Skeleton.Bones == Other.Skeleton.Bones &&
               Provenance == Other.Provenance;
    }
};

} // namespace SnAPI::GameFramework
