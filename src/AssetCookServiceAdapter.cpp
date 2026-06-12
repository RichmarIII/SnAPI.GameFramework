#include "AssetCookServiceAdapter.h"

#include "AssetPipelineIds.h"
#include "AssetPipelineFactories.h"
#include "AuthoredAssetRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <optional>
#include <ranges>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <AssetPipeline.h>

namespace SnAPI::GameFramework
{
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateAuthoredAssetJsonImporter();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateAuthoredAssetPassThroughCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateNodeSourceCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateLevelSourceCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateWorldSourceCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetJsonImporter();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderTextureCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialInstanceCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletonCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderAnimationCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderStaticMeshCooker();
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletalMeshCooker();

    namespace
    {
        using Json = nlohmann::ordered_json;

        constexpr std::string_view kCookedAssetIndexFileName = "CookedAssets.index.json";
        constexpr std::string_view kImportedMaterialSourceSuffix = ".snmaterial.json";
        constexpr std::string_view kImportedMaterialInstanceSourceSuffix = ".snmatinst.json";
        constexpr std::string_view kImportedStaticMeshSourceSuffix = ".snstaticmesh.json";
        constexpr std::string_view kImportedSkeletalMeshSourceSuffix = ".snskeletalmesh.json";

        /**
         * @brief Create one directory tree when it does not already exist.
         * @param Directory Directory path to create.
         * @return Success or a filesystem error.
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
         * @brief Read one UTF-8 text file into memory.
         * @param FilePath Source file path.
         * @return File text or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::string> ReadTextFile(const std::filesystem::path& FilePath)
        {
            std::ifstream Stream(FilePath, std::ios::binary);
            if (!Stream.is_open())
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Failed to open file: " + FilePath.string()));
            }

            std::string Text{};
            Stream.seekg(0, std::ios::end);
            Text.resize(static_cast<std::size_t>(Stream.tellg()));
            Stream.seekg(0, std::ios::beg);
            if (!Text.empty())
            {
                Stream.read(Text.data(), static_cast<std::streamsize>(Text.size()));
            }
            return Text;
        }

        /**
         * @brief Write one UTF-8 text file, creating parent directories as needed.
         * @param FilePath Output file path.
         * @param Text Text payload to write.
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
                    MakeError(EErrorCode::InternalError, "Failed to open output file: " + FilePath.string()));
            }

            Stream.write(Text.data(), static_cast<std::streamsize>(Text.size()));
            if (!Stream.good())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to write output file: " + FilePath.string()));
            }
            return Ok();
        }

        /**
         * @brief Normalize one filesystem path for manifests and reports.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
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
         * @brief Lowercase one copied ASCII string.
         * @param Text Source text.
         * @return Lowercased copy.
         */
        [[nodiscard]] std::string ToLowerCopy(std::string Text)
        {
            std::ranges::transform(Text, Text.begin(), [](const unsigned char Character) {
                return static_cast<char>(std::tolower(Character));
            });
            return Text;
        }

        /**
         * @brief Normalize one asset/source extension for comparisons.
         * @param Extension Raw extension string.
         * @return Lowercase extension with a leading dot when present.
         */
        [[nodiscard]] std::string NormalizeAssetExtension(std::string Extension)
        {
            Extension = TrimCopy(Extension);
            if (Extension.empty())
            {
                return Extension;
            }
            if (Extension.front() != '.')
            {
                Extension.insert(Extension.begin(), '.');
            }
            return ToLowerCopy(std::move(Extension));
        }

        /**
         * @brief Return `true` when one string ends with another string, case-insensitively.
         * @param Text Source text.
         * @param Suffix Expected suffix.
         * @return `true` when `Text` ends with `Suffix`.
         */
        [[nodiscard]] bool EndsWithInsensitive(const std::string_view Text, const std::string_view Suffix)
        {
            if (Suffix.size() > Text.size())
            {
                return false;
            }

            const std::size_t Offset = Text.size() - Suffix.size();
            for (std::size_t Index = 0u; Index < Suffix.size(); ++Index)
            {
                const char Left = static_cast<char>(std::tolower(static_cast<unsigned char>(Text[Offset + Index])));
                const char Right = static_cast<char>(std::tolower(static_cast<unsigned char>(Suffix[Index])));
                if (Left != Right)
                {
                    return false;
                }
            }
            return true;
        }

        /**
         * @brief Normalize one asset-root-relative logical name.
         * @param LogicalName Authored or discovered logical name.
         * @return Canonical generic-string logical name.
         */
        [[nodiscard]] std::string NormalizeLogicalName(std::string_view LogicalName)
        {
            std::string Value = TrimCopy(LogicalName);
            std::replace(Value.begin(), Value.end(), '\\', '/');
            Value = std::filesystem::path(Value).lexically_normal().generic_string();
            if (Value == ".")
            {
                return {};
            }
            if (!Value.empty() && Value.starts_with("./"))
            {
                Value.erase(0u, 2u);
            }
            return Value;
        }

        /**
         * @brief Canonicalize one authored asset-relative field against the project asset root.
         * @param Request Frozen build request that owns the asset root.
         * @param RawField Authored field text.
         * @return Canonical asset-root-relative field text.
         */
        [[nodiscard]] std::string CanonicalizeAssetField(const ResolvedBuildRequest& Request, std::string_view RawField)
        {
            std::string Value = NormalizeLogicalName(RawField);
            if (Value.empty())
            {
                return {};
            }

            const std::string AssetRootField =
                NormalizeLogicalName(Request.Project.Descriptor.Paths.AssetRoot);
            if (!AssetRootField.empty() && (Value == AssetRootField || Value.starts_with(AssetRootField + "/")))
            {
                Value.erase(0u, AssetRootField.size());
                if (!Value.empty() && Value.front() == '/')
                {
                    Value.erase(Value.begin());
                }
            }

            const std::string AssetRootLeaf = NormalizeLogicalName(Request.Project.AssetRootDirectory.filename().string());
            if (!AssetRootLeaf.empty() && (Value == AssetRootLeaf || Value.starts_with(AssetRootLeaf + "/")))
            {
                Value.erase(0u, AssetRootLeaf.size());
                if (!Value.empty() && Value.front() == '/')
                {
                    Value.erase(Value.begin());
                }
            }

            return NormalizeLogicalName(Value);
        }

        /**
         * @brief Return `true` when one logical asset name is under one folder rule.
         * @param LogicalName Asset-root-relative logical name.
         * @param FolderRule Canonical asset-root-relative folder rule.
         * @return `true` when the asset is contained by the folder rule.
         */
        [[nodiscard]] bool LogicalNameMatchesFolderRule(const std::string_view LogicalName,
                                                        const std::string_view FolderRule)
        {
            if (FolderRule.empty())
            {
                return false;
            }

            if (LogicalName == FolderRule)
            {
                return true;
            }

            const std::string Prefix = std::string(FolderRule) + "/";
            return LogicalName.starts_with(Prefix);
        }

        /**
         * @brief Classification of one package candidate source file.
         */
        struct PackageSourceDescriptor
        {
            std::string AssetKindLabel{};
            bool Cookable = false;
            bool StageVerbatim = false;
        };

        /**
         * @brief Return `true` when one source file should be considered a package candidate.
         * @param Path Candidate source file path.
         * @return `true` when the file may participate in package selection.
         */
        [[nodiscard]] bool IsPackageCandidateFile(const std::filesystem::path& Path)
        {
            if (!std::filesystem::is_regular_file(Path))
            {
                return false;
            }

            return NormalizeAssetExtension(Path.extension().string()) != ".snpak";
        }

        /**
         * @brief Return `true` when one source file maps to a cookable authored/imported asset.
         * @param Path Candidate source file path.
         * @return `true` when the path should be cooked through `SnAPI.AssetPipeline`.
         */
        [[nodiscard]] bool IsCookableSourceFile(const std::filesystem::path& Path)
        {
            if (!IsPackageCandidateFile(Path))
            {
                return false;
            }

            AuthoredAssetRegistry::Instance().EnsureBuilt();
            if (AuthoredAssetRegistry::Instance().FindByExtension(Path.extension().string()) != nullptr)
            {
                return true;
            }

            const std::string PathText = NormalizePathString(Path);
            return EndsWithInsensitive(PathText, kImportedMaterialSourceSuffix) ||
                   EndsWithInsensitive(PathText, kImportedMaterialInstanceSourceSuffix) ||
                   EndsWithInsensitive(PathText, kImportedStaticMeshSourceSuffix) ||
                   EndsWithInsensitive(PathText, kImportedSkeletalMeshSourceSuffix);
        }

        /**
         * @brief Classify one package candidate by cookability and human-readable kind label.
         * @param Path Candidate source file path.
         * @return Package-source classification.
         */
        [[nodiscard]] PackageSourceDescriptor DescribePackageSource(const std::filesystem::path& Path)
        {
            PackageSourceDescriptor Descriptor{};
            if (!IsPackageCandidateFile(Path))
            {
                return Descriptor;
            }

            const std::string Extension = NormalizeAssetExtension(Path.extension().string());
            const std::string PathText = NormalizePathString(Path);

            AuthoredAssetRegistry::Instance().EnsureBuilt();
            if (const auto* AuthoredDescriptor = AuthoredAssetRegistry::Instance().FindByExtension(Path.extension().string());
                AuthoredDescriptor != nullptr)
            {
                Descriptor.AssetKindLabel = AuthoredDescriptor->DisplayName;
                Descriptor.Cookable = true;
                return Descriptor;
            }

            if (EndsWithInsensitive(PathText, kImportedMaterialSourceSuffix))
            {
                Descriptor.AssetKindLabel = "Material";
                Descriptor.Cookable = true;
                return Descriptor;
            }
            if (EndsWithInsensitive(PathText, kImportedMaterialInstanceSourceSuffix))
            {
                Descriptor.AssetKindLabel = "Material Instance";
                Descriptor.Cookable = true;
                return Descriptor;
            }
            if (EndsWithInsensitive(PathText, kImportedStaticMeshSourceSuffix))
            {
                Descriptor.AssetKindLabel = "Static Mesh";
                Descriptor.Cookable = true;
                return Descriptor;
            }
            if (EndsWithInsensitive(PathText, kImportedSkeletalMeshSourceSuffix))
            {
                Descriptor.AssetKindLabel = "Skeletal Mesh";
                Descriptor.Cookable = true;
                return Descriptor;
            }

            Descriptor.AssetKindLabel = Extension.empty() ? std::string("Source File") : (Extension + " Source");
            Descriptor.StageVerbatim = true;
            return Descriptor;
        }

