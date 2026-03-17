#include "RenderAssets/SkeletalAnimationAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    SkeletalAnimationAsset,
    (TTypeBuilder<SkeletalAnimationAsset>(SkeletalAnimationAsset::kTypeName)
        .Base<IAsset>()
        .Field("Animation", &SkeletalAnimationAsset::Animation, EFieldFlagBits::Serialized)
        .Field("Provenance", &SkeletalAnimationAsset::Provenance, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

Result SkeletalAnimationAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

} // namespace SnAPI::GameFramework
