#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

#include "AuthoredAssetJson.h"
#include "Export.h"
#include "PayloadRegistry.h"
#include "TypedPayload.h"

namespace SnAPI::GameFramework
{

struct AuthoredAssetIdentity
{
    ::SnAPI::AssetPipeline::AssetId AssetId{};
    std::string LogicalName{};
};

SNAPI_GAMEFRAMEWORK_API TExpected<std::filesystem::path> ResolveAuthoredAssetPath(std::string_view AssetName);
SNAPI_GAMEFRAMEWORK_API TExpected<std::string> LoadAuthoredAssetSourceText(std::string_view AssetName);
SNAPI_GAMEFRAMEWORK_API TExpected<AuthoredAssetIdentity> LoadAuthoredAssetIdentityFromText(
    const TypeId& Type,
    std::string_view Text,
    std::string_view FallbackLogicalName = {},
    AuthoredAssetImportDiagnostics* OutDiagnostics = nullptr);
SNAPI_GAMEFRAMEWORK_API TExpected<AuthoredAssetIdentity> LoadAuthoredAssetIdentityFromPath(
    const TypeId& Type,
    const std::filesystem::path& Path,
    std::string_view FallbackLogicalName = {},
    AuthoredAssetImportDiagnostics* OutDiagnostics = nullptr);
SNAPI_GAMEFRAMEWORK_API void UpsertAuthoredAssetIdentityIndexEntry(
    const std::filesystem::path& AssetRoot,
    const std::filesystem::path& Path,
    const ::SnAPI::AssetPipeline::AssetId& AssetId);
SNAPI_GAMEFRAMEWORK_API void RemoveAuthoredAssetIdentityIndexEntry(
    const std::filesystem::path& AssetRoot,
    const std::filesystem::path& Path);
SNAPI_GAMEFRAMEWORK_API void PruneAuthoredAssetIdentityIndex(
    const std::filesystem::path& AssetRoot,
    const std::unordered_set<std::string>& LiveLogicalNames);
SNAPI_GAMEFRAMEWORK_API Result SaveAuthoredAssetIdentityIndex(const std::filesystem::path& AssetRoot);
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
