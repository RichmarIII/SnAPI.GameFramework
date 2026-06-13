#include "PackageManifest.h"

#include <AssetPackReader.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace SnAPI::GameFramework
{
    namespace
    {

        using Json = nlohmann::ordered_json;

        constexpr std::string_view kRuleManifestBuildIdMissing = "PackageManifest.BuildIdMissing";
        constexpr std::string_view kRuleManifestProjectNameMissing = "PackageManifest.ProjectNameMissing";
        constexpr std::string_view kRuleManifestPlatformMissing = "PackageManifest.PlatformMissing";
        constexpr std::string_view kRuleManifestStageDirectoryMissing = "PackageManifest.StageDirectoryMissing";

        constexpr std::uint64_t kFnv1aOffset = 14695981039346656037ull;
        constexpr std::uint64_t kFnv1aPrime = 1099511628211ull;

        /**
         * @brief Convert one build-configuration enum into canonical text.
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
         * @brief Convert one module-type enum into canonical text.
         * @param Type Module type to stringify.
         * @return Canonical module-type name.
         */
        [[nodiscard]] std::string ToString(const EProjectModuleType Type)
        {
            switch (Type)
            {
            case EProjectModuleType::Runtime:
                return "Runtime";
            case EProjectModuleType::Editor:
                return "Editor";
            case EProjectModuleType::Shared:
                return "Shared";
            case EProjectModuleType::Developer:
                return "Developer";
            case EProjectModuleType::Test:
                return "Test";
            case EProjectModuleType::Program:
                return "Program";
            }

            return "Runtime";
        }

        /**
         * @brief Normalize one filesystem path for manifest storage.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
        }

        /**
         * @brief Append one build-validation issue to a destination list.
         * @param Issues Destination issue list.
         * @param Severity Validation severity to record.
         * @param RuleId Stable rule identifier.
         * @param Message Human-readable diagnostic message.
         */
        void AppendIssue(std::vector<BuildValidationIssue>& Issues, const EBuildValidationSeverity Severity,
                         const std::string_view RuleId, std::string Message)
        {
            Issues.push_back(BuildValidationIssue{
                .Severity = Severity,
                .RuleId = std::string(RuleId),
                .Message = std::move(Message),
            });
        }

        /**
         * @brief Return the first blocking validation issue when one exists.
         * @param Issues Validation issues to inspect.
         * @return First error issue or `nullptr`.
         */
        [[nodiscard]] const BuildValidationIssue* FindBlockingIssue(const std::vector<BuildValidationIssue>& Issues)
        {
            const auto It = std::ranges::find_if(Issues, [](const BuildValidationIssue& Issue)
                                                 { return Issue.Severity == EBuildValidationSeverity::Error; });
            return It == Issues.end() ? nullptr : std::addressof(*It);
        }

        /**
         * @brief Hash one block of bytes with 64-bit FNV-1a.
         * @param Data Input byte block.
         * @param Size Byte count.
         * @param Seed Existing hash state.
         * @return Updated hash value.
         */
        [[nodiscard]] std::uint64_t HashBytes64(const void* Data, const std::size_t Size, std::uint64_t Seed)
        {
            std::uint64_t Hash = Seed;
            const auto* Bytes = static_cast<const std::uint8_t*>(Data);
            for (std::size_t Index = 0; Index < Size; ++Index)
            {
                Hash ^= static_cast<std::uint64_t>(Bytes[Index]);
                Hash *= kFnv1aPrime;
            }
            return Hash;
        }

        /**
         * @brief Convert one 64-bit hash to lowercase hexadecimal text.
         * @param Value Hash value to format.
         * @return Lowercase hexadecimal string.
         */
        [[nodiscard]] std::string ToHexString(const std::uint64_t Value)
        {
            std::ostringstream Stream{};
            Stream << std::hex << std::setfill('0') << std::setw(16) << Value;
            return Stream.str();
        }

        /**
         * @brief Hash one regular file on disk.
         * @param FilePath File to hash.
         * @return Lowercase hexadecimal content hash or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::string> HashFileContents(const std::filesystem::path& FilePath)
        {
            std::ifstream Input(FilePath, std::ios::binary);
            if (!Input.is_open())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to open staged file for hashing: " + FilePath.string()));
            }

            std::uint64_t Hash = kFnv1aOffset;
            char Buffer[64 * 1024]{};
            while (Input.good())
            {
                Input.read(Buffer, static_cast<std::streamsize>(sizeof(Buffer)));
                const std::streamsize BytesRead = Input.gcount();
                if (BytesRead > 0)
                {
                    Hash = HashBytes64(Buffer, static_cast<std::size_t>(BytesRead), Hash);
                }
            }

            if (!Input.eof() && Input.fail())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed while hashing staged file: " + FilePath.string()));
            }

            return ToHexString(Hash);
        }

        /**
         * @brief Enumerate regular files under one directory tree in deterministic order.
         * @param RootDirectory Directory tree to inspect.
         * @return Sorted regular file paths or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::vector<std::filesystem::path>> EnumerateRegularFiles(
            const std::filesystem::path& RootDirectory)
        {
            std::vector<std::filesystem::path> Files{};
            if (RootDirectory.empty())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Stage directory cannot be empty for package enumeration"));
            }

            std::error_code Error{};
            if (!std::filesystem::exists(RootDirectory, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Stage directory does not exist for package enumeration"));
            }

            for (const auto& Entry : std::filesystem::recursive_directory_iterator(RootDirectory, Error))
            {
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to enumerate staged files: " + Error.message()));
                }
                if (Entry.is_regular_file())
                {
                    Files.push_back(Entry.path().lexically_normal());
                }
            }

            std::ranges::sort(Files, [](const std::filesystem::path& Left, const std::filesystem::path& Right)
                              { return NormalizePathString(Left) < NormalizePathString(Right); });
            return Files;
        }

        /**
         * @brief Read the asset count from one `.snpak` file when possible.
         * @param PackPath `.snpak` file to inspect.
         * @return Discovered asset count, or `0` when the bundle cannot be opened.
         */
        [[nodiscard]] std::uint64_t TryReadSnpakAssetCount(const std::filesystem::path& PackPath)
        {
            ::SnAPI::AssetPipeline::AssetPackReader Reader{};
            auto OpenResult = Reader.Open(PackPath.string());
            if (!OpenResult.has_value())
            {
                return 0u;
            }
            return Reader.GetAssetCount();
        }

        /**
         * @brief Convert one staged file path into one manifest output-file entry.
         * @param StageDirectory Stage-root directory.
         * @param FilePath Regular staged file to convert.
         * @return Output-file entry or a structured filesystem error.
         */
        [[nodiscard]] TExpected<PackageManifestOutputFile> BuildOutputFile(
            const std::filesystem::path& StageDirectory, const std::filesystem::path& FilePath)
        {
            std::error_code Error{};
            const std::filesystem::path RelativePath = std::filesystem::relative(FilePath, StageDirectory, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to relativize staged file: " + Error.message()));
            }

            auto Hash = HashFileContents(FilePath);
            if (!Hash)
            {
                return std::unexpected(Hash.error());
            }

            const std::uint64_t SizeBytes = static_cast<std::uint64_t>(std::filesystem::file_size(FilePath, Error));
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to determine staged file size: " + Error.message()));
            }

            return PackageManifestOutputFile{
                .RelativePath = NormalizePathString(RelativePath),
                .SizeBytes = SizeBytes,
                .ContentHash = std::move(*Hash),
            };
        }

        /**
         * @brief Build `.snpak` metadata for one staged output-file entry when applicable.
         * @param Entry Staged output-file entry.
         * @param FilePath Absolute staged file path.
         * @return Optional `.snpak` metadata entry.
         */
        [[nodiscard]] std::optional<PackageManifestSnpakFile> BuildSnpakEntry(const PackageManifestOutputFile& Entry,
                                                                               const std::filesystem::path& FilePath,
                                                                               const std::unordered_map<std::string, std::string>& ChunkIdLookup)
        {
            if (FilePath.extension() != ".snpak")
            {
                return std::nullopt;
            }

            const std::string RelativePath = Entry.RelativePath;
            std::string ChunkId = FilePath.stem().string();
            if (const auto It = ChunkIdLookup.find(RelativePath); It != ChunkIdLookup.end() && !It->second.empty())
            {
                ChunkId = It->second;
            }

            return PackageManifestSnpakFile{
                .RelativePath = RelativePath,
                .ChunkId = std::move(ChunkId),
                .AssetCount = TryReadSnpakAssetCount(FilePath),
                .SizeBytes = Entry.SizeBytes,
                .ContentHash = Entry.ContentHash,
            };
        }

        /**
         * @brief Load the cook-manifest `.snpak` chunk-id map when it is available.
         * @param HistoryDirectory Build history directory that owns `Manifests/CookManifest.json`.
         * @return Relative staged `.snpak` path to chunk-id map.
         */
        [[nodiscard]] std::unordered_map<std::string, std::string>
        LoadCookManifestChunkLookup(const std::filesystem::path& HistoryDirectory)
        {
            std::unordered_map<std::string, std::string> Lookup{};

            const std::filesystem::path CookManifestPath = HistoryDirectory / "Manifests" / "CookManifest.json";
            std::ifstream Input(CookManifestPath, std::ios::binary);
            if (!Input.is_open())
            {
                return Lookup;
            }

            const std::string Text((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
            Json Root = Json::parse(Text, nullptr, false);
            if (Root.is_discarded() || !Root.is_object())
            {
                return Lookup;
            }

            const auto It = Root.find("SnpakFiles");
            if (It == Root.end() || !It->is_array())
            {
                return Lookup;
            }

            for (const Json& Entry : *It)
            {
                if (!Entry.is_object())
                {
                    continue;
                }

                const std::string Name = Entry.value("Name", std::string{});
                const std::string ChunkId = Entry.value("ChunkId", std::string{});
                if (Name.empty() || ChunkId.empty())
                {
                    continue;
                }

                Lookup.emplace("Assets/" + Name, ChunkId);
            }

            return Lookup;
        }

        /**
         * @brief Serialize one output-file entry into ordered JSON.
         * @param Entry Output-file entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeOutputFile(const PackageManifestOutputFile& Entry)
        {
            return Json::object({
                {"RelativePath", Entry.RelativePath},
                {"SizeBytes", Entry.SizeBytes},
                {"ContentHash", Entry.ContentHash},
            });
        }

        /**
         * @brief Serialize one `.snpak` entry into ordered JSON.
         * @param Entry `.snpak` entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeSnpakFile(const PackageManifestSnpakFile& Entry)
        {
            return Json::object({
                {"RelativePath", Entry.RelativePath},
                {"ChunkId", Entry.ChunkId},
                {"AssetCount", Entry.AssetCount},
                {"SizeBytes", Entry.SizeBytes},
                {"ContentHash", Entry.ContentHash},
            });
        }

        /**
         * @brief Serialize one module entry into ordered JSON.
         * @param Entry Module entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeModule(const PackageManifestModule& Entry)
        {
            return Json::object({
                {"Name", Entry.Name},
                {"Type", ToString(Entry.Type)},
                {"LoadInEditor", Entry.LoadInEditor},
                {"LoadInRuntime", Entry.LoadInRuntime},
            });
        }

        /**
         * @brief Serialize one warning entry into ordered JSON.
         * @param Entry Warning entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeWarning(const PackageManifestWarning& Entry)
        {
            return Json::object({
                {"RuleId", Entry.RuleId},
                {"Message", Entry.Message},
            });
        }

    } // namespace

    TExpected<PackageManifest> PackageManifestService::Create(const ResolvedBuildRequest& Request,
                                                              const BuildGraph& Graph)
    {
        PackageManifest Manifest{};
        Manifest.BuildId = Graph.BuildId;
        Manifest.RequestHash = Request.RequestHash;
        Manifest.ProjectId = Request.Project.Descriptor.Project.ProjectId;
        Manifest.ProjectName = Request.Project.Descriptor.Project.Name;
        Manifest.ProfileName = Request.ProfileName;
        Manifest.TargetPlatform = Request.Profile.Platform;
        Manifest.Configuration = Request.Profile.Configuration;
        Manifest.ExecutionEnvironment = Request.Profile.ExecutionEnvironment;
        Manifest.DescriptorSchemaVersion = Request.Project.Descriptor.Format.SchemaVersion;
        Manifest.MinimumToolVersion = Request.Project.Descriptor.Format.MinimumToolVersion;
        Manifest.StageDirectory = Graph.StageDirectory.lexically_normal();
        Manifest.IncludedLevels = Request.Profile.SelectedLevels;
        if (Manifest.IncludedLevels.empty() && !Request.Project.Descriptor.Startup.StartupLevelAsset.empty())
        {
            Manifest.IncludedLevels.push_back(Request.Project.Descriptor.Startup.StartupLevelAsset);
        }

        for (const ProjectModuleDescriptor& Module : Request.Project.Descriptor.Modules)
        {
            if (!Module.LoadInRuntime)
            {
                continue;
            }

            Manifest.Modules.push_back(PackageManifestModule{
                .Name = Module.Name,
                .Type = Module.Type,
                .LoadInEditor = Module.LoadInEditor,
                .LoadInRuntime = Module.LoadInRuntime,
            });
        }

        for (const BuildValidationIssue& Issue : Request.ValidationIssues)
        {
            if (Issue.Severity != EBuildValidationSeverity::Warning)
            {
                continue;
            }
            Manifest.Warnings.push_back(PackageManifestWarning{
                .RuleId = Issue.RuleId,
                .Message = Issue.Message,
            });
        }

        auto Files = EnumerateRegularFiles(Manifest.StageDirectory);
        if (!Files)
        {
            return std::unexpected(Files.error());
        }

        const std::unordered_map<std::string, std::string> ChunkIdLookup =
            LoadCookManifestChunkLookup(Graph.HistoryDirectory);

        for (const std::filesystem::path& FilePath : *Files)
        {
            auto OutputEntry = BuildOutputFile(Manifest.StageDirectory, FilePath);
            if (!OutputEntry)
            {
                return std::unexpected(OutputEntry.error());
            }

            if (auto SnpakEntry = BuildSnpakEntry(*OutputEntry, FilePath, ChunkIdLookup); SnpakEntry.has_value())
            {
                Manifest.SnpakFiles.push_back(std::move(*SnpakEntry));
            }

            Manifest.OutputFiles.push_back(std::move(*OutputEntry));
        }

        const auto Issues = Validate(Manifest);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        return Manifest;
    }

    std::vector<BuildValidationIssue> PackageManifestService::Validate(const PackageManifest& Manifest)
    {
        std::vector<BuildValidationIssue> Issues{};

        if (Manifest.BuildId.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleManifestBuildIdMissing,
                        "Package manifests require a non-empty build id.");
        }
        if (Manifest.ProjectName.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleManifestProjectNameMissing,
                        "Package manifests require a non-empty project name.");
        }
        if (Manifest.TargetPlatform.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleManifestPlatformMissing,
                        "Package manifests require a non-empty target platform.");
        }
        if (Manifest.StageDirectory.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleManifestStageDirectoryMissing,
                        "Package manifests require a non-empty stage directory.");
        }

        return Issues;
    }

    TExpected<std::string> PackageManifestService::Serialize(const PackageManifest& Manifest, const int Indent)
    {
        try
        {
            const auto Issues = Validate(Manifest);
            if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
            }

            Json Root = Json::object({
                {"BuildId", Manifest.BuildId},
                {"RequestHash", Manifest.RequestHash},
                {"ProjectId", Manifest.ProjectId},
                {"ProjectName", Manifest.ProjectName},
                {"ProfileName", Manifest.ProfileName},
                {"TargetPlatform", Manifest.TargetPlatform},
                {"Configuration", ToString(Manifest.Configuration)},
                {"ExecutionEnvironment", Manifest.ExecutionEnvironment},
                {"ToolVersions",
                 Json::object({
                     {"MinimumToolVersion", Manifest.MinimumToolVersion},
                     {"DescriptorSchemaVersion", Manifest.DescriptorSchemaVersion},
                 })},
                {"StageDirectory", NormalizePathString(Manifest.StageDirectory)},
                {"IncludedLevels", Manifest.IncludedLevels},
                {"OutputFiles", Json::array()},
                {"SnpakFiles", Json::array()},
                {"Modules", Json::array()},
                {"Warnings", Json::array()},
            });

            for (const PackageManifestOutputFile& Entry : Manifest.OutputFiles)
            {
                Root["OutputFiles"].push_back(SerializeOutputFile(Entry));
            }
            for (const PackageManifestSnpakFile& Entry : Manifest.SnpakFiles)
            {
                Root["SnpakFiles"].push_back(SerializeSnpakFile(Entry));
            }
            for (const PackageManifestModule& Entry : Manifest.Modules)
            {
                Root["Modules"].push_back(SerializeModule(Entry));
            }
            for (const PackageManifestWarning& Entry : Manifest.Warnings)
            {
                Root["Warnings"].push_back(SerializeWarning(Entry));
            }

            return Root.dump(Indent) + "\n";
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

    TExpected<std::string> PackageManifestService::SerializeStageFileHashes(const PackageManifest& Manifest,
                                                                            const int Indent)
    {
        try
        {
            const auto Issues = Validate(Manifest);
            if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
            }

            Json Root = Json::object({
                {"BuildId", Manifest.BuildId},
                {"ProjectName", Manifest.ProjectName},
                {"StageDirectory", NormalizePathString(Manifest.StageDirectory)},
                {"Files", Json::array()},
            });

            for (const PackageManifestOutputFile& Entry : Manifest.OutputFiles)
            {
                Root["Files"].push_back(SerializeOutputFile(Entry));
            }

            return Root.dump(Indent) + "\n";
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

} // namespace SnAPI::GameFramework
