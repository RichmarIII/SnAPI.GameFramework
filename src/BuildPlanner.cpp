#include "BuildPlanner.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <ranges>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace SnAPI::GameFramework
{
    namespace
    {

        using Json = nlohmann::ordered_json;

        constexpr std::string_view kRuleMissingBuildId = "BuildPlan.BuildIdMissing";
        constexpr std::string_view kRuleMissingRequestHash = "BuildPlan.RequestHashMissing";
        constexpr std::string_view kRuleMissingHistoryDirectory = "BuildPlan.HistoryDirectoryMissing";
        constexpr std::string_view kRuleMissingStageDirectory = "BuildPlan.StageDirectoryMissing";
        constexpr std::string_view kRuleNodeIdMissing = "BuildPlan.NodeIdMissing";
        constexpr std::string_view kRuleNodeDuplicateId = "BuildPlan.NodeDuplicateId";
        constexpr std::string_view kRuleNodeNameMissing = "BuildPlan.NodeNameMissing";
        constexpr std::string_view kRuleNodeDependencyMissing = "BuildPlan.NodeDependencyMissing";
        constexpr std::string_view kRuleNodeDependencyCycle = "BuildPlan.NodeDependencyCycle";
        constexpr std::string_view kRuleNodeDependencyStage = "BuildPlan.NodeDependencyStage";
        constexpr std::string_view kDefaultExecutionEnvironment = "host-local";

        /**
         * @brief Trim leading and trailing ASCII whitespace from one string copy.
         * @param Text Source text.
         * @return Trimmed copy.
         */
        [[nodiscard]] std::string TrimCopy(const std::string_view Text)
        {
            std::size_t Begin = 0;
            std::size_t End = Text.size();
            while (Begin < End && std::isspace(static_cast<unsigned char>(Text[Begin])) != 0)
            {
                ++Begin;
            }
            while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1])) != 0)
            {
                --End;
            }
            return std::string(Text.substr(Begin, End - Begin));
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
         * @brief Convert one build stage enum into canonical text.
         * @param Stage Build stage to stringify.
         * @return Canonical stage name.
         */
        [[nodiscard]] std::string ToString(const EBuildStage Stage)
        {
            switch (Stage)
            {
            case EBuildStage::Preflight:
                return "Preflight";
            case EBuildStage::Planning:
                return "Planning";
            case EBuildStage::Code:
                return "Code";
            case EBuildStage::Assets:
                return "Assets";
            case EBuildStage::Staging:
                return "Staging";
            case EBuildStage::Finalize:
                return "Finalize";
            }

            return "Preflight";
        }

        /**
         * @brief Convert one build node type enum into canonical text.
         * @param Type Build node type to stringify.
         * @return Canonical node type name.
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
         * @brief Normalize one authored token into a filesystem-safe path component.
         * @param RawValue Source token such as a platform or execution-environment string.
         * @param Fallback Fallback component when the source token is empty after normalization.
         * @return Stable filesystem-safe path component.
         */
        [[nodiscard]] std::string SanitizePathComponent(const std::string_view RawValue,
                                                        const std::string_view Fallback)
        {
            const std::string Trimmed = TrimCopy(RawValue);
            const std::string_view Source = Trimmed.empty() ? Fallback : std::string_view(Trimmed);

            std::string Result{};
            Result.reserve(Source.size());

            bool LastWasSeparator = false;
            for (const unsigned char Character : Source)
            {
                if (std::isalnum(Character) != 0)
                {
                    Result.push_back(static_cast<char>(std::tolower(Character)));
                    LastWasSeparator = false;
                    continue;
                }

                if (Character == '-' || Character == '_')
                {
                    Result.push_back(static_cast<char>(Character));
                    LastWasSeparator = false;
                    continue;
                }

                if (!LastWasSeparator)
                {
                    Result.push_back('_');
                    LastWasSeparator = true;
                }
            }

            while (!Result.empty() && Result.back() == '_')
            {
                Result.pop_back();
            }

            return Result.empty() ? std::string(Fallback) : Result;
        }

        /**
         * @brief Build the isolated intermediate code-build root for one resolved request.
         * @param Request Frozen request whose platform/config/environment drive the build tree.
         * @return Per-platform/per-config/per-environment build directory.
         */
        [[nodiscard]] std::filesystem::path BuildCodeBuildRootDirectory(const ResolvedBuildRequest& Request)
        {
            const std::string PlatformKey = SanitizePathComponent(Request.Profile.Platform, "unknown-platform");
            const std::string ConfigurationKey = SanitizePathComponent(ToString(Request.Profile.Configuration), "development");
            const std::string EnvironmentKey = SanitizePathComponent(
                Request.Profile.ExecutionEnvironment.empty() ? kDefaultExecutionEnvironment
                                                             : std::string_view(Request.Profile.ExecutionEnvironment),
                kDefaultExecutionEnvironment);

            return (Request.Project.IntermediateRootDirectory / "Build" / PlatformKey / ConfigurationKey / EnvironmentKey)
                .lexically_normal();
        }

        /**
         * @brief Convert one stage enum into a sortable integral rank.
         * @param Stage Build stage to rank.
         * @return Stable stage ordering rank.
         */
        [[nodiscard]] int ToStageRank(const EBuildStage Stage)
        {
            return static_cast<int>(Stage);
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
         * @brief Normalize one string token for use in planned directory names.
         * @param Token Raw authored token.
         * @return Sanitized path-safe token.
         */
        [[nodiscard]] std::string SanitizePathToken(const std::string_view Token)
        {
            const std::string Trimmed = TrimCopy(Token);
            std::string Result{};
            Result.reserve(Trimmed.size());

            bool PreviousWasSeparator = false;
            for (const unsigned char Character : Trimmed)
            {
                if (std::isalnum(Character) != 0)
                {
                    Result.push_back(static_cast<char>(Character));
                    PreviousWasSeparator = false;
                    continue;
                }

                if (Character == '_' || Character == '-')
                {
                    Result.push_back(static_cast<char>(Character));
                    PreviousWasSeparator = false;
                    continue;
                }

                if (!PreviousWasSeparator)
                {
                    Result.push_back('_');
                    PreviousWasSeparator = true;
                }
            }

            while (!Result.empty() && Result.back() == '_')
            {
                Result.pop_back();
            }

            return Result.empty() ? "Unnamed" : Result;
        }

        /**
         * @brief Build the stage-directory leaf name for one resolved request.
         * @param Request Frozen request to name.
         * @return Deterministic stage-directory leaf.
         */
        [[nodiscard]] std::string MakeStageDirectoryLeaf(const ResolvedBuildRequest& Request)
        {
            const std::string ProfileName = Request.ProfileName.empty() ? std::string("AdHoc") : Request.ProfileName;
            return SanitizePathToken(Request.Project.Descriptor.Project.Name) + "_" + SanitizePathToken(ProfileName) +
                   "_" + SanitizePathToken(Request.Profile.Platform) + "_" +
                   SanitizePathToken(ToString(Request.Profile.Configuration));
        }

        /**
         * @brief Generate one build invocation id when the caller does not provide one.
         * @param Request Frozen request used to seed the id.
         * @return Build invocation id containing UTC wall time and request hash prefix.
         */
        [[nodiscard]] std::string GenerateBuildId(const ResolvedBuildRequest& Request)
        {
            const auto Now = std::chrono::system_clock::now();
            const std::time_t NowTime = std::chrono::system_clock::to_time_t(Now);

            std::tm UtcTime{};
#if defined(_WIN32)
            gmtime_s(&UtcTime, &NowTime);
#else
            gmtime_r(&NowTime, &UtcTime);
#endif

            std::ostringstream Stream{};
            Stream << std::put_time(&UtcTime, "%Y%m%d-%H%M%S");
            const std::string HashPrefix =
                Request.RequestHash.size() > 8u ? Request.RequestHash.substr(0u, 8u) : Request.RequestHash;
            if (!HashPrefix.empty())
            {
                Stream << "-" << HashPrefix;
            }
            return Stream.str();
        }

        /**
         * @brief Normalize one filesystem path for graph storage and serialization.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
        }

        /**
         * @brief Build one deterministic node cache key from the request hash and node type.
         * @param Request Frozen request that owns the graph.
         * @param Type Node type to key.
         * @return Deterministic cache-key seed.
         */
        [[nodiscard]] std::string BuildNodeCacheKey(const ResolvedBuildRequest& Request, const EBuildNodeType Type)
        {
            return Request.RequestHash + ":" + ToString(Type);
        }

        /**
         * @brief Serialize one planned node into ordered JSON.
         * @param Node Planned node to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeNode(const BuildGraphNode& Node)
        {
            return Json::object({
                {"Id", Node.Id},
                {"Stage", ToString(Node.Stage)},
                {"Type", ToString(Node.Type)},
                {"Name", Node.Name},
                {"Dependencies", Node.Dependencies},
                {"Inputs", Node.Inputs},
                {"Outputs", Node.Outputs},
                {"CacheKey", Node.CacheKey},
                {"Cacheable", Node.Cacheable},
            });
        }

    } // namespace

    TExpected<BuildGraph> BuildPlannerService::CreatePlan(const ResolvedBuildRequest& Request,
                                                          const BuildPlannerOptions& Options)
    {
        const auto RequestIssues = BuildRequestService::Validate(Request);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(RequestIssues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        BuildGraph Graph{};
        Graph.RequestHash = Request.RequestHash;
        Graph.BuildId = TrimCopy(Options.BuildId);
        if (Graph.BuildId.empty())
        {
            Graph.BuildId = GenerateBuildId(Request);
        }

        Graph.HistoryDirectory =
            (Request.Project.SavedRootDirectory / "BuildHistory" / Graph.BuildId).lexically_normal();
        Graph.StageDirectory =
            (Graph.HistoryDirectory / "Stage" / MakeStageDirectoryLeaf(Request)).lexically_normal();

        const std::filesystem::path MetadataDirectory = Graph.HistoryDirectory / "Metadata";
        const std::filesystem::path ManifestDirectory = Graph.HistoryDirectory / "Manifests";
        const std::filesystem::path LogDirectory = Graph.HistoryDirectory / "Logs";
        const std::filesystem::path ArtifactBinDirectory = Graph.HistoryDirectory / "Artifacts/Bin";
        const std::filesystem::path ArtifactAssetsDirectory = Graph.HistoryDirectory / "Artifacts/Assets";
        const std::filesystem::path StageBinDirectory = Graph.StageDirectory / "Bin";
        const std::filesystem::path StageAssetsDirectory = Graph.StageDirectory / "Assets";
        const std::filesystem::path StageConfigDirectory = Graph.StageDirectory / "Config";
        const std::filesystem::path StageMetadataDirectory = Graph.StageDirectory / "Metadata";
        const std::filesystem::path StageSymbolsDirectory = Graph.StageDirectory / "Symbols";
        const std::filesystem::path GeneratedProjectBuildFile =
            Request.Project.IntermediateRootDirectory / "Build/Generated/ProjectModules.cmake";
        const std::filesystem::path CodeBuildRootDirectory = BuildCodeBuildRootDirectory(Request);
        const std::filesystem::path ConfiguredCMakeCache = CodeBuildRootDirectory / "CMakeCache.txt";
        const std::filesystem::path CookedOutputDirectory =
            Request.Project.IntermediateRootDirectory / "Cooked" / Request.Profile.Platform /
            ToString(Request.Profile.Configuration);
        const std::filesystem::path BuildRequestFile = Graph.HistoryDirectory / "BuildRequest.json";
        const std::filesystem::path ResolvedModulesFile = MetadataDirectory / "ResolvedModules.json";
        const std::filesystem::path ResolvedAssetsFile = MetadataDirectory / "ResolvedAssets.json";
        const std::filesystem::path ExecutionEnvironmentFile = MetadataDirectory / "ExecutionEnvironment.json";
        const std::filesystem::path EnumeratedAssetsFile = MetadataDirectory / "EnumeratedAssets.json";
        const std::filesystem::path CookManifestFile = ManifestDirectory / "CookManifest.json";
        const std::filesystem::path PackageManifestFile = StageMetadataDirectory / "PackageManifest.json";
        const std::filesystem::path StageFileHashesFile = StageMetadataDirectory / "StageFileHashes.json";
        const std::filesystem::path BuildReportFile = Graph.HistoryDirectory / "BuildReport.json";

        std::uint32_t NextNodeId = 1u;
        auto AddNode = [&](const EBuildStage Stage, const EBuildNodeType Type, std::string Name,
                           std::vector<std::uint32_t> Dependencies, std::vector<std::string> Inputs,
                           std::vector<std::string> Outputs, const bool Cacheable = true) -> std::uint32_t
        {
            const std::uint32_t NodeId = NextNodeId++;
            Graph.Nodes.push_back(BuildGraphNode{
                .Id = NodeId,
                .Stage = Stage,
                .Type = Type,
                .Name = std::move(Name),
                .Dependencies = std::move(Dependencies),
                .Inputs = std::move(Inputs),
                .Outputs = std::move(Outputs),
                .CacheKey = BuildNodeCacheKey(Request, Type),
                .Cacheable = Cacheable,
            });
            return NodeId;
        };

        const std::uint32_t LoadProjectId =
            AddNode(EBuildStage::Preflight, EBuildNodeType::LoadProject, "Load Project Descriptor", {},
                    {NormalizePathString(Request.Project.ProjectFilePath)},
                    {NormalizePathString(Request.Project.ProjectFilePath)}, false);
        const std::uint32_t ValidateRequestId =
            AddNode(EBuildStage::Preflight, EBuildNodeType::ValidateResolvedRequest, "Validate Resolved Request",
                    {LoadProjectId},
                    {NormalizePathString(Request.Project.ProjectFilePath), Request.RequestHash},
                    {NormalizePathString(BuildRequestFile)}, false);
        const std::uint32_t ResolveExecutionEnvironmentId =
            AddNode(EBuildStage::Preflight, EBuildNodeType::ResolveExecutionEnvironment,
                    "Resolve Execution Environment", {ValidateRequestId},
                    {Request.Profile.ExecutionEnvironment.empty() ? std::string("host-local")
                                                                  : Request.Profile.ExecutionEnvironment},
                    {NormalizePathString(ExecutionEnvironmentFile)});
        const std::uint32_t ResolveModuleSetId =
            AddNode(EBuildStage::Planning, EBuildNodeType::ResolveModuleSet, "Resolve Module Set",
                    {ValidateRequestId},
                    {NormalizePathString(Request.Project.CodeRootDirectory)},
                    {NormalizePathString(ResolvedModulesFile)});
        const std::uint32_t ResolveAssetSelectionId =
            AddNode(EBuildStage::Planning, EBuildNodeType::ResolveAssetSelection, "Resolve Asset Selection",
                    {ValidateRequestId},
                    {Request.Profile.SelectedLevels.empty() ? Request.Project.Descriptor.Startup.StartupLevelAsset
                                                            : Request.Profile.SelectedLevels.front()},
                    {NormalizePathString(ResolvedAssetsFile)});
        const std::uint32_t GenerateProjectBuildFilesId =
            AddNode(EBuildStage::Code, EBuildNodeType::GenerateProjectBuildFiles, "Generate Project Build Files",
                    {ResolveModuleSetId},
                    {NormalizePathString(Request.Project.CodeRootDirectory)},
                    {NormalizePathString(GeneratedProjectBuildFile)});
        const std::uint32_t ConfigureCMakeId =
            AddNode(EBuildStage::Code, EBuildNodeType::ConfigureCMake, "Configure CMake",
                    {ResolveExecutionEnvironmentId, GenerateProjectBuildFilesId},
                    {NormalizePathString(GeneratedProjectBuildFile)},
                    {NormalizePathString(ConfiguredCMakeCache)});
        const std::uint32_t BuildCodeId =
            AddNode(EBuildStage::Code, EBuildNodeType::BuildCode, "Build Runtime Code", {ConfigureCMakeId},
                    {NormalizePathString(ConfiguredCMakeCache)}, {NormalizePathString(ArtifactBinDirectory)});
        const std::uint32_t EnumerateAssetsId =
            AddNode(EBuildStage::Assets, EBuildNodeType::EnumerateAssets, "Enumerate Assets",
                    {ResolveAssetSelectionId},
                    {NormalizePathString(ResolvedAssetsFile)}, {NormalizePathString(EnumeratedAssetsFile)});
        const std::uint32_t CookAssetsId =
            AddNode(EBuildStage::Assets, EBuildNodeType::CookAssets, "Cook Assets",
                    {ResolveExecutionEnvironmentId, EnumerateAssetsId},
                    {NormalizePathString(EnumeratedAssetsFile), NormalizePathString(Request.Project.AssetRootDirectory)},
                    {NormalizePathString(CookedOutputDirectory)});
        const std::uint32_t WriteCookManifestId =
            AddNode(EBuildStage::Assets, EBuildNodeType::WriteCookManifest, "Write Cook Manifest",
                    {CookAssetsId},
                    {NormalizePathString(CookedOutputDirectory)}, {NormalizePathString(CookManifestFile)});
        const std::uint32_t WriteSnpakId =
            AddNode(EBuildStage::Assets, EBuildNodeType::WriteSnpak, "Write Snpak Bundles",
                    {CookAssetsId, WriteCookManifestId},
                    {NormalizePathString(CookedOutputDirectory), NormalizePathString(CookManifestFile),
                     NormalizePathString(EnumeratedAssetsFile), NormalizePathString(Request.Project.AssetRootDirectory)},
                    {NormalizePathString(ArtifactAssetsDirectory)});
        const std::uint32_t CreateStageTreeId =
            AddNode(EBuildStage::Staging, EBuildNodeType::CreateStageTree, "Create Stage Tree",
                    {ValidateRequestId},
                    {NormalizePathString(Graph.StageDirectory)},
                    {NormalizePathString(StageBinDirectory), NormalizePathString(StageAssetsDirectory),
                     NormalizePathString(StageConfigDirectory), NormalizePathString(StageMetadataDirectory),
                     NormalizePathString(StageSymbolsDirectory), NormalizePathString(LogDirectory)},
                    false);
        const std::uint32_t StageBinariesId =
            AddNode(EBuildStage::Staging, EBuildNodeType::StageBinaries, "Stage Binaries",
                    {CreateStageTreeId, BuildCodeId},
                    {NormalizePathString(ArtifactBinDirectory)}, {NormalizePathString(StageBinDirectory)});
        const std::uint32_t StageAssetsId =
            AddNode(EBuildStage::Staging, EBuildNodeType::StageAssets, "Stage Assets",
                    {CreateStageTreeId, WriteSnpakId},
                    {NormalizePathString(ArtifactAssetsDirectory)}, {NormalizePathString(StageAssetsDirectory)});
        const std::uint32_t StageConfigsId =
            AddNode(EBuildStage::Staging, EBuildNodeType::StageConfigs, "Stage Configs", {CreateStageTreeId},
                    {NormalizePathString(Request.Project.ConfigRootDirectory)},
                    {NormalizePathString(StageConfigDirectory)});
        const std::uint32_t WritePackageManifestId =
            AddNode(EBuildStage::Finalize, EBuildNodeType::WritePackageManifest, "Write Package Manifest",
                    {StageBinariesId, StageAssetsId, StageConfigsId},
                    {NormalizePathString(Graph.StageDirectory)},
                    {NormalizePathString(PackageManifestFile), NormalizePathString(StageFileHashesFile)});
        AddNode(EBuildStage::Finalize, EBuildNodeType::WriteBuildReport, "Write Build Report",
                {WriteCookManifestId, WritePackageManifestId},
                {NormalizePathString(CookManifestFile), NormalizePathString(PackageManifestFile)},
                {NormalizePathString(BuildReportFile)}, false);

        const auto GraphIssues = Validate(Graph);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(GraphIssues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        return Graph;
    }

    std::vector<BuildValidationIssue> BuildPlannerService::Validate(const BuildGraph& Graph)
    {
        std::vector<BuildValidationIssue> Issues{};

        if (TrimCopy(Graph.BuildId).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingBuildId,
                        "Build graphs require a non-empty build id.");
        }
        if (TrimCopy(Graph.RequestHash).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingRequestHash,
                        "Build graphs require a non-empty request hash.");
        }
        if (Graph.HistoryDirectory.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingHistoryDirectory,
                        "Build graphs require a non-empty history directory.");
        }
        if (Graph.StageDirectory.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingStageDirectory,
                        "Build graphs require a non-empty stage directory.");
        }

        std::unordered_set<std::uint32_t> SeenIds{};
        SeenIds.reserve(Graph.Nodes.size());
        std::unordered_map<std::uint32_t, EBuildStage> StageByNodeId{};
        StageByNodeId.reserve(Graph.Nodes.size());

        for (const BuildGraphNode& Node : Graph.Nodes)
        {
            if (Node.Id == 0u)
            {
                AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleNodeIdMissing,
                            "Build graph nodes require non-zero ids.");
            }
            if (!SeenIds.insert(Node.Id).second)
            {
                AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleNodeDuplicateId,
                            "Build graph node id is duplicated: " + std::to_string(Node.Id));
            }
            if (TrimCopy(Node.Name).empty())
            {
                AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleNodeNameMissing,
                            "Build graph node name is empty for node id " + std::to_string(Node.Id) + ".");
            }

            StageByNodeId.insert_or_assign(Node.Id, Node.Stage);
        }

        for (const BuildGraphNode& Node : Graph.Nodes)
        {
            for (const std::uint32_t DependencyId : Node.Dependencies)
            {
                if (DependencyId == Node.Id)
                {
                    AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleNodeDependencyCycle,
                                "Build graph node " + std::to_string(Node.Id) +
                                    " cannot depend on itself.");
                    continue;
                }

                const auto DependencyIt = StageByNodeId.find(DependencyId);
                if (DependencyIt == StageByNodeId.end())
                {
                    AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleNodeDependencyMissing,
                                "Build graph node " + std::to_string(Node.Id) +
                                    " depends on missing node id " + std::to_string(DependencyId) + ".");
                    continue;
                }

                if (ToStageRank(DependencyIt->second) > ToStageRank(Node.Stage))
                {
                    AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleNodeDependencyStage,
                                "Build graph node " + std::to_string(Node.Id) +
                                    " depends on a later stage node " + std::to_string(DependencyId) + ".");
                }
            }
        }

        return Issues;
    }

    TExpected<std::string> BuildPlannerService::Serialize(const BuildGraph& Graph, const int Indent)
    {
        try
        {
            const auto Issues = Validate(Graph);
            if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
            }

            Json Root = Json::object({
                {"BuildId", Graph.BuildId},
                {"RequestHash", Graph.RequestHash},
                {"HistoryDirectory", NormalizePathString(Graph.HistoryDirectory)},
                {"StageDirectory", NormalizePathString(Graph.StageDirectory)},
                {"Nodes", Json::array()},
            });

            for (const BuildGraphNode& Node : Graph.Nodes)
            {
                Root["Nodes"].push_back(SerializeNode(Node));
            }

            return Root.dump(Indent) + "\n";
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

} // namespace SnAPI::GameFramework
