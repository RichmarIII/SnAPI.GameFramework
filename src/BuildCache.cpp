#include "BuildCache.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

namespace SnAPI::GameFramework
{
    namespace
    {

        using Json = nlohmann::ordered_json;

        /**
         * @brief Trim leading and trailing ASCII whitespace from one string copy.
         * @param Text Source text.
         * @return Trimmed copy.
         */
        [[nodiscard]] std::string TrimCopy(const std::string_view Text)
        {
            std::size_t Begin = 0u;
            std::size_t End = Text.size();
            while (Begin < End && std::isspace(static_cast<unsigned char>(Text[Begin])) != 0)
            {
                ++Begin;
            }
            while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1u])) != 0)
            {
                --End;
            }
            return std::string(Text.substr(Begin, End - Begin));
        }

        /**
         * @brief Normalize one filesystem path for cache manifests and comparisons.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
        }

        /**
         * @brief Hash one byte span into a 64-bit FNV-1a accumulator.
         * @param Data Byte span to hash.
         * @param Size Byte count in `Data`.
         * @param Seed Existing hash state.
         * @return Updated FNV-1a hash state.
         */
        [[nodiscard]] std::uint64_t HashBytes64(const void* Data, const std::size_t Size, std::uint64_t Seed)
        {
            constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
            constexpr std::uint64_t kFnvPrime = 1099511628211ull;

            const auto* Bytes = static_cast<const unsigned char*>(Data);
            std::uint64_t Hash = Seed == 0u ? kFnvOffset : Seed;
            for (std::size_t Index = 0; Index < Size; ++Index)
            {
                Hash ^= static_cast<std::uint64_t>(Bytes[Index]);
                Hash *= kFnvPrime;
            }
            return Hash;
        }

        /**
         * @brief Hash one UTF-8 string value into a 64-bit FNV-1a accumulator.
         * @param Text Text to hash.
         * @param Seed Existing hash state.
         * @return Updated FNV-1a hash state.
         */
        [[nodiscard]] std::uint64_t HashString64(const std::string_view Text, const std::uint64_t Seed)
        {
            return HashBytes64(Text.data(), Text.size(), Seed);
        }

        /**
         * @brief Hash one 64-bit scalar value into a 64-bit FNV-1a accumulator.
         * @param Value Scalar value to hash.
         * @param Seed Existing hash state.
         * @return Updated FNV-1a hash state.
         */
        [[nodiscard]] std::uint64_t HashUInt64(const std::uint64_t Value, const std::uint64_t Seed)
        {
            return HashBytes64(&Value, sizeof(Value), Seed);
        }

        /**
         * @brief Hash one filesystem entry's metadata into a 64-bit accumulator.
         * @param Path Entry path to hash.
         * @param Seed Existing hash state.
         * @return Updated metadata fingerprint.
         */
        [[nodiscard]] std::uint64_t HashFilesystemMetadata(const std::filesystem::path& Path, std::uint64_t Seed)
        {
            const std::string Normalized = NormalizePathString(Path);
            Seed = HashString64(Normalized, Seed);

            std::error_code Error{};
            const std::filesystem::file_status Status = std::filesystem::symlink_status(Path, Error);
            if (Error)
            {
                return HashString64(std::string("status-error:") + Error.message(), Seed);
            }

            Seed = HashUInt64(static_cast<std::uint64_t>(Status.type()), Seed);

            Error.clear();
            const auto LastWriteTime = std::filesystem::last_write_time(Path, Error);
            if (Error)
            {
                Seed = HashString64(std::string("mtime-error:") + Error.message(), Seed);
            }
            else
            {
                const auto TimeCount = static_cast<std::uint64_t>(LastWriteTime.time_since_epoch().count());
                Seed = HashUInt64(TimeCount, Seed);
            }

            Error.clear();
            if (std::filesystem::is_regular_file(Status))
            {
                const auto FileSize = std::filesystem::file_size(Path, Error);
                if (Error)
                {
                    Seed = HashString64(std::string("size-error:") + Error.message(), Seed);
                }
                else
                {
                    Seed = HashUInt64(FileSize, Seed);
                }
            }

            return Seed;
        }

        /**
         * @brief Hash one file or directory input path into a 64-bit accumulator.
         * @param Path Filesystem path to fingerprint.
         * @param Seed Existing hash state.
         * @return Updated path fingerprint.
         */
        [[nodiscard]] std::uint64_t HashPathInput(const std::filesystem::path& Path, std::uint64_t Seed)
        {
            std::error_code Error{};
            if (!std::filesystem::exists(Path, Error) || Error)
            {
                Seed = HashString64("missing-path", Seed);
                return HashString64(NormalizePathString(Path), Seed);
            }

            Seed = HashFilesystemMetadata(Path, Seed);

            Error.clear();
            if (!std::filesystem::is_directory(Path, Error) || Error)
            {
                return Seed;
            }

            std::vector<std::filesystem::path> Entries{};
            for (std::filesystem::recursive_directory_iterator Iterator(
                     Path, std::filesystem::directory_options::skip_permission_denied, Error),
                 End;
                 Iterator != End;
                 Iterator.increment(Error))
            {
                if (Error)
                {
                    Seed = HashString64(std::string("dir-walk-error:") + Error.message(), Seed);
                    Error.clear();
                    continue;
                }

                Entries.push_back(Iterator->path().lexically_normal());
            }

            std::ranges::sort(Entries, [](const std::filesystem::path& Left, const std::filesystem::path& Right) {
                return NormalizePathString(Left) < NormalizePathString(Right);
            });

            for (const std::filesystem::path& EntryPath : Entries)
            {
                Seed = HashFilesystemMetadata(EntryPath, Seed);
            }

            return Seed;
        }

        /**
         * @brief Build the effective cache key for one node using its authored cache key plus current inputs.
         * @param Node Planned node to fingerprint.
         * @return Effective cache key string.
         */
        [[nodiscard]] std::string BuildEffectiveCacheKey(const BuildGraphNode& Node)
        {
            const std::string BaseKey = TrimCopy(Node.CacheKey);
            std::uint64_t Fingerprint = HashString64(BaseKey, 0u);
            Fingerprint = HashUInt64(static_cast<std::uint64_t>(Node.Inputs.size()), Fingerprint);

            for (const std::string& Input : Node.Inputs)
            {
                const std::string TrimmedInput = TrimCopy(Input);
                if (TrimmedInput.empty())
                {
                    Fingerprint = HashString64("<empty-input>", Fingerprint);
                    continue;
                }

                const std::filesystem::path InputPath(TrimmedInput);
                std::error_code Error{};
                if (std::filesystem::exists(InputPath, Error) && !Error)
                {
                    Fingerprint = HashPathInput(InputPath, Fingerprint);
                }
                else
                {
                    Fingerprint = HashString64(TrimmedInput, Fingerprint);
                }
            }

            std::ostringstream Stream{};
            Stream << BaseKey << ":v2:" << std::hex << std::setw(16) << std::setfill('0') << Fingerprint;
            return Stream.str();
        }

        /**
         * @brief Create one directory tree when it does not already exist.
         * @param Directory Directory path to create.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result EnsureDirectory(const std::filesystem::path& Directory)
        {
            if (Directory.empty())
            {
                return Ok();
            }

            std::error_code Error{};
            std::filesystem::create_directories(Directory, Error);
            if (Error)
            {
                return std::unexpected(MakeError(
                    EErrorCode::InternalError,
                    "Failed to create directory '" + Directory.string() + "': " + Error.message()));
            }
            return Ok();
        }

        /**
         * @brief Write one UTF-8 text file and create parent directories first.
         * @param FilePath Target file path.
         * @param Text UTF-8 payload to write.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result WriteTextFile(const std::filesystem::path& FilePath, std::string_view Text)
        {
            if (Result DirectoryResult = EnsureDirectory(FilePath.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::ofstream Stream(FilePath, std::ios::binary | std::ios::trunc);
            if (!Stream.is_open())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to open cache file for writing: " + FilePath.string()));
            }

            Stream.write(Text.data(), static_cast<std::streamsize>(Text.size()));
            if (!Stream.good())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to write cache file: " + FilePath.string()));
            }
            return Ok();
        }

        /**
         * @brief Read one UTF-8 text file from disk.
         * @param FilePath File path to read.
         * @return File contents or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::string> ReadTextFile(const std::filesystem::path& FilePath)
        {
            std::ifstream Stream(FilePath, std::ios::binary);
            if (!Stream.is_open())
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Failed to open cache file: " + FilePath.string()));
            }

            std::ostringstream Output{};
            Output << Stream.rdbuf();
            if (!Stream.good() && !Stream.eof())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to read cache file: " + FilePath.string()));
            }
            return Output.str();
        }

        /**
         * @brief Format the current UTC wall-clock time as ISO-8601 text.
         * @return UTC timestamp string.
         */
        [[nodiscard]] std::string MakeUtcTimestamp()
        {
            const auto Now = std::chrono::system_clock::now();
            const std::time_t TimeValue = std::chrono::system_clock::to_time_t(Now);

            std::tm UtcTime{};
#if defined(_WIN32)
            gmtime_s(&UtcTime, &TimeValue);
#else
            gmtime_r(&TimeValue, &UtcTime);
#endif

            std::ostringstream Stream{};
            Stream << std::put_time(&UtcTime, "%Y-%m-%dT%H:%M:%SZ");
            return Stream.str();
        }

        /**
         * @brief Convert one build node type into canonical text.
         * @param Type Node type to stringify.
         * @return Canonical node-type text.
         */
        [[nodiscard]] std::string ToString(const EBuildNodeType Type)
        {
            switch (Type)
            {
            case EBuildNodeType::LoadProject:
                return "LoadProject";
            case EBuildNodeType::ValidateResolvedRequest:
                return "ValidateResolvedRequest";
            case EBuildNodeType::ResolveExecutionEnvironment:
                return "ResolveExecutionEnvironment";
            case EBuildNodeType::ResolveModuleSet:
                return "ResolveModuleSet";
            case EBuildNodeType::ResolveAssetSelection:
                return "ResolveAssetSelection";
            case EBuildNodeType::GenerateProjectBuildFiles:
                return "GenerateProjectBuildFiles";
            case EBuildNodeType::ConfigureCMake:
                return "ConfigureCMake";
            case EBuildNodeType::BuildCode:
                return "BuildCode";
            case EBuildNodeType::EnumerateAssets:
                return "EnumerateAssets";
            case EBuildNodeType::CookAssets:
                return "CookAssets";
            case EBuildNodeType::WriteCookManifest:
                return "WriteCookManifest";
            case EBuildNodeType::WriteSnpak:
                return "WriteSnpak";
            case EBuildNodeType::CreateStageTree:
                return "CreateStageTree";
            case EBuildNodeType::StageBinaries:
                return "StageBinaries";
            case EBuildNodeType::StageAssets:
                return "StageAssets";
            case EBuildNodeType::StageConfigs:
                return "StageConfigs";
            case EBuildNodeType::WritePackageManifest:
                return "WritePackageManifest";
            case EBuildNodeType::WriteBuildReport:
                return "WriteBuildReport";
            }

            return "LoadProject";
        }

        /**
         * @brief Parse one build node type from canonical text.
         * @param Text Canonical node-type text.
         * @return Parsed node type or `std::nullopt`.
         */
        [[nodiscard]] std::optional<EBuildNodeType> ParseNodeType(const std::string_view Text)
        {
            if (Text == "LoadProject")
            {
                return EBuildNodeType::LoadProject;
            }
            if (Text == "ValidateResolvedRequest")
            {
                return EBuildNodeType::ValidateResolvedRequest;
            }
            if (Text == "ResolveExecutionEnvironment")
            {
                return EBuildNodeType::ResolveExecutionEnvironment;
            }
            if (Text == "ResolveModuleSet")
            {
                return EBuildNodeType::ResolveModuleSet;
            }
            if (Text == "ResolveAssetSelection")
            {
                return EBuildNodeType::ResolveAssetSelection;
            }
            if (Text == "GenerateProjectBuildFiles")
            {
                return EBuildNodeType::GenerateProjectBuildFiles;
            }
            if (Text == "ConfigureCMake")
            {
                return EBuildNodeType::ConfigureCMake;
            }
            if (Text == "BuildCode")
            {
                return EBuildNodeType::BuildCode;
            }
            if (Text == "EnumerateAssets")
            {
                return EBuildNodeType::EnumerateAssets;
            }
            if (Text == "CookAssets")
            {
                return EBuildNodeType::CookAssets;
            }
            if (Text == "WriteCookManifest")
            {
                return EBuildNodeType::WriteCookManifest;
            }
            if (Text == "WriteSnpak")
            {
                return EBuildNodeType::WriteSnpak;
            }
            if (Text == "CreateStageTree")
            {
                return EBuildNodeType::CreateStageTree;
            }
            if (Text == "StageBinaries")
            {
                return EBuildNodeType::StageBinaries;
            }
            if (Text == "StageAssets")
            {
                return EBuildNodeType::StageAssets;
            }
            if (Text == "StageConfigs")
            {
                return EBuildNodeType::StageConfigs;
            }
            if (Text == "WritePackageManifest")
            {
                return EBuildNodeType::WritePackageManifest;
            }
            if (Text == "WriteBuildReport")
            {
                return EBuildNodeType::WriteBuildReport;
            }
            return std::nullopt;
        }

        /**
         * @brief Convert one cache artifact kind into canonical text.
         * @param Kind Artifact kind to stringify.
         * @return Canonical artifact-kind text.
         */
        [[nodiscard]] std::string ToString(const EBuildCacheArtifactKind Kind)
        {
            return Kind == EBuildCacheArtifactKind::Directory ? "Directory" : "File";
        }

        /**
         * @brief Parse one cache artifact kind from canonical text.
         * @param Text Canonical artifact-kind text.
         * @return Parsed artifact kind or `std::nullopt`.
         */
        [[nodiscard]] std::optional<EBuildCacheArtifactKind> ParseArtifactKind(const std::string_view Text)
        {
            if (Text == "File")
            {
                return EBuildCacheArtifactKind::File;
            }
            if (Text == "Directory")
            {
                return EBuildCacheArtifactKind::Directory;
            }
            return std::nullopt;
        }

        /**
         * @brief Convert one build configuration enum into canonical text.
         * @param Configuration Build configuration to stringify.
         * @return Canonical configuration name.
         */
        [[nodiscard]] std::string ToString(const EBuildConfiguration Configuration)
        {
            switch (Configuration)
            {
            case EBuildConfiguration::Debug:
                return "Debug";
            case EBuildConfiguration::Development:
                return "Development";
            case EBuildConfiguration::Test:
                return "Test";
            case EBuildConfiguration::Shipping:
                return "Shipping";
            }

            return "Development";
        }

        /**
         * @brief Build the JSON manifest path for one cacheable node.
         * @param Request Frozen request that owns the cache root.
         * @param Node Planned node that owns the entry.
         * @return Canonical cache entry file path.
         */
        [[nodiscard]] std::filesystem::path CacheEntryPath(const ResolvedBuildRequest& Request, const BuildGraphNode& Node)
        {
            return (BuildCacheService::CacheRootDirectory(Request) /
                    (ToString(Node.Type) + "_" + Request.RequestHash + ".json"))
                .lexically_normal();
        }

        /**
         * @brief Remove one destination path when it already exists.
         * @param Path Filesystem path to clear.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result RemovePathIfExists(const std::filesystem::path& Path)
        {
            std::error_code Error{};
            if (!std::filesystem::exists(Path, Error) || Error)
            {
                return Ok();
            }

            std::filesystem::remove_all(Path, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to clear cache destination: " + Error.message()));
            }
            return Ok();
        }

        /**
         * @brief Copy one directory tree into a destination path, replacing stale contents first.
         * @param SourceDirectory Source directory tree.
         * @param DestinationDirectory Destination directory tree.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result CopyDirectoryTree(const std::filesystem::path& SourceDirectory,
                                               const std::filesystem::path& DestinationDirectory)
        {
            if (SourceDirectory.lexically_normal() == DestinationDirectory.lexically_normal())
            {
                return Ok();
            }

            if (Result RemoveResult = RemovePathIfExists(DestinationDirectory); !RemoveResult)
            {
                return RemoveResult;
            }
            if (Result DirectoryResult = EnsureDirectory(DestinationDirectory.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::error_code Error{};
            std::filesystem::copy(SourceDirectory, DestinationDirectory,
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::overwrite_existing,
                                  Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to restore cached directory: " + Error.message()));
            }
            return Ok();
        }

        /**
         * @brief Copy one regular file into a destination path, replacing any stale file first.
         * @param SourceFile Source regular file path.
         * @param DestinationFile Destination regular file path.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result CopyFile(const std::filesystem::path& SourceFile, const std::filesystem::path& DestinationFile)
        {
            if (SourceFile.lexically_normal() == DestinationFile.lexically_normal())
            {
                return Ok();
            }

            if (Result DirectoryResult = EnsureDirectory(DestinationFile.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::error_code Error{};
            std::filesystem::copy_file(SourceFile, DestinationFile, std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to restore cached file: " + Error.message()));
            }
            return Ok();
        }

        /**
         * @brief Serialize one cache entry into canonical JSON text.
         * @param Entry Cache entry to serialize.
         * @return Canonical JSON text.
         */
        [[nodiscard]] std::string SerializeEntry(const BuildCacheEntry& Entry)
        {
            Json Root = Json::object({
                {"CacheKey", Entry.CacheKey},
                {"RequestHash", Entry.RequestHash},
                {"NodeType", ToString(Entry.NodeType)},
                {"StoredAtUtc", Entry.StoredAtUtc},
                {"Artifacts", Json::array()},
            });

            for (const BuildCacheArtifact& Artifact : Entry.Artifacts)
            {
                Root["Artifacts"].push_back(Json::object({
                    {"Path", NormalizePathString(Artifact.Path)},
                    {"Kind", ToString(Artifact.Kind)},
                }));
            }

            return Root.dump(2) + "\n";
        }

        /**
         * @brief Parse one cache entry from JSON text.
         * @param Text JSON text to parse.
         * @return Parsed cache entry or `std::nullopt` when parsing fails.
         */
        [[nodiscard]] std::optional<BuildCacheEntry> ParseEntry(const std::string_view Text)
        {
            Json Root = Json::parse(Text, nullptr, false);
            if (Root.is_discarded() || !Root.is_object())
            {
                return std::nullopt;
            }

            const auto CacheKeyIt = Root.find("CacheKey");
            const auto RequestHashIt = Root.find("RequestHash");
            const auto NodeTypeIt = Root.find("NodeType");
            const auto StoredAtIt = Root.find("StoredAtUtc");
            const auto ArtifactsIt = Root.find("Artifacts");
            if (CacheKeyIt == Root.end() || !CacheKeyIt->is_string() || RequestHashIt == Root.end() ||
                !RequestHashIt->is_string() || NodeTypeIt == Root.end() || !NodeTypeIt->is_string() ||
                StoredAtIt == Root.end() || !StoredAtIt->is_string() || ArtifactsIt == Root.end() ||
                !ArtifactsIt->is_array())
            {
                return std::nullopt;
            }

            const std::optional<EBuildNodeType> NodeType = ParseNodeType(NodeTypeIt->get<std::string>());
            if (!NodeType.has_value())
            {
                return std::nullopt;
            }

            BuildCacheEntry Entry{};
            Entry.CacheKey = TrimCopy(CacheKeyIt->get<std::string>());
            Entry.RequestHash = TrimCopy(RequestHashIt->get<std::string>());
            Entry.NodeType = *NodeType;
            Entry.StoredAtUtc = TrimCopy(StoredAtIt->get<std::string>());

            for (const Json& ArtifactJson : *ArtifactsIt)
            {
                if (!ArtifactJson.is_object())
                {
                    return std::nullopt;
                }

                const auto PathIt = ArtifactJson.find("Path");
                const auto KindIt = ArtifactJson.find("Kind");
                if (PathIt == ArtifactJson.end() || !PathIt->is_string() || KindIt == ArtifactJson.end() ||
                    !KindIt->is_string())
                {
                    return std::nullopt;
                }

                const std::optional<EBuildCacheArtifactKind> Kind = ParseArtifactKind(KindIt->get<std::string>());
                if (!Kind.has_value())
                {
                    return std::nullopt;
                }

                Entry.Artifacts.push_back(BuildCacheArtifact{
                    .Path = std::filesystem::path(PathIt->get<std::string>()).lexically_normal(),
                    .Kind = *Kind,
                });
            }

            return Entry;
        }

    } // namespace

    std::filesystem::path BuildCacheService::CacheRootDirectory(const ResolvedBuildRequest& Request)
    {
        return (Request.Project.IntermediateRootDirectory / "BuildCache" / Request.Profile.Platform /
                ToString(Request.Profile.Configuration))
            .lexically_normal();
    }

    bool BuildCacheService::SupportsPersistentCache(const BuildGraphNode& Node)
    {
        if (!Node.Cacheable || TrimCopy(Node.CacheKey).empty() || Node.Outputs.empty())
        {
            return false;
        }

        switch (Node.Type)
        {
        case EBuildNodeType::CookAssets:
        case EBuildNodeType::WriteSnpak:
        case EBuildNodeType::StageBinaries:
        case EBuildNodeType::StageAssets:
            return true;
        default:
            return false;
        }
    }

    TExpected<std::optional<BuildCacheRestoreResult>>
    BuildCacheService::TryRestore(const ResolvedBuildRequest& Request, const BuildGraphNode& Node)
    {
        if (!SupportsPersistentCache(Node))
        {
            return std::optional<BuildCacheRestoreResult>{};
        }

        const std::filesystem::path EntryPath = CacheEntryPath(Request, Node);
        std::error_code Error{};
        if (!std::filesystem::exists(EntryPath, Error) || Error)
        {
            return std::optional<BuildCacheRestoreResult>{};
        }

        auto Text = ReadTextFile(EntryPath);
        if (!Text)
        {
            return std::optional<BuildCacheRestoreResult>{};
        }

        const std::optional<BuildCacheEntry> Entry = ParseEntry(*Text);
        const std::string EffectiveCacheKey = BuildEffectiveCacheKey(Node);
        if (!Entry.has_value() || Entry->CacheKey != EffectiveCacheKey ||
            Entry->Artifacts.size() != Node.Outputs.size())
        {
            return std::optional<BuildCacheRestoreResult>{};
        }

        for (const BuildCacheArtifact& Artifact : Entry->Artifacts)
        {
            if (!std::filesystem::exists(Artifact.Path))
            {
                return std::optional<BuildCacheRestoreResult>{};
            }
        }

        std::vector<std::string> RestoredOutputs{};
        RestoredOutputs.reserve(Node.Outputs.size());
        for (std::size_t Index = 0u; Index < Entry->Artifacts.size(); ++Index)
        {
            const BuildCacheArtifact& Artifact = Entry->Artifacts[Index];
            const std::filesystem::path Destination = std::filesystem::path(Node.Outputs[Index]).lexically_normal();

            Result RestoreResult = Artifact.Kind == EBuildCacheArtifactKind::Directory
                ? CopyDirectoryTree(Artifact.Path, Destination)
                : CopyFile(Artifact.Path, Destination);
            if (!RestoreResult)
            {
                return std::unexpected(RestoreResult.error());
            }

            RestoredOutputs.push_back(NormalizePathString(Destination));
        }

        return BuildCacheRestoreResult{
            .Message = "Restored node outputs from persistent build cache.",
            .Outputs = std::move(RestoredOutputs),
        };
    }

    Result BuildCacheService::Store(const ResolvedBuildRequest& Request, const BuildGraphNode& Node)
    {
        if (!SupportsPersistentCache(Node))
        {
            return Ok();
        }

        BuildCacheEntry Entry{};
        Entry.CacheKey = BuildEffectiveCacheKey(Node);
        Entry.RequestHash = Request.RequestHash;
        Entry.NodeType = Node.Type;
        Entry.StoredAtUtc = MakeUtcTimestamp();

        for (const std::string& OutputText : Node.Outputs)
        {
            const std::filesystem::path OutputPath = std::filesystem::path(OutputText).lexically_normal();
            std::error_code Error{};
            if (!std::filesystem::exists(OutputPath, Error) || Error)
            {
                return Ok();
            }

            if (std::filesystem::is_directory(OutputPath, Error) && !Error)
            {
                Entry.Artifacts.push_back(BuildCacheArtifact{
                    .Path = OutputPath,
                    .Kind = EBuildCacheArtifactKind::Directory,
                });
                continue;
            }
            if (std::filesystem::is_regular_file(OutputPath, Error) && !Error)
            {
                Entry.Artifacts.push_back(BuildCacheArtifact{
                    .Path = OutputPath,
                    .Kind = EBuildCacheArtifactKind::File,
                });
                continue;
            }

            return Ok();
        }

        if (Entry.Artifacts.size() != Node.Outputs.size())
        {
            return Ok();
        }

        const std::filesystem::path EntryPath = CacheEntryPath(Request, Node);
        return WriteTextFile(EntryPath, SerializeEntry(Entry));
    }

} // namespace SnAPI::GameFramework