        /**
         * @brief Return `true` when one package source matches one authored kind rule.
         * @param Descriptor Package-source classification.
         * @param Path Candidate source file path.
         * @param KindRule Authored include/exclude kind rule.
         * @return `true` when the rule matches the candidate.
         */
        [[nodiscard]] bool PackageSourceMatchesKindRule(const PackageSourceDescriptor& Descriptor,
                                                        const std::filesystem::path& Path,
                                                        std::string_view KindRule)
        {
            const std::string NormalizedRule = ToLowerCopy(TrimCopy(KindRule));
            if (NormalizedRule.empty())
            {
                return false;
            }

            const std::string Extension = NormalizeAssetExtension(Path.extension().string());
            const std::string KindLabel = ToLowerCopy(Descriptor.AssetKindLabel);
            const std::string ExtensionSource = Extension.empty() ? std::string{} : ToLowerCopy(Extension + " Source");
            const std::string ExtensionBare = Extension.empty() ? std::string{} : Extension.substr(1u);

            return NormalizedRule == KindLabel ||
                   (!Extension.empty() && (NormalizedRule == ToLowerCopy(Extension) ||
                                           NormalizedRule == ExtensionBare ||
                                           NormalizedRule == ExtensionSource));
        }

        /**
         * @brief Locate one selected logical source asset under the project asset root.
         * @param Request Frozen build request that owns the asset root.
         * @param LogicalName Asset-root-relative logical name.
         * @return Concrete source file path or a structured validation/filesystem error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveSelectedSourcePath(const ResolvedBuildRequest& Request,
                                                                                 std::string_view LogicalName)
        {
            const std::string Normalized = CanonicalizeAssetField(Request, LogicalName);
            if (Normalized.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Selected asset logical name is empty"));
            }

            const std::filesystem::path SourcePath =
                (Request.Project.AssetRootDirectory / std::filesystem::path(Normalized)).lexically_normal();
            if (!std::filesystem::exists(SourcePath))
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Selected asset source file does not exist: " + SourcePath.string()));
            }
            if (!IsPackageCandidateFile(SourcePath))
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument,
                              "Selected asset source file is not a valid package input: " + SourcePath.string()));
            }
            return SourcePath;
        }

        /**
         * @brief Enumerate supported source assets under one folder rule.
         * @param Request Frozen build request that owns the asset root.
         * @param FolderRule Authored folder rule text.
         * @return Ordered selected source records discovered under the folder.
         */
        [[nodiscard]] TExpected<std::vector<AssetSelectionRecord>>
        EnumerateFolderSelection(const ResolvedBuildRequest& Request, std::string_view FolderRule)
        {
            const std::string RelativeFolder = CanonicalizeAssetField(Request, FolderRule);
            if (RelativeFolder.empty())
            {
                return std::vector<AssetSelectionRecord>{};
            }

            const std::filesystem::path FolderPath =
                (Request.Project.AssetRootDirectory / std::filesystem::path(RelativeFolder)).lexically_normal();
            if (!std::filesystem::exists(FolderPath))
            {
                return std::vector<AssetSelectionRecord>{};
            }

            std::vector<AssetSelectionRecord> Records{};
            for (const auto& Entry : std::filesystem::recursive_directory_iterator(FolderPath))
            {
                if (!Entry.is_regular_file())
                {
                    continue;
                }
                const PackageSourceDescriptor SourceDescriptor = DescribePackageSource(Entry.path());
                if (!SourceDescriptor.Cookable && !SourceDescriptor.StageVerbatim)
                {
                    continue;
                }

                const std::filesystem::path RelativePath =
                    std::filesystem::relative(Entry.path(), Request.Project.AssetRootDirectory);
                Records.push_back(AssetSelectionRecord{
                    .LogicalName = NormalizeLogicalName(RelativePath.generic_string()),
                    .SourcePath = Entry.path().lexically_normal(),
                    .AssetKindLabel = SourceDescriptor.AssetKindLabel,
                    .SelectionReason = "IncludeFolder",
                    .ExplicitSelection = false,
                    .Cookable = SourceDescriptor.Cookable,
                    .StageVerbatim = SourceDescriptor.StageVerbatim,
                });
            }

            std::ranges::sort(Records, {}, &AssetSelectionRecord::LogicalName);
            return Records;
        }

        /**
         * @brief Enumerate all supported source assets under the project asset root.
         * @param Request Frozen build request that owns the asset root.
         * @return Ordered source records discovered under the asset root.
         */
        [[nodiscard]] TExpected<std::vector<AssetSelectionRecord>>
        EnumerateWholeProjectSelection(const ResolvedBuildRequest& Request)
        {
            std::vector<AssetSelectionRecord> Records{};
            if (!std::filesystem::exists(Request.Project.AssetRootDirectory))
            {
                return std::vector<AssetSelectionRecord>{};
            }

            for (const auto& Entry : std::filesystem::recursive_directory_iterator(Request.Project.AssetRootDirectory))
            {
                if (!Entry.is_regular_file())
                {
                    continue;
                }
                const PackageSourceDescriptor SourceDescriptor = DescribePackageSource(Entry.path());
                if (!SourceDescriptor.Cookable && !SourceDescriptor.StageVerbatim)
                {
                    continue;
                }

                const std::filesystem::path RelativePath =
                    std::filesystem::relative(Entry.path(), Request.Project.AssetRootDirectory);
                Records.push_back(AssetSelectionRecord{
                    .LogicalName = NormalizeLogicalName(RelativePath.generic_string()),
                    .SourcePath = Entry.path().lexically_normal(),
                    .AssetKindLabel = SourceDescriptor.AssetKindLabel,
                    .SelectionReason = "WholeProject",
                    .ExplicitSelection = false,
                    .Cookable = SourceDescriptor.Cookable,
                    .StageVerbatim = SourceDescriptor.StageVerbatim,
                });
            }

            std::ranges::sort(Records, {}, &AssetSelectionRecord::LogicalName);
            return Records;
        }

        /**
         * @brief Append one unique provenance entry to a destination record.
         * @param Record Destination record.
         * @param Kind Stable provenance kind.
         * @param Value Authored selector/rule value.
         * @param Included `true` when the entry kept the asset, `false` when it excluded it.
         */
        void AppendUniqueProvenance(AssetSelectionRecord& Record, std::string Kind, std::string Value, bool Included);

        /**
         * @brief Enumerate package candidates that match one authored kind rule.
         * @param Request Frozen build request that owns the asset root.
         * @param KindRule Authored include/exclude kind rule.
         * @return Ordered matching package records.
         */
        [[nodiscard]] TExpected<std::vector<AssetSelectionRecord>>
        EnumerateKindSelection(const ResolvedBuildRequest& Request, std::string_view KindRule)
        {
            auto AllRecords = EnumerateWholeProjectSelection(Request);
            if (!AllRecords)
            {
                return std::unexpected(AllRecords.error());
            }

            std::vector<AssetSelectionRecord> Matches{};
            for (AssetSelectionRecord& Record : *AllRecords)
            {
                if (!PackageSourceMatchesKindRule(DescribePackageSource(Record.SourcePath), Record.SourcePath, KindRule))
                {
                    continue;
                }

                Record.SelectionReason = "IncludeAssetKind";
                AppendUniqueProvenance(Record, "IncludeAssetKind", TrimCopy(KindRule), true);
                Matches.push_back(std::move(Record));
            }

            std::ranges::sort(Matches, {}, &AssetSelectionRecord::LogicalName);
            return Matches;
        }

        /**
         * @brief Insert one selected asset record when it has not already been seen.
         * @param Record Record to insert.
         * @param Seen Logical-name set used for deduplication.
         * @param OutRecords Ordered output records.
         */
        void AddUniqueSelection(AssetSelectionRecord Record,
                                std::unordered_set<std::string>& Seen,
                                std::vector<AssetSelectionRecord>& OutRecords)
        {
            if (Record.LogicalName.empty())
            {
                return;
            }
            if (Seen.insert(Record.LogicalName).second)
            {
                OutRecords.push_back(std::move(Record));
            }
        }

        /**
         * @brief Register the currently available GameFramework importers and cookers on one pipeline engine.
         * @param Engine Asset pipeline engine to populate.
         */
        void RegisterGameFrameworkPipeline(::SnAPI::AssetPipeline::AssetPipelineEngine& Engine)
        {
            RegisterAssetPipelinePayloads(Engine.GetRegistry());

            Engine.RegisterImporter(CreateAuthoredAssetJsonImporter());
            Engine.RegisterImporter(CreateRenderAssetJsonImporter());
            Engine.RegisterImporter(CreateRenderAssetAssimpImporter());

            Engine.RegisterCooker(CreateAuthoredAssetPassThroughCooker());
            Engine.RegisterCooker(CreateNodeSourceCooker());
            Engine.RegisterCooker(CreateLevelSourceCooker());
            Engine.RegisterCooker(CreateWorldSourceCooker());
            Engine.RegisterCooker(CreateRenderTextureCooker());
            Engine.RegisterCooker(CreateRenderMaterialCooker());
            Engine.RegisterCooker(CreateRenderMaterialInstanceCooker());
            Engine.RegisterCooker(CreateRenderSkeletonCooker());
            Engine.RegisterCooker(CreateRenderAnimationCooker());
            Engine.RegisterCooker(CreateRenderStaticMeshCooker());
            Engine.RegisterCooker(CreateRenderSkeletalMeshCooker());
        }

        /**
         * @brief Convert one chunk-strategy enum into canonical text.
         * @param Strategy Chunk strategy to stringify.
         * @return Canonical chunk-strategy name.
         */
        [[nodiscard]] std::string ToString(const EAssetChunkStrategy Strategy)
        {
            switch (Strategy)
            {
            case EAssetChunkStrategy::Monolithic:
                return "Monolithic";
            case EAssetChunkStrategy::SharedPlusPerLevel:
                return "SharedPlusPerLevel";
            case EAssetChunkStrategy::PerLabel:
                return "PerLabel";
            case EAssetChunkStrategy::CustomGraph:
                return "CustomGraph";
            }

            return "Monolithic";
        }

