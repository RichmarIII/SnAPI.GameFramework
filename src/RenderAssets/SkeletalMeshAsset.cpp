#include "RenderAssets/SkeletalMeshAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    SkeletalMeshAsset,
    (TTypeBuilder<SkeletalMeshAsset>(SkeletalMeshAsset::kTypeName)
        .Base<IAsset>()
        .Field("BaseMesh", &SkeletalMeshAsset::BaseMesh, EFieldFlagBits::Serialized)
        .Field("Bones", &SkeletalMeshAsset::Bones, EFieldFlagBits::Serialized)
        .Field("Skeleton", &SkeletalMeshAsset::Skeleton, EFieldFlagBits::Serialized)
        .Field("Animations", &SkeletalMeshAsset::Animations, EFieldFlagBits::Serialized)
        .Field("SkeletonAnimationUri", &SkeletalMeshAsset::SkeletonAnimationUri, EFieldFlagBits::Serialized)
        .Field("SkeletonAnimationBytes", &SkeletalMeshAsset::SkeletonAnimationBytes, EFieldFlagBits::Serialized)
        .Field("SkeletonAnimationSubIndex", &SkeletalMeshAsset::SkeletonAnimationSubIndex, EFieldFlagBits::Serialized)
        .Field("CompressSkeletonAnimation", &SkeletalMeshAsset::CompressSkeletonAnimation, EFieldFlagBits::Serialized)
        .Field("Provenance", &SkeletalMeshAsset::Provenance, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

Result SkeletalMeshAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

TExpected<void> SerializeSkeletalMeshSourcePayload(const SkeletalMeshAsset& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<SkeletalMeshAsset> DeserializeSkeletalMeshSourcePayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<SkeletalMeshAsset>(Bytes, Size, "Null source payload bytes");
}

} // namespace SnAPI::GameFramework
