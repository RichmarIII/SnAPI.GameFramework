#include "RenderAssets/SkeletonAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    SkeletonAsset,
    (TTypeBuilder<SkeletonAsset>(SkeletonAsset::kTypeName)
        .Base<IAsset>()
        .Field("Skeleton", &SkeletonAsset::Skeleton, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("Provenance", &SkeletonAsset::Provenance, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Advanced)
        .Constructor<>()
        .Register()));

Result SkeletonAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetJson(*this, Output);
}

} // namespace SnAPI::GameFramework
