#include "AuthoredAssetLoading.h"

#include <fstream>

#include "AssetPipelineIds.h"
#include "IAsset.h"
#include "PathResolver.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_map>

namespace SnAPI::GameFramework
{
namespace
{
constexpr std::string_view kSourceAssetIdentityIndexDirectoryName = ".snapi_editor";
constexpr std::string_view kSourceAssetIdentityIndexFileName = "source_asset_identity_index.json";
constexpr uint32_t kSourceAssetIdentityIndexVersion = 1u;

struct SourceAssetIdentityIndexState
{
    std::filesystem::path AssetRoot{};
    std::filesystem::path IndexPath{};
    bool Loaded = false;
    bool Dirty = false;
    std::unordered_map<std::string, ::SnAPI::AssetPipeline::AssetId> AssetIdsByLogicalName{};
};

std::mutex GSourceAssetIdentityIndexMutex{};
std::unordered_map<std::string, SourceAssetIdentityIndexState> GSourceAssetIdentityIndices{};

[[nodiscard]] bool HasSchema(std::string_view Value)
{
    return Value.find("://") != std::string_view::npos;
}

[[nodiscard]] std::string NormalizeAssetLogicalName(std::string_view RawName)
{
    std::string Name(RawName);
    std::replace(Name.begin(), Name.end(), '\\', '/');

    while (!Name.empty() && std::isspace(static_cast<unsigned char>(Name.front())) != 0)
    {
        Name.erase(Name.begin());
    }
    while (!Name.empty() && std::isspace(static_cast<unsigned char>(Name.back())) != 0)
    {
        Name.pop_back();
    }

    while (Name.find("//") != std::string::npos)
    {
        Name.replace(Name.find("//"), 2u, "/");
    }
    while (Name.rfind("./", 0) == 0)
    {
        Name.erase(0, 2u);
    }
    while (!Name.empty() && Name.front() == '/')
    {
        Name.erase(Name.begin());
    }

    if (Name == ".")
    {
        Name.clear();
    }

    return Name;
}

[[nodiscard]] std::filesystem::path NormalizeAssetRootPath(std::filesystem::path AssetRoot)
{
    if (AssetRoot.empty())
    {
        return {};
    }

    std::error_code Error{};
    auto Canonical = std::filesystem::weakly_canonical(AssetRoot, Error);
    if (!Error)
    {
        return Canonical;
    }

    Error.clear();
    auto Absolute = std::filesystem::absolute(AssetRoot, Error);
    if (!Error)
    {
        return Absolute.lexically_normal();
    }

    return AssetRoot.lexically_normal();
}

[[nodiscard]] std::string BuildSourceLogicalName(const std::filesystem::path& AssetRoot,
                                                 const std::filesystem::path& SourceFile)
{
    std::error_code Error{};
    std::filesystem::path RelativePath = std::filesystem::relative(SourceFile, AssetRoot, Error);
    if (Error)
    {
        RelativePath = SourceFile.filename();
    }
    return NormalizeAssetLogicalName(RelativePath.generic_string());
}

[[nodiscard]] std::filesystem::path ResolveSourceAssetIdentityIndexPath(const std::filesystem::path& AssetRoot)
{
    const std::filesystem::path NormalizedRoot = NormalizeAssetRootPath(AssetRoot);
    if (NormalizedRoot.empty())
    {
        return {};
    }

    return NormalizedRoot /
        std::filesystem::path(std::string(kSourceAssetIdentityIndexDirectoryName)) /
        std::filesystem::path(std::string(kSourceAssetIdentityIndexFileName));
}

[[nodiscard]] std::filesystem::path InferAssetRootFromPathAndLogicalName(const std::filesystem::path& Path,
                                                                         const std::string_view LogicalName)
{
    const std::string NormalizedLogicalName = NormalizeAssetLogicalName(LogicalName);
    if (NormalizedLogicalName.empty())
    {
        return {};
    }

    std::filesystem::path AssetRoot = Path.lexically_normal();
    for (const auto& Component : std::filesystem::path(NormalizedLogicalName))
    {
        (void)Component;
        AssetRoot = AssetRoot.parent_path();
        if (AssetRoot.empty())
        {
            return {};
        }
    }

    const std::filesystem::path NormalizedRoot = NormalizeAssetRootPath(AssetRoot);
    if (NormalizedRoot.empty())
    {
        return {};
    }

    return BuildSourceLogicalName(NormalizedRoot, Path) == NormalizedLogicalName ? NormalizedRoot : std::filesystem::path{};
}

[[nodiscard]] const nlohmann::json* ResolveAuthoredAssetRoot(const nlohmann::json& Document)
{
    if (Document.is_object())
    {
        const auto AssetIt = Document.find("Asset");
        if (AssetIt != Document.end() && AssetIt->is_object())
        {
            return &(*AssetIt);
        }
    }

    return &Document;
}

[[nodiscard]] SourceAssetIdentityIndexState& EnsureSourceAssetIdentityIndexLoadedLocked(const std::filesystem::path& AssetRoot)
{
    const std::filesystem::path NormalizedRoot = NormalizeAssetRootPath(AssetRoot);
    const std::string RootKey = NormalizedRoot.generic_string();
    SourceAssetIdentityIndexState& State = GSourceAssetIdentityIndices[RootKey];
    if (State.Loaded)
    {
        return State;
    }

    State.AssetRoot = NormalizedRoot;
    State.IndexPath = ResolveSourceAssetIdentityIndexPath(NormalizedRoot);
    State.Loaded = true;
    State.Dirty = false;
    State.AssetIdsByLogicalName.clear();

    if (State.IndexPath.empty())
    {
        return State;
    }

    std::error_code ExistsError{};
    if (!std::filesystem::exists(State.IndexPath, ExistsError) || ExistsError)
    {
        return State;
    }

    std::ifstream Input(State.IndexPath, std::ios::binary);
    if (!Input.is_open())
    {
        return State;
    }

    const auto Document = nlohmann::json::parse(Input, nullptr, false);
    if (Document.is_discarded() || !Document.is_object())
    {
        return State;
    }

    const auto VersionIt = Document.find("Version");
    if (VersionIt == Document.end() || !VersionIt->is_number_unsigned() ||
        VersionIt->get<uint32_t>() != kSourceAssetIdentityIndexVersion)
    {
        return State;
    }

    const auto EntriesIt = Document.find("Entries");
    if (EntriesIt == Document.end() || !EntriesIt->is_array())
    {
        return State;
    }

    for (const auto& Entry : *EntriesIt)
    {
        if (!Entry.is_object())
        {
            continue;
        }

        const auto LogicalNameIt = Entry.find("LogicalName");
        const auto AssetIdIt = Entry.find("AssetId");
        if (LogicalNameIt == Entry.end() || !LogicalNameIt->is_string() ||
            AssetIdIt == Entry.end() || !AssetIdIt->is_string())
        {
            continue;
        }

        const std::string LogicalName = NormalizeAssetLogicalName(LogicalNameIt->get<std::string>());
        const ::SnAPI::AssetPipeline::AssetId ParsedAssetId =
            ::SnAPI::AssetPipeline::AssetId::FromString(AssetIdIt->get<std::string>());
        if (LogicalName.empty() || ParsedAssetId.IsNull())
        {
            continue;
        }

        State.AssetIdsByLogicalName[LogicalName] = ParsedAssetId;
    }

    return State;
}

[[nodiscard]] TExpected<AuthoredAssetIdentity> LoadAuthoredAssetIdentityFromTextUncached(
    const TypeId& Type,
    const std::string_view Text,
    const std::string_view FallbackLogicalName,
    AuthoredAssetImportDiagnostics* OutDiagnostics)
{
    (void)Type;
    (void)OutDiagnostics;

    const auto Document = nlohmann::json::parse(Text.begin(), Text.end(), nullptr, false);
    if (Document.is_discarded())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset JSON parse failed"));
    }