        /**
         * @brief Hash one block of bytes with 64-bit FNV-1a.
         * @param Data Source byte span.
         * @param Size Byte count.
         * @param Seed Existing hash state.
         * @return Updated hash value.
         */
        [[nodiscard]] std::uint64_t HashBytes64(const void* Data, const std::size_t Size, std::uint64_t Seed)
        {
            constexpr std::uint64_t kFnv1aOffset = 14695981039346656037ull;
            constexpr std::uint64_t kFnv1aPrime = 1099511628211ull;

            std::uint64_t Hash = Seed == 0u ? kFnv1aOffset : Seed;
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
         * @return Lowercase hexadecimal text.
         */
        [[nodiscard]] std::string ToHexString(const std::uint64_t Value)
        {
            static constexpr char kDigits[] = "0123456789abcdef";
            std::string Text(16u, '0');
            for (std::size_t Index = 0u; Index < 16u; ++Index)
            {
                const std::size_t Shift = (15u - Index) * 4u;
                Text[Index] = kDigits[(Value >> Shift) & 0xFu];
            }
            return Text;
        }

        /**
         * @brief Hash one source file on disk.
         * @param FilePath File to hash.
         * @return Lowercase content hash or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::string> HashFileContents(const std::filesystem::path& FilePath)
        {
            std::ifstream Input(FilePath, std::ios::binary);
            if (!Input.is_open())
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Failed to open source asset for hashing: " + FilePath.string()));
            }

            std::uint64_t Hash = 0u;
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
                    MakeError(EErrorCode::InternalError, "Failed while hashing source asset: " + FilePath.string()));
            }

            return ToHexString(Hash);
        }

        void AppendUniqueProvenance(AssetSelectionRecord& Record, std::string Kind, std::string Value, const bool Included)
        {
            Value = TrimCopy(Value);
            if (Kind.empty() || Value.empty())
            {
                return;
            }

            const auto Existing = std::ranges::find_if(
                Record.Provenance,
                [&](const AssetSelectionProvenanceEntry& Entry)
                { return Entry.Kind == Kind && Entry.Value == Value && Entry.Included == Included; });
            if (Existing == Record.Provenance.end())
            {
                Record.Provenance.push_back(AssetSelectionProvenanceEntry{
                    .Kind = std::move(Kind),
                    .Value = std::move(Value),
                    .Included = Included,
                });
            }
        }

        /**
         * @brief Build one selected source record with one initial provenance entry.
         * @param LogicalName Canonical logical asset name.
         * @param SourcePath Concrete source path.
         * @param Reason Stable selection reason.
         * @param ReasonValue Authored selector/rule value that produced the record.
         * @param ExplicitSelection `true` when the selector was explicit.
         * @return Initialized selection record.
         */
        [[nodiscard]] AssetSelectionRecord MakeSelectionRecord(std::string LogicalName,
                                                               std::filesystem::path SourcePath,
                                                               std::string Reason,
                                                               std::string ReasonValue,
                                                               const bool ExplicitSelection)
        {
            const PackageSourceDescriptor SourceDescriptor = DescribePackageSource(SourcePath);
            AssetSelectionRecord Record{
                .LogicalName = std::move(LogicalName),
                .SourcePath = std::move(SourcePath),
                .AssetKindLabel = SourceDescriptor.AssetKindLabel,
                .SelectionReason = Reason,
                .ExplicitSelection = ExplicitSelection,
                .Cookable = SourceDescriptor.Cookable,
                .StageVerbatim = SourceDescriptor.StageVerbatim,
            };
            AppendUniqueProvenance(Record, Record.SelectionReason, std::move(ReasonValue), true);
            return Record;
        }

        /**
         * @brief Merge one source selection record into another with the same logical name.
         * @param Destination Existing output record.
         * @param Source New selection record to merge.
         */
        void MergeSelectionRecord(AssetSelectionRecord& Destination, const AssetSelectionRecord& Source)
        {
            if (Destination.SourcePath.empty())
            {
                Destination.SourcePath = Source.SourcePath;
            }
            if (Destination.AssetKindLabel.empty())
            {
                Destination.AssetKindLabel = Source.AssetKindLabel;
            }
            Destination.ExplicitSelection = Destination.ExplicitSelection || Source.ExplicitSelection;
            Destination.Cookable = Destination.Cookable || Source.Cookable;
            Destination.StageVerbatim = Destination.StageVerbatim || Source.StageVerbatim;
            if (Destination.SelectionReason.empty())
            {
                Destination.SelectionReason = Source.SelectionReason;
            }

            for (const AssetSelectionProvenanceEntry& Entry : Source.Provenance)
            {
                AppendUniqueProvenance(Destination, Entry.Kind, Entry.Value, Entry.Included);
            }
        }

        /**
         * @brief Insert one selection record or merge it with an existing logical-name match.
         * @param Record Record to insert or merge.
         * @param RecordIndices Logical-name to output-index map.
         * @param OutRecords Output record array.
         */
        void AddOrMergeSelection(AssetSelectionRecord Record,
                                 std::unordered_map<std::string, std::size_t>& RecordIndices,
                                 std::vector<AssetSelectionRecord>& OutRecords)
        {
            if (Record.LogicalName.empty())
            {
                return;
            }

            if (const auto It = RecordIndices.find(Record.LogicalName); It != RecordIndices.end())
            {
                MergeSelectionRecord(OutRecords[It->second], Record);
                return;
            }

            RecordIndices.emplace(Record.LogicalName, OutRecords.size());
            OutRecords.push_back(std::move(Record));
        }

        /**
         * @brief Convert one generic asset-pipeline dependency kind into canonical text.
         * @param Kind Generic dependency kind.
         * @return Stable dependency-kind label.
         */
        [[nodiscard]] std::string ToString(const ::SnAPI::AssetPipeline::EAssetDependencyKind Kind)
        {
            switch (Kind)
            {
            case ::SnAPI::AssetPipeline::EAssetDependencyKind::Required:
                return "Required";
            case ::SnAPI::AssetPipeline::EAssetDependencyKind::Optional:
                return "Optional";
            case ::SnAPI::AssetPipeline::EAssetDependencyKind::Auxiliary:
                return "Auxiliary";
            }

            return "Required";
        }

        /**
         * @brief Return `true` when one dependency kind should be expanded by the profile policy.
         * @param Policy Resolved build-profile dependency policy.
         * @param Kind Generic asset-pipeline dependency kind.
         * @return `true` when the dependency should be included.
         */
        [[nodiscard]] bool ShouldIncludeDependency(const EAssetDependencyPolicy Policy,
                                                   const ::SnAPI::AssetPipeline::EAssetDependencyKind Kind)
        {
            switch (Policy)
            {
            case EAssetDependencyPolicy::HardOnly:
                return Kind == ::SnAPI::AssetPipeline::EAssetDependencyKind::Required;
            case EAssetDependencyPolicy::HardAndSoft:
                return Kind != ::SnAPI::AssetPipeline::EAssetDependencyKind::Auxiliary;
            case EAssetDependencyPolicy::HardSoftAndEditorPreview:
                return true;
            case EAssetDependencyPolicy::CustomResolver:
                return false;
            }

            return Kind == ::SnAPI::AssetPipeline::EAssetDependencyKind::Required;
        }

        /**
         * @brief Convert one generic dependency kind into a selection-reason label.
         * @param Kind Generic dependency kind.
         * @return Stable selection reason.
         */
        [[nodiscard]] std::string MakeDependencySelectionReason(const ::SnAPI::AssetPipeline::EAssetDependencyKind Kind)
        {
            return ToString(Kind) + "Dependency";
        }

        /**
         * @brief One source-catalog entry used to resolve dependencies by logical name or asset id.
         */
        struct AssetSourceCatalogEntry
        {
            std::string LogicalName{};
            std::filesystem::path SourcePath{};
            std::string AssetId{};
        };

        /**
         * @brief Lookup tables for supported authored/imported source assets under the project asset root.
         */
        struct AssetSourceCatalog
        {
            std::unordered_map<std::string, AssetSourceCatalogEntry> ByLogicalName{};
            std::unordered_map<std::string, AssetSourceCatalogEntry> ByAssetId{};
        };

        /**
         * @brief Read one declared authored/imported asset id from a source file when present.
         * @param FilePath Candidate source file path.
         * @return Declared asset id string, or an empty string when the file does not author one.
         */
        [[nodiscard]] TExpected<std::string> ReadDeclaredAssetId(const std::filesystem::path& FilePath)
        {
            auto Text = ReadTextFile(FilePath);
            if (!Text)
            {
                return std::unexpected(Text.error());
            }

            Json Root = Json::parse(*Text, nullptr, false);
            if (Root.is_discarded() || !Root.is_object())
            {
                return std::string{};
            }

            if (const auto It = Root.find("AssetId"); It != Root.end() && It->is_string())
            {
                return It->get<std::string>();
            }
            if (const auto It = Root.find("assetId"); It != Root.end() && It->is_string())
            {
                return It->get<std::string>();
            }
            return std::string{};
        }

        /**
         * @brief Build one lookup catalog of supported source assets for dependency expansion.
         * @param Request Frozen build request.
         * @return Source catalog keyed by logical name and known asset ids.
         */
        [[nodiscard]] TExpected<AssetSourceCatalog> BuildAssetSourceCatalog(const ResolvedBuildRequest& Request)
        {
            auto AllAssets = EnumerateWholeProjectSelection(Request);
            if (!AllAssets)
            {
                return std::unexpected(AllAssets.error());
            }

            AssetSourceCatalog Catalog{};
            Catalog.ByLogicalName.reserve(AllAssets->size());
            Catalog.ByAssetId.reserve(AllAssets->size() * 2u);

            for (const AssetSelectionRecord& Record : *AllAssets)
            {
                if (!Record.Cookable)
                {
                    continue;
                }

                AssetSourceCatalogEntry Entry{
                    .LogicalName = Record.LogicalName,
                    .SourcePath = Record.SourcePath,
                };

                Entry.AssetId = SourceAssetIdFromLogicalName(Entry.LogicalName).ToString();
                Catalog.ByLogicalName.emplace(Entry.LogicalName, Entry);
                Catalog.ByAssetId.emplace(Entry.AssetId, Entry);

                auto DeclaredId = ReadDeclaredAssetId(Record.SourcePath);
                if (!DeclaredId)
                {
                    return std::unexpected(DeclaredId.error());
                }
                const std::string TrimmedDeclaredId = TrimCopy(*DeclaredId);
                if (!TrimmedDeclaredId.empty())
                {
                    Catalog.ByAssetId.emplace(TrimmedDeclaredId, Entry);
                }
            }

            return Catalog;
        }

        /**
         * @brief Resolve one cookable authored asset selector to a source-catalog entry.
         * @param Request Frozen build request.
         * @param Catalog Source catalog keyed by logical name and asset id.
         * @param Selector Logical asset name or authored asset id.
         * @param Context Stable selector context used in diagnostics.
         * @return Resolved source-catalog entry or a structured not-found error.
         *
         * This is used for project-authored startup assets such as the default render settings
         * asset, which are stored in descriptors by asset id rather than by logical source path.
         */
        [[nodiscard]] TExpected<AssetSourceCatalogEntry> ResolveCookableAssetSelector(
            const ResolvedBuildRequest& Request,
            const AssetSourceCatalog& Catalog,
            std::string_view Selector,
            const std::string_view Context)
        {
            const std::string NormalizedLogicalName = CanonicalizeAssetField(Request, Selector);
            if (!NormalizedLogicalName.empty())
            {
                if (const auto It = Catalog.ByLogicalName.find(NormalizedLogicalName); It != Catalog.ByLogicalName.end())
                {
                    return It->second;
                }
            }

            const std::string TrimmedSelector = TrimCopy(std::string(Selector));
            if (const ::SnAPI::AssetPipeline::AssetId ParsedAssetId = ::SnAPI::AssetPipeline::AssetId::FromString(TrimmedSelector);
                !ParsedAssetId.IsNull())
            {
                if (const auto It = Catalog.ByAssetId.find(ParsedAssetId.ToString()); It != Catalog.ByAssetId.end())
                {
                    return It->second;
                }
            }

            return std::unexpected(
                MakeError(EErrorCode::NotFound,
                          std::string(Context) + " asset could not be resolved from selector: " + TrimmedSelector));
        }

        /**
         * @brief Resolve one asset-pipeline dependency to a supported source asset.
         * @param Request Frozen build request.
         * @param Catalog Source catalog keyed by logical name and asset id.
         * @param Dependency Dependency reference to resolve.
         * @return Resolved source-catalog entry or a structured not-found error.
         */
        [[nodiscard]] TExpected<AssetSourceCatalogEntry> ResolveDependencySource(
            const ResolvedBuildRequest& Request,
            const AssetSourceCatalog& Catalog,
            const ::SnAPI::AssetPipeline::AssetDependencyRef& Dependency)
        {
            const std::string LogicalName = CanonicalizeAssetField(Request, Dependency.LogicalName);
            if (!LogicalName.empty())
            {
                if (const auto It = Catalog.ByLogicalName.find(LogicalName); It != Catalog.ByLogicalName.end())
                {
                    return It->second;
                }
            }

            if (!Dependency.Id.IsNull())
            {
                const std::string AssetId = Dependency.Id.ToString();
                if (const auto It = Catalog.ByAssetId.find(AssetId); It != Catalog.ByAssetId.end())
                {
                    return It->second;
                }
            }

            return std::unexpected(
                MakeError(EErrorCode::NotFound,
                          "Failed to resolve dependent asset '" +
                              (LogicalName.empty() ? Dependency.Id.ToString() : LogicalName) + "'"));
        }

        [[nodiscard]] TExpected<std::unique_ptr<::SnAPI::AssetPipeline::AssetPipelineEngine>>
        CreateAssetPipelineEngine(const ResolvedBuildRequest& Request, const std::filesystem::path& OutputPackPath);

        /**
         * @brief Expand included assets recursively using pipeline-reported semantic asset dependencies.
         * @param Request Frozen build request.
         * @param IncludedIndices Logical-name to selection-index map.
         * @param IncludedAssets Included asset records to expand in place.
         * @return Success or a structured dependency-resolution error.
         */
        [[nodiscard]] Result ExpandSelectedAssetDependencies(const ResolvedBuildRequest& Request,
                                                             std::unordered_map<std::string, std::size_t>& IncludedIndices,
                                                             std::vector<AssetSelectionRecord>& IncludedAssets)
        {
            if (Request.Profile.DependencyPolicy == EAssetDependencyPolicy::CustomResolver || IncludedAssets.empty())
            {
                return Ok();
            }

            auto Catalog = BuildAssetSourceCatalog(Request);
            if (!Catalog)
            {
                return std::unexpected(Catalog.error());
            }

            auto EngineResult = CreateAssetPipelineEngine(Request, {});
            if (!EngineResult)
            {
                return std::unexpected(EngineResult.error());
            }

            std::unique_ptr<::SnAPI::AssetPipeline::AssetPipelineEngine> Engine = std::move(*EngineResult);
            std::deque<std::string> Pending{};
            Pending.resize(IncludedAssets.size());
            std::ranges::transform(IncludedAssets, Pending.begin(), &AssetSelectionRecord::LogicalName);
            std::unordered_set<std::string> Expanded{};

            while (!Pending.empty())
            {
                const std::string LogicalName = std::move(Pending.front());
                Pending.pop_front();
                if (!Expanded.insert(LogicalName).second)
                {
                    continue;
                }

                const auto ExistingIt = IncludedIndices.find(LogicalName);
                if (ExistingIt == IncludedIndices.end())
                {
                    continue;
                }

                const AssetSelectionRecord& ParentRecord = IncludedAssets[ExistingIt->second];
                if (!ParentRecord.Cookable)
                {
                    continue;
                }
                auto AnalysisResult = Engine->AnalyzeSource(ParentRecord.SourcePath.string(), ParentRecord.LogicalName);
                if (!AnalysisResult)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError,
                                  "Failed to analyze source asset dependencies for '" + ParentRecord.LogicalName +
                                      "': " + AnalysisResult.error()));
                }

