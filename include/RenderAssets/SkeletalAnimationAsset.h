#pragma once

#include "IAsset.h"
#include "RenderAssets/AnimationPayload.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"

namespace SnAPI::GameFramework
{

SnType()
struct SkeletalAnimationAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalAnimationAsset";

    SnField(SnKey("Animation"), SnReadOnly)
    AnimationPayload Animation{};
    SnField(SnKey("Provenance"), SnAdvanced)
    ImportedAssetProvenancePayload Provenance{};

    [[nodiscard]] std::string_view DisplayName() const override { return "Skeletal Animation"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".skeletalanim"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] bool CanCreate() const override { return false; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindAnimation(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadAnimation(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindAnimation(); }

    bool operator==(const SkeletalAnimationAsset& Other) const
    {
        return Animation.Name == Other.Animation.Name &&
               Animation.DurationSeconds == Other.Animation.DurationSeconds &&
               Animation.TicksPerSecond == Other.Animation.TicksPerSecond &&
               Animation.Tracks == Other.Animation.Tracks &&
               Provenance == Other.Provenance;
    }
};

} // namespace SnAPI::GameFramework
