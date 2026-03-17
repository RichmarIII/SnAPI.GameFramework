#include "RenderAssets/TextureImportSettingsPayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    TextureImportSettingsPayload,
    (TTypeBuilder<TextureImportSettingsPayload>(TextureImportSettingsPayload::kTypeName)
        .Field("Target", &TextureImportSettingsPayload::Target, EFieldFlagBits::Serialized)
        .Field("Format", &TextureImportSettingsPayload::Format, EFieldFlagBits::Serialized)
        .Field("Quality", &TextureImportSettingsPayload::Quality, EFieldFlagBits::Serialized)
        .Field("ForceSrgb", &TextureImportSettingsPayload::ForceSrgb, EFieldFlagBits::Serialized)
        .Field("ForceLinear", &TextureImportSettingsPayload::ForceLinear, EFieldFlagBits::Serialized)
        .Field("ForceNormalMap", &TextureImportSettingsPayload::ForceNormalMap, EFieldFlagBits::Serialized)
        .Field("MaxMips", &TextureImportSettingsPayload::MaxMips, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