                for (const auto& Dependency : AnalysisResult->AssetDependencies)
                {
                    if (!ShouldIncludeDependency(Request.Profile.DependencyPolicy, Dependency.Kind))
                    {
                        continue;
                    }

                    auto DependencySource = ResolveDependencySource(Request, *Catalog, Dependency);
                    if (!DependencySource)
                    {
                        if (Dependency.Kind == ::SnAPI::AssetPipeline::EAssetDependencyKind::Required)
                        {
                            return std::unexpected(DependencySource.error());
                        }
                        continue;
                    }

                    AssetSelectionRecord DependencyRecord = MakeSelectionRecord(
                        DependencySource->LogicalName,
                        DependencySource->SourcePath,
                        MakeDependencySelectionReason(Dependency.Kind),
                        ParentRecord.LogicalName,
                        false);
                    AppendUniqueProvenance(
                        DependencyRecord,
                        "DependencyKind",
                        ToString(Dependency.Kind),
                        true);
                    const std::size_t PreviousCount = IncludedAssets.size();
                    AddOrMergeSelection(std::move(DependencyRecord), IncludedIndices, IncludedAssets);
                    if (IncludedAssets.size() != PreviousCount ||
                        !Expanded.contains(DependencySource->LogicalName))
                    {
                        Pending.push_back(DependencySource->LogicalName);
                    }
                }
            }

