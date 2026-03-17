#include "AuthoredAssetLoading.h"

#include <fstream>

#include "IAsset.h"
#include "PathResolver.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] bool HasSchema(std::string_view Value)
{
    return Value.find("://") != std::string_view::npos;
}
} // namespace

TExpected<std::filesystem::path> ResolveAuthoredAssetPath(const std::string_view AssetName)
{
    if (AssetName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset name is empty"));
    }

    const std::string ResolveText = HasSchema(AssetName)
        ? std::string(AssetName)
        : std::string("asset://") + std::string(AssetName);
    return SPathResolver::Instance().Resolve(ResolveText);
}

TExpected<std::string> LoadAuthoredAssetSourceText(const std::string_view AssetName)
{
    auto PathResult = ResolveAuthoredAssetPath(AssetName);
    if (!PathResult)
    {
        return std::unexpected(PathResult.error());
    }

    std::ifstream File(*PathResult, std::ios::binary | std::ios::ate);
    if (!File.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Failed to open authored asset source file"));
    }

    const std::streamsize Size = File.tellg();
    std::string Text{};
    if (Size > 0)
    {
        Text.resize(static_cast<size_t>(Size));
        File.seekg(0, std::ios::beg);
        File.read(Text.data(), Size);
    }

    return Text;
}

Result LoadAuthoredAssetFromPath(const TypeId& Type,
                                 const std::filesystem::path& Path,
                                 void* OutAsset,
                                 AuthoredAssetImportDiagnostics* OutDiagnostics)
{
    if (Path.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset path is empty"));
    }

    std::ifstream File(Path, std::ios::binary | std::ios::ate);
    if (!File.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Failed to open authored asset source file"));
    }

    const std::streamsize Size = File.tellg();
    std::string Text{};
    if (Size > 0)
    {
        Text.resize(static_cast<size_t>(Size));
        File.seekg(0, std::ios::beg);
        File.read(Text.data(), Size);
    }

    return DeserializeAuthoredAssetFromJson(Type, Text, OutAsset, OutDiagnostics);
}

Result LoadAuthoredAssetByName(const TypeId& Type,
                               const std::string_view AssetName,
                               void* OutAsset,
                               AuthoredAssetImportDiagnostics* OutDiagnostics)
{
    auto PathResult = ResolveAuthoredAssetPath(AssetName);
    if (!PathResult)
    {
        return std::unexpected(PathResult.error());
    }
    return LoadAuthoredAssetFromPath(Type, *PathResult, OutAsset, OutDiagnostics);
}

TExpected<::SnAPI::AssetPipeline::TypedPayload> BuildAuthoredAssetSourcePayload(
    const TypeId& Type,
    const void* Asset,
    const ::SnAPI::AssetPipeline::PayloadRegistry& Registry)
{
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset source pointer is null"));
    }

    (void)TypeAutoRegistry::Instance().Ensure(Type);
    const void* AssetPtr = TypeRegistry::Instance().Cast(Type, StaticTypeId<IAsset>(), Asset);
    if (!AssetPtr)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Reflected type is not an authored asset"));
    }

    const auto* AuthoredAsset = static_cast<const IAsset*>(AssetPtr);
    const auto PayloadType = AuthoredAsset->SourcePayloadType();
    const auto* Serializer = Registry.Find(PayloadType);
    if (!Serializer)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound,
                                         "Payload serializer is not registered for authored source asset type"));
    }

    std::vector<std::uint8_t> Bytes{};
    Serializer->SerializeToBytes(Asset, Bytes);
    return ::SnAPI::AssetPipeline::TypedPayload(PayloadType, Serializer->GetSchemaVersion(), std::move(Bytes));
}

} // namespace SnAPI::GameFramework