    const nlohmann::json* AssetRoot = ResolveAuthoredAssetRoot(Document);
    if (!AssetRoot || !AssetRoot->is_object())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset JSON root must be an object"));
    }

    AuthoredAssetIdentity Identity{};
    const std::string NormalizedFallbackLogicalName = NormalizeAssetLogicalName(FallbackLogicalName);
    if (!NormalizedFallbackLogicalName.empty())
    {
        Identity.LogicalName = NormalizedFallbackLogicalName;
    }
    else if (const auto LogicalNameIt = AssetRoot->find("LogicalName");
             LogicalNameIt != AssetRoot->end() && LogicalNameIt->is_string())
    {
        Identity.LogicalName = NormalizeAssetLogicalName(LogicalNameIt->get<std::string>());
    }

    if (const auto AssetIdIt = AssetRoot->find("AssetId");
        AssetIdIt != AssetRoot->end() && AssetIdIt->is_string())
    {
        Identity.AssetId = ::SnAPI::AssetPipeline::AssetId::FromString(AssetIdIt->get<std::string>());
    }
    if (Identity.AssetId.IsNull() && !Identity.LogicalName.empty())
    {
        Identity.AssetId = SourceAssetIdFromLogicalName(Identity.LogicalName);
    }

    return Identity;
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

TExpected<AuthoredAssetIdentity> LoadAuthoredAssetIdentityFromText(const TypeId& Type,
                                                                   const std::string_view Text,
                                                                   const std::string_view FallbackLogicalName,
                                                                   AuthoredAssetImportDiagnostics* OutDiagnostics)
{
    return LoadAuthoredAssetIdentityFromTextUncached(Type, Text, FallbackLogicalName, OutDiagnostics);
}

TExpected<AuthoredAssetIdentity> LoadAuthoredAssetIdentityFromPath(const TypeId& Type,
                                                                   const std::filesystem::path& Path,
                                                                   const std::string_view FallbackLogicalName,
                                                                   AuthoredAssetImportDiagnostics* OutDiagnostics)
{
    if (Path.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset path is empty"));
    }

    const std::string NormalizedFallbackLogicalName = NormalizeAssetLogicalName(FallbackLogicalName);
    const std::filesystem::path AssetRoot = InferAssetRootFromPathAndLogicalName(Path, NormalizedFallbackLogicalName);
    if (!AssetRoot.empty() && !NormalizedFallbackLogicalName.empty())
    {
        std::scoped_lock Lock(GSourceAssetIdentityIndexMutex);
        SourceAssetIdentityIndexState& State = EnsureSourceAssetIdentityIndexLoadedLocked(AssetRoot);
        if (const auto It = State.AssetIdsByLogicalName.find(NormalizedFallbackLogicalName);
            It != State.AssetIdsByLogicalName.end())
        {
            return AuthoredAssetIdentity{
                .AssetId = It->second,
                .LogicalName = NormalizedFallbackLogicalName,
            };
        }
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

    auto IdentityResult = LoadAuthoredAssetIdentityFromTextUncached(Type, Text, NormalizedFallbackLogicalName, OutDiagnostics);
    if (!IdentityResult)
    {
        return IdentityResult;
    }

    if (!AssetRoot.empty() &&
        !NormalizedFallbackLogicalName.empty() &&
        !IdentityResult->AssetId.IsNull())
    {
        std::scoped_lock Lock(GSourceAssetIdentityIndexMutex);
        SourceAssetIdentityIndexState& State = EnsureSourceAssetIdentityIndexLoadedLocked(AssetRoot);
        const auto [It, Inserted] = State.AssetIdsByLogicalName.emplace(NormalizedFallbackLogicalName, IdentityResult->AssetId);
        if (Inserted || It->second != IdentityResult->AssetId)
        {
            It->second = IdentityResult->AssetId;
            State.Dirty = true;
        }
    }

    return IdentityResult;
}

void UpsertAuthoredAssetIdentityIndexEntry(const std::filesystem::path& AssetRoot,
                                           const std::filesystem::path& Path,
                                           const ::SnAPI::AssetPipeline::AssetId& AssetId)
{
    if (AssetRoot.empty() || Path.empty() || AssetId.IsNull())
    {
        return;
    }

    const std::filesystem::path NormalizedRoot = NormalizeAssetRootPath(AssetRoot);
    const std::string LogicalName = BuildSourceLogicalName(NormalizedRoot, Path);
    if (LogicalName.empty())
    {
        return;
    }

    std::scoped_lock Lock(GSourceAssetIdentityIndexMutex);
    SourceAssetIdentityIndexState& State = EnsureSourceAssetIdentityIndexLoadedLocked(NormalizedRoot);
    const auto [It, Inserted] = State.AssetIdsByLogicalName.emplace(LogicalName, AssetId);
    if (Inserted || It->second != AssetId)
    {
        It->second = AssetId;
        State.Dirty = true;
    }
}

void RemoveAuthoredAssetIdentityIndexEntry(const std::filesystem::path& AssetRoot,
                                           const std::filesystem::path& Path)
{
    if (AssetRoot.empty() || Path.empty())
    {
        return;
    }

    const std::filesystem::path NormalizedRoot = NormalizeAssetRootPath(AssetRoot);
    const std::string LogicalName = BuildSourceLogicalName(NormalizedRoot, Path);
    if (LogicalName.empty())
    {
        return;
    }

    std::scoped_lock Lock(GSourceAssetIdentityIndexMutex);
    SourceAssetIdentityIndexState& State = EnsureSourceAssetIdentityIndexLoadedLocked(NormalizedRoot);
    if (State.AssetIdsByLogicalName.erase(LogicalName) > 0)
    {
        State.Dirty = true;
    }
}

void PruneAuthoredAssetIdentityIndex(const std::filesystem::path& AssetRoot,
                                     const std::unordered_set<std::string>& LiveLogicalNames)
{
    if (AssetRoot.empty())
    {
        return;
    }

    const std::filesystem::path NormalizedRoot = NormalizeAssetRootPath(AssetRoot);
    std::scoped_lock Lock(GSourceAssetIdentityIndexMutex);
    SourceAssetIdentityIndexState& State = EnsureSourceAssetIdentityIndexLoadedLocked(NormalizedRoot);

    for (auto It = State.AssetIdsByLogicalName.begin(); It != State.AssetIdsByLogicalName.end();)
    {
        if (LiveLogicalNames.contains(It->first))
        {
            ++It;
            continue;
        }

        It = State.AssetIdsByLogicalName.erase(It);
        State.Dirty = true;
    }
}

Result SaveAuthoredAssetIdentityIndex(const std::filesystem::path& AssetRoot)
{
    if (AssetRoot.empty())
    {
        return Ok();
    }

    std::scoped_lock Lock(GSourceAssetIdentityIndexMutex);
    SourceAssetIdentityIndexState& State = EnsureSourceAssetIdentityIndexLoadedLocked(AssetRoot);
    if (!State.Dirty || State.IndexPath.empty())
    {
        return Ok();
    }

    nlohmann::json Document = nlohmann::json::object();
    Document["Version"] = kSourceAssetIdentityIndexVersion;
    Document["Entries"] = nlohmann::json::array();

    std::vector<std::pair<std::string, std::string>> Entries{};
    Entries.reserve(State.AssetIdsByLogicalName.size());
    for (const auto& [LogicalName, AssetId] : State.AssetIdsByLogicalName)
    {
        if (LogicalName.empty() || AssetId.IsNull())
        {
            continue;
        }
        Entries.emplace_back(LogicalName, AssetId.ToString());
    }

    std::sort(Entries.begin(), Entries.end(), [](const auto& Left, const auto& Right) {
        return Left.first < Right.first;
    });

    for (const auto& [LogicalName, AssetIdText] : Entries)
    {
        Document["Entries"].push_back(
            nlohmann::json{{"LogicalName", LogicalName}, {"AssetId", AssetIdText}});
    }

    std::error_code Error{};
    const std::filesystem::path ParentPath = State.IndexPath.parent_path();
    if (!ParentPath.empty())
    {
        std::filesystem::create_directories(ParentPath, Error);
        if (Error)
        {
            return std::unexpected(MakeError(
                EErrorCode::InternalError,
                "Failed to create source asset identity index directory '" + ParentPath.string() + "': " + Error.message()));
        }
    }

    const std::filesystem::path TempPath = State.IndexPath.string() + ".tmp";
    std::ofstream Output(TempPath, std::ios::binary | std::ios::trunc);
    if (!Output.is_open())
    {
        return std::unexpected(MakeError(
            EErrorCode::InternalError,
            "Failed to open source asset identity index temp file: " + TempPath.string()));
    }

    Output << Document.dump(2);
    Output.flush();
    if (!Output.good())
    {
        return std::unexpected(MakeError(
            EErrorCode::InternalError,
            "Failed to write source asset identity index temp file: " + TempPath.string()));
    }

    Output.close();
    if (Output.fail())
    {
        return std::unexpected(MakeError(
            EErrorCode::InternalError,
            "Failed to finalize source asset identity index temp file: " + TempPath.string()));
    }

    Error.clear();
    std::filesystem::rename(TempPath, State.IndexPath, Error);
    if (Error)
    {
        Error.clear();
        std::filesystem::remove(State.IndexPath, Error);
        Error.clear();
        std::filesystem::rename(TempPath, State.IndexPath, Error);
    }

    if (Error)
    {
        return std::unexpected(MakeError(
            EErrorCode::InternalError,
            "Failed to commit source asset identity index '" + State.IndexPath.string() + "': " + Error.message()));
    }

    State.Dirty = false;
    return Ok();
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
