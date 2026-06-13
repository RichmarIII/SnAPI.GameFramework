#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <ostream>

#include <nlohmann/json_fwd.hpp>

#include "Expected.h"
#include "Export.h"
#include "StaticTypeId.h"

namespace SnAPI::GameFramework
{

namespace Conduit
{
struct SerializedValue;
}

using AuthoredAssetImportDiagnostics = std::vector<std::string>;

SNAPI_GAMEFRAMEWORK_API TExpected<nlohmann::json> SerializeSerializedValueToJsonValue(
    const Conduit::SerializedValue& Value);

SNAPI_GAMEFRAMEWORK_API TExpected<std::string> SerializeAuthoredAssetToJson(
    const TypeId& Type,
    const void* Asset);

SNAPI_GAMEFRAMEWORK_API Result SaveAuthoredAssetToJsonStream(
    const TypeId& Type,
    const void* Asset,
    std::ostream& Output);

SNAPI_GAMEFRAMEWORK_API Result DeserializeAuthoredAssetFromJson(
    const TypeId& Type,
    std::string_view Text,
    void* OutAsset);

SNAPI_GAMEFRAMEWORK_API Result DeserializeAuthoredAssetFromJson(
    const TypeId& Type,
    std::string_view Text,
    void* OutAsset,
    AuthoredAssetImportDiagnostics* OutDiagnostics);

template<typename TAsset>
[[nodiscard]] TExpected<std::string> SerializeAuthoredAssetToJson(const TAsset& Asset)
{
    return SerializeAuthoredAssetToJson(StaticTypeId<TAsset>(), &Asset);
}

template<typename TAsset>
[[nodiscard]] Result SaveAuthoredAssetToJsonStream(const TAsset& Asset, std::ostream& Output)
{
    return SaveAuthoredAssetToJsonStream(StaticTypeId<TAsset>(), &Asset, Output);
}

template<typename TAsset>
[[nodiscard]] Result DeserializeAuthoredAssetFromJson(const std::string_view Text, TAsset& OutAsset)
{
    return DeserializeAuthoredAssetFromJson(StaticTypeId<TAsset>(), Text, &OutAsset);
}

template<typename TAsset>
[[nodiscard]] Result DeserializeAuthoredAssetFromJson(const std::string_view Text,
                                                      TAsset& OutAsset,
                                                      AuthoredAssetImportDiagnostics& OutDiagnostics)
{
    return DeserializeAuthoredAssetFromJson(StaticTypeId<TAsset>(), Text, &OutAsset, &OutDiagnostics);
}

} // namespace SnAPI::GameFramework
