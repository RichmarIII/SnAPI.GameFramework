#pragma once

#include <string>
#include <vector>

#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

SnType()
struct AssetRefPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.AssetRefPayload";

    SnField(SnKey("AssetName"))
    std::string AssetName{};
    SnField(SnKey("AssetId"))
    std::string AssetId{};

    bool operator==(const AssetRefPayload&) const = default;
};

SNAPI_DEFINE_TYPE_NAME(std::vector<AssetRefPayload>, "std::vector<SnAPI::GameFramework::AssetRefPayload>")

} // namespace SnAPI::GameFramework
