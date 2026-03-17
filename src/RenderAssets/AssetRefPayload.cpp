#include "RenderAssets/AssetRefPayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    AssetRefPayload,
    (TTypeBuilder<AssetRefPayload>(AssetRefPayload::kTypeName)
        .Field("AssetName", &AssetRefPayload::AssetName, EFieldFlagBits::Serialized)
        .Field("AssetId", &AssetRefPayload::AssetId, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
