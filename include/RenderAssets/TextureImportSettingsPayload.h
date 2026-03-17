#pragma once

#include <cstdint>
#include <string>

#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

SnType(SnDisplayName("Texture Import Settings"))
struct TextureImportSettingsPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TextureImportSettingsPayload";

    SnField(SnKey("Target"))
    std::string Target{"BCn"};
    SnField(SnKey("Format"))
    std::string Format{"Auto"};
    SnField(SnKey("Quality"))
    float Quality = 0.6f;
    SnField(SnKey("ForceSrgb"))
    bool ForceSrgb = false;
    SnField(SnKey("ForceLinear"))
    bool ForceLinear = false;
    SnField(SnKey("ForceNormalMap"))
    bool ForceNormalMap = false;
    SnField(SnKey("MaxMips"))
    uint32_t MaxMips = 0;

    bool operator==(const TextureImportSettingsPayload&) const = default;
};

SNAPI_DEFINE_TYPE_NAME(TextureImportSettingsPayload, "SnAPI::GameFramework::TextureImportSettingsPayload")

} // namespace SnAPI::GameFramework
