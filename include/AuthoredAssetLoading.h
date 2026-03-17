#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "AuthoredAssetJson.h"
#include "Export.h"
#include "PayloadRegistry.h"
#include "TypedPayload.h"

namespace SnAPI::GameFramework
{

SNAPI_GAMEFRAMEWORK_API TExpected<std::filesystem::path> ResolveAuthoredAssetPath(std::string_view AssetName);
SNAPI_GAMEFRAMEWORK_API TExpected<std::string> LoadAuthoredAssetSourceText(std::string_view AssetName);
SNAPI_GAMEFRAMEWORK_API Result LoadAuthoredAssetFromPath(
    const TypeId& Type,
    const std::filesystem::path& Path,
    void* OutAsset,
    AuthoredAssetImportDiagnostics* OutDiagnostics = nullptr);
SNAPI_GAMEFRAMEWORK_API Result LoadAuthoredAssetByName(
    const TypeId& Type,
    std::string_view AssetName,
    void* OutAsset,
    AuthoredAssetImportDiagnostics* OutDiagnostics = nullptr);
SNAPI_GAMEFRAMEWORK_API TExpected<::SnAPI::AssetPipeline::TypedPayload> BuildAuthoredAssetSourcePayload(
    const TypeId& Type,
    const void* Asset,
    const ::SnAPI::AssetPipeline::PayloadRegistry& Registry);

template<typename TAsset>
[[nodiscard]] TExpected<std::unique_ptr<TAsset>> LoadAuthoredAssetByName(std::string_view AssetName)
{
    auto Asset = std::make_unique<TAsset>();
    auto LoadResult = LoadAuthoredAssetByName(StaticTypeId<TAsset>(), AssetName, Asset.get());
    if (!LoadResult)
    {
        return std::unexpected(LoadResult.error());
    }
    return Asset;
}

template<typename TAsset>
[[nodiscard]] TExpected<::SnAPI::AssetPipeline::TypedPayload> BuildAuthoredAssetSourcePayload(
    const TAsset& Asset,
    const ::SnAPI::AssetPipeline::PayloadRegistry& Registry)
{
    return BuildAuthoredAssetSourcePayload(StaticTypeId<TAsset>(), &Asset, Registry);
}

} // namespace SnAPI::GameFramework
