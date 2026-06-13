#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BuiltinTypes.h"
#include "Expected.h"
#include "IAsset.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"
#include "RenderAssets/SkeletalAnimationAsset.h"
#include "RenderAssets/SkeletonAsset.h"
#include "RenderAssets/SkeletonPayload.h"
#include "RenderAssets/StaticMeshAsset.h"

namespace SnAPI::GameFramework
{

SnType()
struct SkeletalMeshAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshAsset";

    SnField(SnKey("BaseMesh"))
    StaticMeshAsset BaseMesh{};
    SnField(SnKey("Bones"), SnReadOnly)
    std::vector<SkeletalBonePayload> Bones{};
    SnField(SnKey("Skeleton"), SnReadOnly)
    SkeletonAssetRef Skeleton{};
    SnField(SnKey("Animations"), SnReadOnly)
    std::vector<SkeletalAnimationAssetRef> Animations{};
    SnField(SnKey("SkeletonAnimationUri"), SnReadOnly, SnAdvanced)
    std::string SkeletonAnimationUri{};
    SnField(SnKey("SkeletonAnimationBytes"), SnHidden, SnHeavyData)
    std::vector<uint8_t> SkeletonAnimationBytes{};
    SnField(SnKey("SkeletonAnimationSubIndex"), SnReadOnly, SnAdvanced)
    uint32_t SkeletonAnimationSubIndex = 0;
    SnField(SnKey("CompressSkeletonAnimation"), SnReadOnly, SnAdvanced)
    bool CompressSkeletonAnimation = true;
    SnField(SnKey("Provenance"), SnAdvanced)
    ImportedAssetProvenancePayload Provenance{};

    [[nodiscard]] std::string_view DisplayName() const override { return "Skeletal Mesh"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".skeletalmesh"; }
    [[nodiscard]] std::string_view Category() const override { return "Rendering"; }
    [[nodiscard]] bool CanCreate() const override { return false; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindSkeletalMesh(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadSkeletalMeshSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindSkeletalMesh(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override { return PayloadSkeletalMesh(); }

    bool operator==(const SkeletalMeshAsset& Other) const
    {
        return BaseMesh == Other.BaseMesh
            && Bones == Other.Bones
            && Skeleton == Other.Skeleton
            && Animations == Other.Animations
            && SkeletonAnimationUri == Other.SkeletonAnimationUri
            && SkeletonAnimationBytes == Other.SkeletonAnimationBytes
            && SkeletonAnimationSubIndex == Other.SkeletonAnimationSubIndex
            && CompressSkeletonAnimation == Other.CompressSkeletonAnimation
            && Provenance == Other.Provenance;
    }
};

TExpected<void> SerializeSkeletalMeshSourcePayload(const SkeletalMeshAsset& Payload, std::vector<uint8_t>& OutBytes);
TExpected<SkeletalMeshAsset> DeserializeSkeletalMeshSourcePayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