            return Ok();
        }

        /**
         * @brief Sanitize one chunk-id component for use in file names and manifest ids.
         * @param Text Raw chunk-id component.
         * @return Sanitized identifier text.
         */
        [[nodiscard]] std::string SanitizeChunkComponent(std::string_view Text)
        {
            std::string Value = NormalizeLogicalName(Text);
            if (Value.empty())
            {
                return "Root";
            }

            for (char& Character : Value)
            {
                const bool IsAlphaNumeric = std::isalnum(static_cast<unsigned char>(Character)) != 0;
                if (!IsAlphaNumeric)
                {
                    Character = '_';
                }
            }

            while (!Value.empty() && Value.front() == '_')
            {
                Value.erase(Value.begin());
            }
            while (!Value.empty() && Value.back() == '_')
            {
                Value.pop_back();
            }

            return Value.empty() ? std::string("Root") : Value;
        }

        /**
         * @brief Build the chunk id that should own one selected asset.
         * @param Request Frozen build request.
         * @param Record Selected asset record.
         * @param SelectedLevels Canonical selected-level set.
         * @return Stable chunk identifier.
         */
        [[nodiscard]] std::string ResolveChunkId(const ResolvedBuildRequest& Request,
                                                 const AssetSelectionRecord& Record,
                                                 const std::unordered_set<std::string>& SelectedLevels)
        {
            switch (Request.Profile.ChunkStrategy)
            {
            case EAssetChunkStrategy::Monolithic:
            case EAssetChunkStrategy::CustomGraph:
                return "Primary";

            case EAssetChunkStrategy::SharedPlusPerLevel:
            {
                if (SelectedLevels.contains(Record.LogicalName))
                {
                    return "Level_" + SanitizeChunkComponent(Record.LogicalName);
                }
                return "Shared";
            }

            case EAssetChunkStrategy::PerLabel:
            {
                const std::filesystem::path LogicalPath = std::filesystem::path(Record.LogicalName);
                const std::filesystem::path Parent = LogicalPath.parent_path();
                const std::string Group = Parent.empty() ? std::string("Root") : Parent.begin()->string();
                return "Label_" + SanitizeChunkComponent(Group);
            }
            }

            return "Primary";
        }

        /**
         * @brief Build the canonical `.snpak` file name for one chunk.
         * @param Request Frozen build request.
         * @param ChunkId Stable chunk identifier.
         * @return Bundle file name.
         */
        [[nodiscard]] std::string MakeSnpakName(const ResolvedBuildRequest& Request, std::string_view ChunkId)
        {
            return Request.Project.Descriptor.Project.Name + "_" + Request.Profile.Platform + "_" +
                   SanitizeChunkComponent(ChunkId) + ".snpak";
        }

        /**
         * @brief Build a deterministic chunk plan from one included asset set.
         * @param Request Frozen build request.
         * @param IncludedAssets Ordered included asset set.
         * @return Deterministic chunk plan entries.
         */
        [[nodiscard]] std::vector<AssetChunkPlanEntry> BuildChunkPlan(const ResolvedBuildRequest& Request,
                                                                      const std::vector<AssetSelectionRecord>& IncludedAssets)
        {
            std::unordered_map<std::string, std::size_t> ChunkIndices{};
            std::vector<AssetChunkPlanEntry> Plan{};

            for (const AssetSelectionRecord& Record : IncludedAssets)
            {
                if (Record.ChunkId.empty())
                {
                    continue;
                }

                auto [It, Inserted] = ChunkIndices.emplace(Record.ChunkId, Plan.size());
                if (Inserted)
                {
                    Plan.push_back(AssetChunkPlanEntry{
                        .ChunkId = Record.ChunkId,
                        .OutputFileName = MakeSnpakName(Request, Record.ChunkId),
                    });
                }

                Plan[It->second].AssetLogicalNames.push_back(Record.LogicalName);
            }

            for (AssetChunkPlanEntry& Entry : Plan)
            {
                std::ranges::sort(Entry.AssetLogicalNames);
            }

            std::ranges::sort(Plan, [](const AssetChunkPlanEntry& Left, const AssetChunkPlanEntry& Right) {
                return Left.ChunkId < Right.ChunkId;
            });
            return Plan;
        }

        /**
         * @brief Serialize one provenance entry into ordered JSON.
         * @param Entry Provenance entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeProvenance(const AssetSelectionProvenanceEntry& Entry)
        {
            return Json::object({
                {"Kind", Entry.Kind},
                {"Value", Entry.Value},
                {"Included", Entry.Included},
            });
        }

        /**
         * @brief Serialize one cooked-asset dependency entry into ordered JSON.
         * @param Entry Dependency entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeCookedDependency(const CookedAssetRecord::DependencyRecord& Entry)
        {
            return Json::object({
                {"AssetId", Entry.AssetId},
                {"LogicalName", Entry.LogicalName},
                {"Kind", Entry.Kind},
            });
        }

        /**
         * @brief Parse one cooked-asset dependency entry from ordered JSON.
         * @param Entry Dependency JSON object.
         * @return Parsed dependency record.
         */
        [[nodiscard]] CookedAssetRecord::DependencyRecord ParseCookedDependency(const Json& Entry)
        {
            return CookedAssetRecord::DependencyRecord{
                .AssetId = Entry.value("AssetId", std::string{}),
                .LogicalName = Entry.value("LogicalName", std::string{}),
                .Kind = Entry.value("Kind", std::string{}),
            };
        }

        /**
         * @brief Serialize one selection record into ordered JSON.
         * @param Record Selection record to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeSelectionRecord(const AssetSelectionRecord& Record)
        {
            Json Provenance = Json::array();
            for (const AssetSelectionProvenanceEntry& Entry : Record.Provenance)
            {
                Provenance.push_back(SerializeProvenance(Entry));
            }

            return Json::object({
                {"LogicalName", Record.LogicalName},
                {"SourcePath", NormalizePathString(Record.SourcePath)},
                {"AssetKindLabel", Record.AssetKindLabel},
                {"Reason", Record.SelectionReason},
                {"ExplicitSelection", Record.ExplicitSelection},
                {"Cookable", Record.Cookable},
                {"StageVerbatim", Record.StageVerbatim},
                {"ChunkId", Record.ChunkId},
                {"Provenance", std::move(Provenance)},
            });
        }

        /**
         * @brief Serialize one chunk-plan entry into ordered JSON.
         * @param Entry Chunk-plan entry to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeChunkPlanEntry(const AssetChunkPlanEntry& Entry)
        {
            return Json::object({
                {"ChunkId", Entry.ChunkId},
                {"OutputFileName", Entry.OutputFileName},
                {"AssetLogicalNames", Entry.AssetLogicalNames},
            });
        }

        /**
         * @brief Parse one provenance entry from JSON.
         * @param Value JSON value to parse.
         * @return Parsed provenance entry.
         */
        [[nodiscard]] AssetSelectionProvenanceEntry ParseProvenance(const Json& Value)
        {
            return AssetSelectionProvenanceEntry{
                .Kind = Value.value("Kind", std::string{}),
                .Value = Value.value("Value", std::string{}),
                .Included = Value.value("Included", true),
            };
        }

        /**
         * @brief Parse one selection-record array from JSON.
         * @param Array JSON array to parse.
         * @return Parsed ordered records.
         */
        [[nodiscard]] std::vector<AssetSelectionRecord> ParseSelectionArray(const Json& Array)
        {
            std::vector<AssetSelectionRecord> Records{};
            if (!Array.is_array())
            {
                return Records;
            }

            for (const Json& Entry : Array)
            {
                if (!Entry.is_object())
                {
                    continue;
                }

                AssetSelectionRecord Record{
                    .LogicalName = Entry.value("LogicalName", std::string{}),
                    .SourcePath = std::filesystem::path(Entry.value("SourcePath", std::string{})).lexically_normal(),
                    .AssetKindLabel = Entry.value("AssetKindLabel", std::string{}),
                    .SelectionReason = Entry.value("Reason", std::string{}),
                    .ExplicitSelection = Entry.value("ExplicitSelection", false),
                    .Cookable = Entry.value("Cookable", true),
                    .StageVerbatim = Entry.value("StageVerbatim", false),
                    .ChunkId = Entry.value("ChunkId", std::string{}),
                };
                if (const auto ProvenanceIt = Entry.find("Provenance"); ProvenanceIt != Entry.end() && ProvenanceIt->is_array())
                {
                    for (const Json& ProvenanceEntry : *ProvenanceIt)
                    {
                        if (ProvenanceEntry.is_object())
                        {
                            Record.Provenance.push_back(ParseProvenance(ProvenanceEntry));
                        }
                    }
                }
                Records.push_back(std::move(Record));
            }

            std::ranges::sort(Records, {}, &AssetSelectionRecord::LogicalName);
            return Records;
        }

        /**
         * @brief Copy selected verbatim package files into one asset-artifact directory.
         * @param Selection Ordered selection plan records.
         * @param DestinationDirectory Asset-artifact directory that will later be staged to `Assets/`.
         * @return Copied file paths.
         */
        [[nodiscard]] TExpected<std::vector<std::string>>
        CopyVerbatimSelectedAssets(const std::vector<AssetSelectionRecord>& Selection,
                                   const std::filesystem::path& DestinationDirectory)
        {
            std::vector<std::string> Outputs{};
            for (const AssetSelectionRecord& Record : Selection)
            {
                if (!Record.StageVerbatim)
                {
                    continue;
                }

                const std::filesystem::path DestinationPath =
                    (DestinationDirectory / std::filesystem::path(Record.LogicalName)).lexically_normal();
                if (Result DirectoryResult = EnsureDirectory(DestinationPath.parent_path()); !DirectoryResult)
                {
                    return std::unexpected(DirectoryResult.error());
                }

                std::error_code Error{};
                std::filesystem::copy_file(
                    Record.SourcePath, DestinationPath, std::filesystem::copy_options::overwrite_existing, Error);
                if (Error)
                {
                    return std::unexpected(MakeError(
                        EErrorCode::InternalError,
                        "Failed to copy auxiliary package asset '" + Record.SourcePath.string() + "': " + Error.message()));
                }

                Outputs.push_back(NormalizePathString(DestinationPath));
            }

            std::ranges::sort(Outputs);
            return Outputs;
        }

        /**
         * @brief Serialize one selection plan into canonical JSON.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Plan Resolved selection plan.
         * @return Ordered JSON document.
         */
        [[nodiscard]] Json BuildSelectionJson(const ResolvedBuildRequest& Request,
                                              const BuildGraph& Graph,
                                              const AssetSelectionPlan& Plan)
        {
            Json IncludedAssets = Json::array();
            for (const AssetSelectionRecord& Record : Plan.IncludedAssets)
            {
                IncludedAssets.push_back(SerializeSelectionRecord(Record));
            }

            Json ExcludedAssets = Json::array();
            for (const AssetSelectionRecord& Record : Plan.ExcludedAssets)
            {
                ExcludedAssets.push_back(SerializeSelectionRecord(Record));
            }

            Json ChunkPlan = Json::array();
            for (const AssetChunkPlanEntry& Entry : BuildChunkPlan(Request, Plan.IncludedAssets))
            {
                ChunkPlan.push_back(SerializeChunkPlanEntry(Entry));
            }

            return Json::object({
                {"BuildId", Graph.BuildId},
                {"ProjectName", Request.Project.Descriptor.Project.Name},
                {"Platform", Request.Profile.Platform},
                {"Configuration", ToString(Request.Profile.Configuration)},
                {"ChunkStrategy", ToString(Request.Profile.ChunkStrategy)},
                {"Assets", IncludedAssets},
                {"IncludedAssets", std::move(IncludedAssets)},
                {"ExcludedAssets", std::move(ExcludedAssets)},
                {"ChunkPlan", std::move(ChunkPlan)},
            });
        }

        /**
         * @brief Parse one selected-asset JSON artifact into a full selection plan.
         * @param FilePath Selection artifact path.
         * @return Parsed selection plan or a structured parse error.
         */
        [[nodiscard]] TExpected<AssetSelectionPlan>
        ParseSelectionFile(const std::filesystem::path& FilePath)
        {
            auto Text = ReadTextFile(FilePath);
            if (!Text)
            {
                return std::unexpected(Text.error());
            }

            Json Root = Json::parse(*Text, nullptr, false);
            if (Root.is_discarded() || !Root.is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Failed to parse asset selection file: " + FilePath.string()));
            }

            AssetSelectionPlan Plan{};
            if (const auto IncludedIt = Root.find("IncludedAssets"); IncludedIt != Root.end() && IncludedIt->is_array())
            {
                Plan.IncludedAssets = ParseSelectionArray(*IncludedIt);
            }
            else if (const auto AssetsIt = Root.find("Assets"); AssetsIt != Root.end() && AssetsIt->is_array())
            {
                Plan.IncludedAssets = ParseSelectionArray(*AssetsIt);
            }

            if (const auto ExcludedIt = Root.find("ExcludedAssets"); ExcludedIt != Root.end() && ExcludedIt->is_array())
            {
                Plan.ExcludedAssets = ParseSelectionArray(*ExcludedIt);
            }

            return Plan;
        }

        /**
         * @brief Serialize cooked-asset records into one canonical JSON index.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Records Cooked asset records.
         * @return Ordered JSON document.
         */
        [[nodiscard]] Json BuildCookIndexJson(const ResolvedBuildRequest& Request,
                                              const BuildGraph& Graph,
                                              const std::vector<CookedAssetRecord>& Records)
        {
            Json Assets = Json::array();
            for (const CookedAssetRecord& Record : Records)
            {
                Json Provenance = Json::array();
                for (const AssetSelectionProvenanceEntry& Entry : Record.Provenance)
                {
                    Provenance.push_back(SerializeProvenance(Entry));
                }

                Json Dependencies = Json::array();
                for (const CookedAssetRecord::DependencyRecord& Entry : Record.Dependencies)
                {
                    Dependencies.push_back(SerializeCookedDependency(Entry));
                }

                Assets.push_back(Json::object({
                    {"LogicalName", Record.LogicalName},
                    {"SourcePath", NormalizePathString(Record.SourcePath)},
                    {"SelectionReason", Record.SelectionReason},
                    {"AssetId", Record.AssetId},
                    {"AssetKind", Record.AssetKind},
                    {"CookedPayloadType", Record.CookedPayloadType},
                    {"CookedPayloadSize", Record.CookedPayloadSize},
                    {"BulkChunkCount", Record.BulkChunkCount},
                    {"ChunkId", Record.ChunkId},
                    {"SourceContentHash", Record.SourceContentHash},
                    {"SettingsHash", Record.SettingsHash},
                    {"Dependencies", std::move(Dependencies)},
                    {"Provenance", std::move(Provenance)},
                }));
            }

            Json ChunkPlan = Json::array();
            for (const AssetChunkPlanEntry& Entry : BuildChunkPlan(
                     Request,
                     [&]() {
                         std::vector<AssetSelectionRecord> RecordsForPlan{};
                         RecordsForPlan.reserve(Records.size());
                         for (const CookedAssetRecord& Record : Records)
                         {
                            RecordsForPlan.push_back(AssetSelectionRecord{
                                .LogicalName = Record.LogicalName,
                                .SourcePath = Record.SourcePath,
                                .AssetKindLabel = Record.AssetKind,
                                .SelectionReason = Record.SelectionReason,
                                .Cookable = true,
                                .ChunkId = Record.ChunkId,
                            });
                         }
                         return RecordsForPlan;
                     }()))
            {
                ChunkPlan.push_back(SerializeChunkPlanEntry(Entry));
            }

            return Json::object({
                {"BuildId", Graph.BuildId},
                {"ProjectName", Request.Project.Descriptor.Project.Name},
                {"Platform", Request.Profile.Platform},
                {"Configuration", ToString(Request.Profile.Configuration)},
                {"ChunkStrategy", ToString(Request.Profile.ChunkStrategy)},
                {"Assets", std::move(Assets)},
                {"ChunkPlan", std::move(ChunkPlan)},
            });
        }

        /**
         * @brief Parse one cooked-asset index file into typed records.
         * @param FilePath Cooked-asset index path.
         * @return Typed cooked asset records or a structured parse error.
         */
        [[nodiscard]] TExpected<std::vector<CookedAssetRecord>>
        ParseCookIndexFile(const std::filesystem::path& FilePath)
        {
            auto Text = ReadTextFile(FilePath);
            if (!Text)
            {
                return std::unexpected(Text.error());
            }

            Json Root = Json::parse(*Text, nullptr, false);
            if (Root.is_discarded() || !Root.is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Failed to parse cooked-asset index: " + FilePath.string()));
            }

            std::vector<CookedAssetRecord> Records{};
            if (const auto AssetsIt = Root.find("Assets"); AssetsIt != Root.end() && AssetsIt->is_array())
            {
                for (const Json& Entry : *AssetsIt)
                {
                    if (!Entry.is_object())
                    {
                        continue;
                    }

                    Records.push_back(CookedAssetRecord{
                        .LogicalName = Entry.value("LogicalName", std::string{}),
                        .SourcePath = std::filesystem::path(Entry.value("SourcePath", std::string{})).lexically_normal(),
                        .SelectionReason = Entry.value("SelectionReason", std::string{}),
                        .AssetId = Entry.value("AssetId", std::string{}),
                        .AssetKind = Entry.value("AssetKind", std::string{}),
                        .CookedPayloadType = Entry.value("CookedPayloadType", std::string{}),
                        .CookedPayloadSize = Entry.value("CookedPayloadSize", static_cast<std::uint64_t>(0u)),
                        .BulkChunkCount = Entry.value("BulkChunkCount", static_cast<std::uint32_t>(0u)),
                        .ChunkId = Entry.value("ChunkId", std::string{}),
                        .SourceContentHash = Entry.value("SourceContentHash", std::string{}),
                        .SettingsHash = Entry.value("SettingsHash", std::string{}),
                    });

                    if (const auto ProvenanceIt = Entry.find("Provenance");
                        ProvenanceIt != Entry.end() && ProvenanceIt->is_array())
                    {
                        for (const Json& ProvenanceEntry : *ProvenanceIt)
                        {
                            if (ProvenanceEntry.is_object())
                            {
                                Records.back().Provenance.push_back(ParseProvenance(ProvenanceEntry));
                            }
                        }
                    }
                    if (const auto DependenciesIt = Entry.find("Dependencies");
                        DependenciesIt != Entry.end() && DependenciesIt->is_array())
                    {
                        for (const Json& DependencyEntry : *DependenciesIt)
                        {
                            if (DependencyEntry.is_object())
                            {
                                Records.back().Dependencies.push_back(ParseCookedDependency(DependencyEntry));
                            }
                        }
                    }
                }
            }

            std::ranges::sort(Records, {}, &CookedAssetRecord::LogicalName);
            return Records;
        }

        /**
         * @brief Create one initialized asset-pipeline engine for build packaging.
         * @param Request Frozen build request.
         * @param OutputPackPath Optional output `.snpak` path.
         * @return Initialized engine or a structured initialization error.
         */
        [[nodiscard]] TExpected<std::unique_ptr<::SnAPI::AssetPipeline::AssetPipelineEngine>>
        CreateAssetPipelineEngine(const ResolvedBuildRequest& Request, const std::filesystem::path& OutputPackPath)
        {
            ::SnAPI::AssetPipeline::PipelineBuildConfig Config{};
            Config.SourceRoots = {Request.Project.AssetRootDirectory.string()};
            Config.OutputPackPath = OutputPackPath.string();
            Config.bDeterministicAssetIds = true;
            Config.bEnableAppendUpdates = true;
            Config.bVerbose = false;

            auto Engine = std::make_unique<::SnAPI::AssetPipeline::AssetPipelineEngine>();
            if (auto InitResult = Engine->Initialize(Config); !InitResult.has_value())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to initialize asset pipeline engine: " + InitResult.error()));
            }

            RegisterGameFrameworkPipeline(*Engine);
            return Engine;
        }

        /**
         * @brief Cook one ordered asset-selection set through `AssetPipelineEngine`.
         * @param Request Frozen build request.
         * @param Selection Ordered selected source records.
         * @param OutputPackPath Optional output `.snpak` path to write via `SaveAll()`.
         * @return Ordered cooked asset records or a structured cook/save error.
         */
        [[nodiscard]] TExpected<std::vector<CookedAssetRecord>> CookSelectedAssets(
            const ResolvedBuildRequest& Request,
            const std::vector<AssetSelectionRecord>& Selection,
            const std::filesystem::path& OutputPackPath = {})
        {
            auto EngineResult = CreateAssetPipelineEngine(Request, OutputPackPath);
            if (!EngineResult)
            {
                return std::unexpected(EngineResult.error());
            }

            std::unique_ptr<::SnAPI::AssetPipeline::AssetPipelineEngine> Engine = std::move(*EngineResult);
            std::vector<CookedAssetRecord> Records{};
            Records.reserve(Selection.size());

            for (const AssetSelectionRecord& Record : Selection)
            {
                if (!Record.Cookable)
                {
                    continue;
                }

                auto SourceHash = HashFileContents(Record.SourcePath);
                if (!SourceHash)
                {
                    return std::unexpected(SourceHash.error());
                }

                auto ProcessResult = Engine->ProcessSource(Record.SourcePath.string(), Record.LogicalName);
                if (!ProcessResult.has_value())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError,
                                  "Failed to cook asset '" + Record.LogicalName + "': " + ProcessResult.error()));
                }

                auto CookedResult = Engine->GetCookedAsset(Record.LogicalName);
                if (!CookedResult.has_value())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError,
                                  "Cooked asset record is not available for '" + Record.LogicalName + "': " +
                                      CookedResult.error()));
                }

                const ::SnAPI::AssetPipeline::CookedAsset& Cooked = CookedResult->get();
                std::vector<CookedAssetRecord::DependencyRecord> Dependencies{};
                Dependencies.reserve(Cooked.AssetDependencies.size());
                for (const auto& Dependency : Cooked.AssetDependencies)
                {
                    Dependencies.push_back(CookedAssetRecord::DependencyRecord{
                        .AssetId = Dependency.Id.IsNull() ? std::string{} : Dependency.Id.ToString(),
                        .LogicalName = Dependency.LogicalName,
                        .Kind = ToString(Dependency.Kind),
                    });
                }
                Records.push_back(CookedAssetRecord{
                    .LogicalName = Record.LogicalName,
                    .SourcePath = Record.SourcePath,
                    .SelectionReason = Record.SelectionReason,
                    .AssetId = Cooked.Id.ToString(),
                    .AssetKind = Cooked.AssetKind.ToString(),
                    .CookedPayloadType = Cooked.Cooked.PayloadType.ToString(),
                    .CookedPayloadSize = static_cast<std::uint64_t>(Cooked.Cooked.Bytes.size()),
                    .BulkChunkCount = static_cast<std::uint32_t>(Cooked.Bulk.size()),
                    .ChunkId = Record.ChunkId,
                    .SourceContentHash = std::move(*SourceHash),
                    .SettingsHash = Request.RequestHash,
                    .Dependencies = std::move(Dependencies),
                    .Provenance = Record.Provenance,
                });
            }

            if (!OutputPackPath.empty())
            {
                if (Result DirectoryResult = EnsureDirectory(OutputPackPath.parent_path()); !DirectoryResult)
                {
                    return std::unexpected(DirectoryResult.error());
                }

                auto SaveResult = Engine->SaveAll();
                if (!SaveResult.has_value())
                {
                    return std::unexpected(MakeError(
                        EErrorCode::InternalError,
                        "Failed to write asset bundle '" + OutputPackPath.string() + "': " + SaveResult.error()));
                }
            }

            std::ranges::sort(Records, {}, &CookedAssetRecord::LogicalName);
            return Records;
        }

        /**
         * @brief Build one cook manifest JSON document from cooked asset records.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Records Cooked asset records.
         * @return Ordered JSON document.
         */
        [[nodiscard]] Json BuildCookManifestJson(const ResolvedBuildRequest& Request,
                                                 const BuildGraph& Graph,
                                                 const std::vector<CookedAssetRecord>& Records)
        {
            Json Assets = Json::array();
            for (const CookedAssetRecord& Record : Records)
            {
                Json Provenance = Json::array();
                for (const AssetSelectionProvenanceEntry& Entry : Record.Provenance)
                {
                    Provenance.push_back(SerializeProvenance(Entry));
                }

                Json Dependencies = Json::array();
                for (const CookedAssetRecord::DependencyRecord& Entry : Record.Dependencies)
                {
                    Dependencies.push_back(SerializeCookedDependency(Entry));
                }

                Assets.push_back(Json::object({
                    {"LogicalName", Record.LogicalName},
                    {"SourcePath", NormalizePathString(Record.SourcePath)},
                    {"SelectionReason", Record.SelectionReason},
                    {"AssetId", Record.AssetId},
                    {"AssetKind", Record.AssetKind},
                    {"CookedPayloadType", Record.CookedPayloadType},
                    {"CookedPayloadSize", Record.CookedPayloadSize},
                    {"BulkChunkCount", Record.BulkChunkCount},
                    {"ChunkId", Record.ChunkId},
                    {"SourceContentHash", Record.SourceContentHash},
                    {"SettingsHash", Record.SettingsHash},
                    {"Dependencies", std::move(Dependencies)},
                    {"Provenance", std::move(Provenance)},
                }));
            }

            Json SnpakFiles = Json::array();
            std::unordered_map<std::string, std::uint64_t> ChunkAssetCounts{};
            for (const CookedAssetRecord& Record : Records)
            {
                ++ChunkAssetCounts[Record.ChunkId.empty() ? std::string("Primary") : Record.ChunkId];
            }

            std::vector<std::string> ChunkIds{};
            ChunkIds.reserve(ChunkAssetCounts.size());
            for (const auto& [ChunkId, _] : ChunkAssetCounts)
            {
                ChunkIds.push_back(ChunkId);
            }
            std::ranges::sort(ChunkIds);
            for (const std::string& ChunkId : ChunkIds)
            {
                SnpakFiles.push_back(Json::object({
                    {"Name", MakeSnpakName(Request, ChunkId)},
                    {"ChunkId", ChunkId},
                    {"ContainedAssetCount", ChunkAssetCounts[ChunkId]},
                }));
            }

            return Json::object({
                {"BuildId", Graph.BuildId},
                {"ProjectName", Request.Project.Descriptor.Project.Name},
                {"ProfileName", Request.ProfileName.empty() ? std::string("AdHoc") : Request.ProfileName},
                {"TargetPlatform", Request.Profile.Platform},
                {"Configuration", ToString(Request.Profile.Configuration)},
                {"ChunkStrategy", ToString(Request.Profile.ChunkStrategy)},
                {"IncludedLevels", Request.Profile.SelectedLevels},
                {"Assets", std::move(Assets)},
                {"SnpakFiles", std::move(SnpakFiles)},
            });
        }
    } // namespace

    TExpected<AssetSelectionPlan> AssetCookServiceAdapter::ResolveAssetSelection(const ResolvedBuildRequest& Request)
    {
        std::unordered_map<std::string, std::size_t> IncludedIndices{};
        std::vector<AssetSelectionRecord> IncludedAssets{};
        std::optional<AssetSourceCatalog> StartupAssetCatalog{};

        const auto EnsureStartupAssetCatalog = [&]() -> TExpected<const AssetSourceCatalog*>
        {
            if (!StartupAssetCatalog.has_value())
            {
                auto Catalog = BuildAssetSourceCatalog(Request);
                if (!Catalog)
                {
                    return std::unexpected(Catalog.error());
                }
                StartupAssetCatalog = std::move(*Catalog);
            }

            return std::addressof(*StartupAssetCatalog);
        };

        for (const std::string& SelectedLevel : Request.Profile.SelectedLevels)
        {
            auto SourcePath = ResolveSelectedSourcePath(Request, SelectedLevel);
            if (!SourcePath)
            {
                return std::unexpected(SourcePath.error());
            }

            AddOrMergeSelection(
                MakeSelectionRecord(CanonicalizeAssetField(Request, SelectedLevel), *SourcePath, "SelectedLevel",
                                    CanonicalizeAssetField(Request, SelectedLevel), true),
                IncludedIndices, IncludedAssets);
        }

        for (const std::string& ExplicitAsset : Request.Profile.ExplicitAssets)
        {
            auto SourcePath = ResolveSelectedSourcePath(Request, ExplicitAsset);
            if (!SourcePath)
            {
                return std::unexpected(SourcePath.error());
            }

            AddOrMergeSelection(
                MakeSelectionRecord(CanonicalizeAssetField(Request, ExplicitAsset), *SourcePath, "ExplicitAsset",
                                    CanonicalizeAssetField(Request, ExplicitAsset), true),
                IncludedIndices, IncludedAssets);
        }

        for (const std::string& IncludeFolder : Request.Profile.IncludeFolders)
        {
            auto FolderRecords = EnumerateFolderSelection(Request, IncludeFolder);
            if (!FolderRecords)
            {
                return std::unexpected(FolderRecords.error());
            }

            for (AssetSelectionRecord& Record : *FolderRecords)
            {
                AppendUniqueProvenance(Record, "IncludeFolder", CanonicalizeAssetField(Request, IncludeFolder), true);
                AddOrMergeSelection(std::move(Record), IncludedIndices, IncludedAssets);
            }
        }

        for (const std::string& IncludeKind : Request.Profile.IncludeAssetKinds)
        {
            auto KindRecords = EnumerateKindSelection(Request, IncludeKind);
            if (!KindRecords)
            {
                return std::unexpected(KindRecords.error());
            }

            for (AssetSelectionRecord& Record : *KindRecords)
            {
                AddOrMergeSelection(std::move(Record), IncludedIndices, IncludedAssets);
            }
        }

        const std::string DefaultRenderSettingsSelector =
            TrimCopy(Request.Project.Descriptor.Startup.DefaultRenderSettingsAssetId);
        if (!DefaultRenderSettingsSelector.empty())
        {
            auto Catalog = EnsureStartupAssetCatalog();
            if (!Catalog)
            {
                return std::unexpected(Catalog.error());
            }

            auto DefaultSettingsEntry =
                ResolveCookableAssetSelector(Request, **Catalog, DefaultRenderSettingsSelector, "Default render settings");
            if (!DefaultSettingsEntry)
            {
                return std::unexpected(DefaultSettingsEntry.error());
            }

            AddOrMergeSelection(
                MakeSelectionRecord(DefaultSettingsEntry->LogicalName,
                                    DefaultSettingsEntry->SourcePath,
                                    "DefaultRenderSettings",
                                    DefaultRenderSettingsSelector,
                                    false),
                IncludedIndices,
                IncludedAssets);
        }

        if (IncludedAssets.empty())
        {
            const std::string StartupLogicalName =
                CanonicalizeAssetField(Request, Request.Project.Descriptor.Startup.StartupLevelAsset);
            if (!StartupLogicalName.empty())
            {
                auto SourcePath = ResolveSelectedSourcePath(Request, StartupLogicalName);
                if (SourcePath)
                {
                    AddOrMergeSelection(MakeSelectionRecord(StartupLogicalName, *SourcePath, "StartupLevel",
                                                            StartupLogicalName, false),
                                        IncludedIndices, IncludedAssets);
                }
            }
        }

        if (IncludedAssets.empty())
        {
            auto WholeProjectRecords = EnumerateWholeProjectSelection(Request);
            if (!WholeProjectRecords)
            {
                return std::unexpected(WholeProjectRecords.error());
            }

            for (AssetSelectionRecord& Record : *WholeProjectRecords)
            {
                AppendUniqueProvenance(Record, "WholeProject", Record.LogicalName, true);
                AddOrMergeSelection(std::move(Record), IncludedIndices, IncludedAssets);
            }
        }

        if (Result DependencyResult = ExpandSelectedAssetDependencies(Request, IncludedIndices, IncludedAssets);
            !DependencyResult)
        {
            return std::unexpected(DependencyResult.error());
        }

        std::vector<std::string> ExcludeRules{};
        ExcludeRules.reserve(Request.Profile.ExcludeFolders.size());
        for (const std::string& ExcludeFolder : Request.Profile.ExcludeFolders)
        {
            const std::string CanonicalRule = CanonicalizeAssetField(Request, ExcludeFolder);
            if (!CanonicalRule.empty())
            {
                ExcludeRules.push_back(CanonicalRule);
            }
        }

        std::vector<std::string> ExcludeKindRules{};
        ExcludeKindRules.reserve(Request.Profile.ExcludeAssetKinds.size());
        for (const std::string& ExcludeKind : Request.Profile.ExcludeAssetKinds)
        {
            const std::string CanonicalRule = TrimCopy(ExcludeKind);
            if (!CanonicalRule.empty())
            {
                ExcludeKindRules.push_back(CanonicalRule);
            }
        }

        AssetSelectionPlan Plan{};
        if (ExcludeRules.empty() && ExcludeKindRules.empty())
        {
            Plan.IncludedAssets = std::move(IncludedAssets);
        }
        else
        {
            for (AssetSelectionRecord& Record : IncludedAssets)
            {
                std::vector<std::string> MatchedRules{};
                for (const std::string& Rule : ExcludeRules)
                {
                    if (LogicalNameMatchesFolderRule(Record.LogicalName, Rule))
                    {
                        MatchedRules.push_back(Rule);
                    }
                }

                std::vector<std::string> MatchedKindRules{};
                for (const std::string& Rule : ExcludeKindRules)
                {
                    if (PackageSourceMatchesKindRule(DescribePackageSource(Record.SourcePath), Record.SourcePath, Rule))
                    {
                        MatchedKindRules.push_back(Rule);
                    }
                }

                if (MatchedRules.empty() && MatchedKindRules.empty())
                {
                    Plan.IncludedAssets.push_back(std::move(Record));
                    continue;
                }

                if (Record.ExplicitSelection && Request.Profile.AllowExplicitOverrideExcludes)
                {
                    for (const std::string& Rule : MatchedRules)
                    {
                        AppendUniqueProvenance(Record, "ExcludeFolderIgnoredByExplicitOverride", Rule, true);
                    }
                    for (const std::string& Rule : MatchedKindRules)
                    {
                        AppendUniqueProvenance(Record, "ExcludeAssetKindIgnoredByExplicitOverride", Rule, true);
                    }
                    Plan.IncludedAssets.push_back(std::move(Record));
                    continue;
                }

                for (const std::string& Rule : MatchedRules)
                {
                    AppendUniqueProvenance(Record, "ExcludeFolder", Rule, false);
                }
                for (const std::string& Rule : MatchedKindRules)
                {
                    AppendUniqueProvenance(Record, "ExcludeAssetKind", Rule, false);
                }
                Plan.ExcludedAssets.push_back(std::move(Record));
            }
        }

        std::unordered_set<std::string> SelectedLevels{};
        SelectedLevels.reserve(Request.Profile.SelectedLevels.size());
        for (const std::string& SelectedLevel : Request.Profile.SelectedLevels)
        {
            const std::string CanonicalLevel = CanonicalizeAssetField(Request, SelectedLevel);
            if (!CanonicalLevel.empty())
            {
                SelectedLevels.insert(CanonicalLevel);
            }
        }

        if (SelectedLevels.empty())
        {
            const std::string StartupLevel = CanonicalizeAssetField(Request, Request.Project.Descriptor.Startup.StartupLevelAsset);
            if (!StartupLevel.empty())
            {
                SelectedLevels.insert(StartupLevel);
            }
        }

        for (AssetSelectionRecord& Record : Plan.IncludedAssets)
        {
            Record.ChunkId = Record.Cookable ? ResolveChunkId(Request, Record, SelectedLevels) : std::string{};
        }
        for (AssetSelectionRecord& Record : Plan.ExcludedAssets)
        {
            Record.ChunkId = Record.Cookable ? ResolveChunkId(Request, Record, SelectedLevels) : std::string{};
        }

        std::ranges::sort(Plan.IncludedAssets, {}, &AssetSelectionRecord::LogicalName);
        std::ranges::sort(Plan.ExcludedAssets, {}, &AssetSelectionRecord::LogicalName);
        return Plan;
    }

    TExpected<std::vector<AssetSelectionRecord>>
    AssetCookServiceAdapter::ResolveSelectedAssets(const ResolvedBuildRequest& Request)
    {
        auto Plan = ResolveAssetSelection(Request);
        if (!Plan)
        {
            return std::unexpected(Plan.error());
        }

        return Plan->IncludedAssets;
    }

    TExpected<AssetCookNodeResult> AssetCookServiceAdapter::ExecuteResolveAssetSelection(
        const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node)
    {
        if (Node.Outputs.empty())
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "ResolveAssetSelection node requires one output path"));
        }

        auto Plan = ResolveAssetSelection(Request);
        if (!Plan)
        {
            return std::unexpected(Plan.error());
        }

        const std::filesystem::path OutputPath = std::filesystem::path(Node.Outputs.front()).lexically_normal();
        const Json Root = BuildSelectionJson(Request, Graph, *Plan);
        if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
        {
            return std::unexpected(WriteResult.error());
        }

        return AssetCookNodeResult{
            .Message = "Resolved selected and excluded package assets with provenance.",
            .Outputs = {NormalizePathString(OutputPath)},
        };
    }

    TExpected<AssetCookNodeResult> AssetCookServiceAdapter::ExecuteEnumerateAssets(
        const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node)
    {
        if (Node.Outputs.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "EnumerateAssets node requires one output path"));
        }

        auto Plan = ResolveAssetSelection(Request);
        if (!Plan)
        {
            return std::unexpected(Plan.error());
        }

        const std::filesystem::path OutputPath = std::filesystem::path(Node.Outputs.front()).lexically_normal();
        const Json Root = BuildSelectionJson(Request, Graph, *Plan);
        if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
        {
            return std::unexpected(WriteResult.error());
        }

        return AssetCookNodeResult{
            .Message = "Enumerated supported package assets with chunk ownership.",
            .Outputs = {NormalizePathString(OutputPath)},
        };
    }

    TExpected<AssetCookNodeResult> AssetCookServiceAdapter::ExecuteCookAssets(
        const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node)
    {
        if (Node.Inputs.empty() || Node.Outputs.empty())
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "CookAssets node requires one input path and one output directory"));
        }

        auto Selection = ParseSelectionFile(std::filesystem::path(Node.Inputs.front()));
        if (!Selection)
        {
            return std::unexpected(Selection.error());
        }

        const std::filesystem::path OutputDirectory = std::filesystem::path(Node.Outputs.front()).lexically_normal();
        if (Result DirectoryResult = EnsureDirectory(OutputDirectory); !DirectoryResult)
        {
            return std::unexpected(DirectoryResult.error());
        }

        auto Cooked = CookSelectedAssets(Request, Selection->IncludedAssets);
        if (!Cooked)
        {
            return std::unexpected(Cooked.error());
        }

        const std::filesystem::path IndexFile = OutputDirectory / std::string(kCookedAssetIndexFileName);
        const Json Root = BuildCookIndexJson(Request, Graph, *Cooked);
        if (Result WriteResult = WriteTextFile(IndexFile, Root.dump(2) + "\n"); !WriteResult)
        {
            return std::unexpected(WriteResult.error());
        }

        return AssetCookNodeResult{
            .Message = "Cooked selected assets through SnAPI.AssetPipeline with provenance and chunk metadata.",
            .Outputs = {NormalizePathString(IndexFile)},
        };
    }

    TExpected<AssetCookNodeResult> AssetCookServiceAdapter::ExecuteWriteCookManifest(
        const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node)
    {
        if (Node.Inputs.empty() || Node.Outputs.empty())
        {
            return std::unexpected(MakeError(
                EErrorCode::InvalidArgument, "WriteCookManifest node requires one input directory and one output path"));
        }

        const std::filesystem::path CookDirectory = std::filesystem::path(Node.Inputs.front()).lexically_normal();
        const std::filesystem::path IndexFile = CookDirectory / std::string(kCookedAssetIndexFileName);
        auto Cooked = ParseCookIndexFile(IndexFile);
        if (!Cooked)
        {
            return std::unexpected(Cooked.error());
        }

        const std::filesystem::path OutputPath = std::filesystem::path(Node.Outputs.front()).lexically_normal();
        const Json Root = BuildCookManifestJson(Request, Graph, *Cooked);
        if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
        {
            return std::unexpected(WriteResult.error());
        }

        return AssetCookNodeResult{
            .Message = "Wrote cook manifest from cooked asset records.",
            .Outputs = {NormalizePathString(OutputPath)},
        };
    }

    TExpected<AssetCookNodeResult> AssetCookServiceAdapter::ExecuteWriteSnpak(
        const ResolvedBuildRequest& Request, const BuildGraph&,
        const BuildGraphNode& Node)
    {
        if (Node.Inputs.empty() || Node.Outputs.empty())
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "WriteSnpak node requires cooked input metadata and one output directory"));
        }

        const std::filesystem::path CookDirectory = std::filesystem::path(Node.Inputs.front()).lexically_normal();
        const std::filesystem::path IndexFile = CookDirectory / std::string(kCookedAssetIndexFileName);
        auto CookedIndex = ParseCookIndexFile(IndexFile);
        if (!CookedIndex)
        {
            return std::unexpected(CookedIndex.error());
        }

        std::vector<AssetSelectionRecord> Selection{};
        Selection.reserve(CookedIndex->size());
        for (const CookedAssetRecord& Record : *CookedIndex)
        {
            Selection.push_back(AssetSelectionRecord{
                .LogicalName = Record.LogicalName,
                .SourcePath = Record.SourcePath,
                .AssetKindLabel = Record.AssetKind,
                .SelectionReason = Record.SelectionReason,
                .ExplicitSelection = false,
                .Cookable = true,
                .ChunkId = Record.ChunkId.empty() ? std::string("Primary") : Record.ChunkId,
                .Provenance = Record.Provenance,
            });
        }

        const std::filesystem::path OutputDirectory = std::filesystem::path(Node.Outputs.front()).lexically_normal();
        if (Result DirectoryResult = EnsureDirectory(OutputDirectory); !DirectoryResult)
        {
            return std::unexpected(DirectoryResult.error());
        }

        std::vector<std::string> Outputs{};
        const std::vector<AssetChunkPlanEntry> ChunkPlan = BuildChunkPlan(Request, Selection);
        for (const AssetChunkPlanEntry& Chunk : ChunkPlan)
        {
            std::vector<AssetSelectionRecord> ChunkSelection{};
            ChunkSelection.reserve(Chunk.AssetLogicalNames.size());
            for (const std::string& LogicalName : Chunk.AssetLogicalNames)
            {
                const auto It = std::ranges::find(Selection, LogicalName, &AssetSelectionRecord::LogicalName);
                if (It != Selection.end())
                {
                    ChunkSelection.push_back(*It);
                }
            }

            const std::filesystem::path PackPath = OutputDirectory / Chunk.OutputFileName;
            auto Cooked = CookSelectedAssets(Request, ChunkSelection, PackPath);
            if (!Cooked)
            {
                return std::unexpected(Cooked.error());
            }

            Outputs.push_back(NormalizePathString(PackPath));
        }

        if (Node.Inputs.size() > 2u)
        {
            auto SelectionPlan = ParseSelectionFile(std::filesystem::path(Node.Inputs[2]).lexically_normal());
            if (!SelectionPlan)
            {
                return std::unexpected(SelectionPlan.error());
            }

            auto LooseOutputs = CopyVerbatimSelectedAssets(SelectionPlan->IncludedAssets, OutputDirectory);
            if (!LooseOutputs)
            {
                return std::unexpected(LooseOutputs.error());
            }

            Outputs.insert(Outputs.end(), LooseOutputs->begin(), LooseOutputs->end());
        }

        std::ranges::sort(Outputs);

        return AssetCookNodeResult{
            .Message = "Wrote cooked asset bundle set and copied selected verbatim asset files.",
            .Outputs = std::move(Outputs),
        };
    }

} // namespace SnAPI::GameFramework
