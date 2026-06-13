#include "BuildExecution.h"

#include "PackageManifest.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace SnAPI::GameFramework
{
    namespace
    {

        using Json = nlohmann::ordered_json;

        constexpr std::string_view kRuleBuildIdMissing = "BuildExecution.BuildIdMissing";
        constexpr std::string_view kRuleRequestHashMissing = "BuildExecution.RequestHashMissing";
        constexpr std::string_view kRuleHistoryDirectoryMissing = "BuildExecution.HistoryDirectoryMissing";
        constexpr std::string_view kRuleStageDirectoryMissing = "BuildExecution.StageDirectoryMissing";
        constexpr std::string_view kRuleReportPathMissing = "BuildExecution.ReportPathMissing";
        constexpr std::string_view kRuleCancellationReasonMissing = "BuildExecution.CancellationReasonMissing";
        constexpr std::string_view kRuleGraphRequestMismatch = "BuildExecution.GraphRequestMismatch";
        constexpr std::string_view kRulePackageOutputFinalizeFailed = "BuildExecution.PackageOutputFinalizeFailed";
        constexpr std::string_view kRuleNodeCacheStoreFailed = "BuildExecution.NodeCacheStoreFailed";

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
         * @brief Convert one build event kind into canonical text.
         * @param Kind Event kind to stringify.
         * @return Canonical event kind name.
         */
        [[nodiscard]] std::string ToString(const EBuildEventKind Kind)
        {
            switch (Kind)
            {
            case EBuildEventKind::BuildStarted:
                return "BuildStarted";
            case EBuildEventKind::BuildPlanReady:
                return "BuildPlanReady";
            case EBuildEventKind::ValidationIssueRaised:
                return "ValidationIssueRaised";
            case EBuildEventKind::NodeQueued:
                return "NodeQueued";
            case EBuildEventKind::NodeStarted:
                return "NodeStarted";
            case EBuildEventKind::NodeProgress:
                return "NodeProgress";
            case EBuildEventKind::NodeCacheHit:
                return "NodeCacheHit";
            case EBuildEventKind::NodeFinished:
                return "NodeFinished";
            case EBuildEventKind::NodeFailed:
                return "NodeFailed";
            case EBuildEventKind::BuildCancelled:
                return "BuildCancelled";
            case EBuildEventKind::BuildFinished:
                return "BuildFinished";
            }

            return "BuildStarted";
        }

        /**
         * @brief Convert one build execution status into canonical text.
         * @param Status Build execution status to stringify.
         * @return Canonical status name.
         */
        [[nodiscard]] std::string ToString(const EBuildExecutionStatus Status)
        {
            switch (Status)
            {
            case EBuildExecutionStatus::Succeeded:
                return "Succeeded";
            case EBuildExecutionStatus::Failed:
                return "Failed";
            case EBuildExecutionStatus::Cancelled:
                return "Cancelled";
            }

            return "Succeeded";
        }

        /**
         * @brief Convert one build cancellation reason into canonical text.
         * @param Reason Build cancellation reason to stringify.
         * @return Canonical cancellation-reason name.
         */
        [[nodiscard]] std::string ToString(const EBuildCancellationReason Reason)
        {
            switch (Reason)
            {
            case EBuildCancellationReason::None:
                return "None";
            case EBuildCancellationReason::UserRequested:
                return "UserRequested";
            case EBuildCancellationReason::DependencyFailure:
                return "DependencyFailure";
            case EBuildCancellationReason::HostShutdown:
                return "HostShutdown";
            }

            return "None";
        }

        /**
         * @brief Convert one node execution status into canonical text.
         * @param Status Node execution status to stringify.
         * @return Canonical status name.
         */
        [[nodiscard]] std::string ToString(const EBuildNodeExecutionStatus Status)
        {
            switch (Status)
            {
            case EBuildNodeExecutionStatus::Succeeded:
                return "Succeeded";
            case EBuildNodeExecutionStatus::Failed:
                return "Failed";
            case EBuildNodeExecutionStatus::Cancelled:
                return "Cancelled";
            }

            return "Succeeded";
        }

        /**
         * @brief Normalize one filesystem path for storage and serialization.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
        }

        /**
         * @brief Return `true` when one path is equal to or nested under one root path.
         * @param Path Candidate path.
         * @param Root Candidate ancestor root.
         * @return `true` when `Path` starts with `Root`.
         */
        [[nodiscard]] bool IsSameOrDescendantPath(const std::filesystem::path& Path,
                                                  const std::filesystem::path& Root)
        {
            if (Root.empty())
            {
                return false;
            }

            const std::filesystem::path NormalizedPath = Path.lexically_normal();
            const std::filesystem::path NormalizedRoot = Root.lexically_normal();

            auto PathIt = NormalizedPath.begin();
            auto RootIt = NormalizedRoot.begin();
            for (; RootIt != NormalizedRoot.end(); ++RootIt, ++PathIt)
            {
                if (PathIt == NormalizedPath.end() || *PathIt != *RootIt)
                {
                    return false;
                }
            }

            return true;
        }

        /**
         * @brief Remap one path between build-specific history/stage roots.
         * @param Path Source path to remap.
         * @param FromHistoryRoot Source history root.
         * @param FromStageRoot Source stage root.
         * @param ToHistoryRoot Destination history root.
         * @param ToStageRoot Destination stage root.
         * @return Remapped path when the source lives under one known root, otherwise the normalized input path.
         */
        [[nodiscard]] std::filesystem::path RemapPathBetweenBuildRoots(const std::filesystem::path& Path,
                                                                       const std::filesystem::path& FromHistoryRoot,
                                                                       const std::filesystem::path& FromStageRoot,
                                                                       const std::filesystem::path& ToHistoryRoot,
                                                                       const std::filesystem::path& ToStageRoot)
        {
            const std::filesystem::path NormalizedPath = Path.lexically_normal();
            std::error_code Error{};

            if (IsSameOrDescendantPath(NormalizedPath, FromStageRoot))
            {
                const std::filesystem::path RelativePath = std::filesystem::relative(NormalizedPath, FromStageRoot, Error);
                if (!Error)
                {
                    return (ToStageRoot / RelativePath).lexically_normal();
                }
            }

            Error.clear();
            if (IsSameOrDescendantPath(NormalizedPath, FromHistoryRoot))
            {
                const std::filesystem::path RelativePath =
                    std::filesystem::relative(NormalizedPath, FromHistoryRoot, Error);
                if (!Error)
                {
                    return (ToHistoryRoot / RelativePath).lexically_normal();
                }
            }

            return NormalizedPath;
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
         * @brief Ensure one directory exists.
         * @param Directory Directory to create.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result EnsureDirectory(const std::filesystem::path& Directory)
        {
            std::error_code Error{};
            std::filesystem::create_directories(Directory, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to create directory: " + Error.message()));
            }
            return Ok();
        }

        /**
         * @brief Remove one filesystem path when it already exists.
         * @param Path Filesystem path to clear.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result RemovePathIfExists(const std::filesystem::path& Path)
        {
            if (Path.empty())
            {
                return Ok();
            }

            std::error_code Error{};
            if (!std::filesystem::exists(Path, Error))
            {
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to inspect output path: " + Error.message()));
                }
                return Ok();
            }

            std::filesystem::remove_all(Path, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to clear output path: " + Error.message()));
            }

            return Ok();
        }

        /**
         * @brief Copy one file or directory tree into a destination path.
         * @param Source Source path to copy from.
         * @param Destination Destination path to materialize.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result CopyPathRecursive(const std::filesystem::path& Source,
                                               const std::filesystem::path& Destination)
        {
            const std::filesystem::path NormalizedSource = Source.lexically_normal();
            const std::filesystem::path NormalizedDestination = Destination.lexically_normal();

            std::error_code Error{};
            if (!std::filesystem::exists(NormalizedSource, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Resume source output is missing: " + NormalizedSource.string()));
            }

            if (NormalizedSource == NormalizedDestination)
            {
                return Ok();
            }

            if (Result RemoveResult = RemovePathIfExists(NormalizedDestination); !RemoveResult)
            {
                return RemoveResult;
            }

            if (std::filesystem::is_directory(NormalizedSource, Error) && !Error)
            {
                if (Result DirectoryResult = EnsureDirectory(NormalizedDestination.parent_path()); !DirectoryResult)
                {
                    return DirectoryResult;
                }

                std::filesystem::copy(NormalizedSource,
                                      NormalizedDestination,
                                      std::filesystem::copy_options::recursive |
                                          std::filesystem::copy_options::overwrite_existing,
                                      Error);
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to copy resumed directory output: " + Error.message()));
                }
                return Ok();
            }

            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to inspect resume source output: " + Error.message()));
            }

            if (Result DirectoryResult = EnsureDirectory(NormalizedDestination.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::filesystem::copy_file(
                NormalizedSource, NormalizedDestination, std::filesystem::copy_options::overwrite_existing, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to copy resumed file output: " + Error.message()));
            }

            return Ok();
        }

        /**
         * @brief Write one UTF-8 text file, creating parent directories first.
         * @param FilePath Target file path.
         * @param Text UTF-8 text to write.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result WriteTextFile(const std::filesystem::path& FilePath, const std::string_view Text)
        {
            if (Result DirectoryResult = EnsureDirectory(FilePath.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::ofstream Output(FilePath, std::ios::binary | std::ios::trunc);
            if (!Output.is_open())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open output file for writing"));
            }

            Output.write(Text.data(), static_cast<std::streamsize>(Text.size()));
            if (!Output.good())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to write output file"));
            }

            Output.flush();
            if (!Output.good())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to flush output file"));
            }

            return Ok();
        }

        /**
         * @brief Build a minimal placeholder `.snpak` file name for one request.
         * @param Request Frozen build request.
         * @return Placeholder `.snpak` file name.
         */
        [[nodiscard]] std::string MakePlaceholderSnpakName(const ResolvedBuildRequest& Request)
        {
            return Request.Project.Descriptor.Project.Name + "_" + Request.Profile.Platform + "_" +
                   (Request.Profile.SelectedLevels.empty() ? std::string("Primary") : std::string("Selected")) +
                   ".snpak";
        }

        /**
         * @brief Return the first output path from one node when present.
         * @param Node Planned node to inspect.
         * @return First output path or an empty path.
         */
        [[nodiscard]] std::filesystem::path FirstOutputPath(const BuildGraphNode& Node)
        {
            return Node.Outputs.empty() ? std::filesystem::path{} : std::filesystem::path(Node.Outputs.front());
        }

        /**
         * @brief Find the prior execution record for one node in one baseline report.
         * @param Report Prior build report.
         * @param Node Planned node to match.
         * @return Matching baseline record or `nullptr`.
         */
        [[nodiscard]] const BuildNodeExecutionRecord* FindBaselineNodeRecord(const BuildExecutionReport& Report,
                                                                             const BuildGraphNode& Node)
        {
            const auto It = std::ranges::find_if(Report.NodeRecords, [&Node](const BuildNodeExecutionRecord& Record)
                                                 { return Record.NodeId == Node.Id && Record.Type == Node.Type; });
            return It == Report.NodeRecords.end() ? nullptr : std::addressof(*It);
        }

        /**
         * @brief Attempt to reuse one prior successful node result from a baseline report.
         * @param Graph Current planned graph.
         * @param Node Current planned node.
         * @param BaselineReport Prior build report to reuse.
         * @return Reused node result, `std::nullopt` when the node cannot be resumed, or a structured restore error.
         */
        [[nodiscard]] TExpected<std::optional<BuildNodeExecutionResult>>
        TryResumeNodeFromBaseline(const BuildGraph& Graph,
                                  const BuildGraphNode& Node,
                                  const BuildExecutionReport& BaselineReport)
        {
            if (Node.Type == EBuildNodeType::WriteBuildReport)
            {
                return std::optional<BuildNodeExecutionResult>{};
            }

            const BuildNodeExecutionRecord* BaselineRecord = FindBaselineNodeRecord(BaselineReport, Node);
            if (BaselineRecord == nullptr || BaselineRecord->Status != EBuildNodeExecutionStatus::Succeeded)
            {
                return std::optional<BuildNodeExecutionResult>{};
            }

            std::vector<std::filesystem::path> SourceOutputs{};
            if (!BaselineRecord->Outputs.empty())
            {
                SourceOutputs.reserve(BaselineRecord->Outputs.size());
                for (const std::string& Output : BaselineRecord->Outputs)
                {
                    SourceOutputs.push_back(std::filesystem::path(Output).lexically_normal());
                }
            }
            else
            {
                SourceOutputs.reserve(Node.Outputs.size());
                for (const std::string& Output : Node.Outputs)
                {
                    SourceOutputs.push_back(
                        RemapPathBetweenBuildRoots(std::filesystem::path(Output),
                                                   Graph.HistoryDirectory,
                                                   Graph.StageDirectory,
                                                   BaselineReport.HistoryDirectory,
                                                   BaselineReport.StageDirectory));
                }
            }

            for (const std::filesystem::path& SourceOutput : SourceOutputs)
            {
                std::error_code Error{};
                if (!std::filesystem::exists(SourceOutput, Error) || Error)
                {
                    return std::optional<BuildNodeExecutionResult>{};
                }
            }

            std::vector<std::string> RestoredOutputs{};
            RestoredOutputs.reserve(SourceOutputs.size());
            for (const std::filesystem::path& SourceOutput : SourceOutputs)
            {
                const std::filesystem::path DestinationOutput =
                    RemapPathBetweenBuildRoots(SourceOutput,
                                               BaselineReport.HistoryDirectory,
                                               BaselineReport.StageDirectory,
                                               Graph.HistoryDirectory,
                                               Graph.StageDirectory);
                if (Result CopyResult = CopyPathRecursive(SourceOutput, DestinationOutput); !CopyResult)
                {
                    return std::unexpected(CopyResult.error());
                }

                RestoredOutputs.push_back(NormalizePathString(DestinationOutput));
            }

            return BuildNodeExecutionResult{
                .CacheHit = true,
                .Message = "Reused outputs from prior build '" + BaselineReport.BuildId + "'.",
                .Outputs = std::move(RestoredOutputs),
            };
        }

        /**
         * @brief Copy regular files from one directory tree into another directory tree.
         * @param SourceDirectory Directory to copy from.
         * @param DestinationDirectory Directory to copy into.
         * @return Copied destination file paths or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::vector<std::filesystem::path>> CopyRegularFiles(
            const std::filesystem::path& SourceDirectory, const std::filesystem::path& DestinationDirectory)
        {
            std::vector<std::filesystem::path> CopiedFiles{};
            std::error_code Error{};
            if (!std::filesystem::exists(SourceDirectory, Error) || Error)
            {
                return CopiedFiles;
            }

            if (Result DirectoryResult = EnsureDirectory(DestinationDirectory); !DirectoryResult)
            {
                return std::unexpected(DirectoryResult.error());
            }

            for (const auto& Entry : std::filesystem::recursive_directory_iterator(SourceDirectory, Error))
            {
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to enumerate artifact directory: " + Error.message()));
                }
                if (!Entry.is_regular_file())
                {
                    continue;
                }

                const std::filesystem::path RelativePath = std::filesystem::relative(Entry.path(), SourceDirectory, Error);
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to relativize artifact path: " + Error.message()));
                }

                const std::filesystem::path DestinationPath = DestinationDirectory / RelativePath;
                if (Result DirectoryResult = EnsureDirectory(DestinationPath.parent_path()); !DirectoryResult)
                {
                    return std::unexpected(DirectoryResult.error());
                }

                std::filesystem::copy_file(Entry.path(), DestinationPath, std::filesystem::copy_options::overwrite_existing,
                                           Error);
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to copy staged file: " + Error.message()));
                }

                CopiedFiles.push_back(DestinationPath.lexically_normal());
            }

            return CopiedFiles;
        }

        /**
         * @brief Build the resolved runtime-config payload written into the stage tree.
         * @param Request Frozen build request that owns the package.
         * @param Graph Planned graph that owns the stage directory.
         * @return Ordered JSON object describing resolved runtime bootstrap settings.
         */
        [[nodiscard]] Json BuildResolvedRuntimeConfig(const ResolvedBuildRequest& Request, const BuildGraph& Graph)
        {
            return Json::object({
                {"BuildId", Graph.BuildId},
                {"ProjectName", Request.Project.Descriptor.Project.Name},
                {"ProjectId", Request.Project.Descriptor.Project.ProjectId},
                {"TargetPlatform", Request.Profile.Platform},
                {"Configuration", ToString(Request.Profile.Configuration)},
                {"ExecutionEnvironment", Request.Profile.ExecutionEnvironment},
                {"AssetRoot", "Assets"},
                {"Startup",
                 Json::object({
                     {"StartupLevelAsset", Request.Project.Descriptor.Startup.StartupLevelAsset},
                     {"DefaultRenderSettingsAssetId", Request.Project.Descriptor.Startup.DefaultRenderSettingsAssetId},
                     {"DefaultGameClass", Request.Project.Descriptor.Startup.DefaultGameClass},
                     {"DefaultGameModeClass", Request.Project.Descriptor.Startup.DefaultGameModeClass},
                 })},
            });
        }

        /**
         * @brief Default no-op executor used until real build adapters are integrated.
         *
         * The default executor materializes lightweight placeholder artifacts so the
         * graph can be executed end-to-end, history/reporting can be verified, and
         * future concrete executors can drop in without changing the surrounding
         * execution/reporting infrastructure.
         */
        class DefaultBuildNodeExecutor final : public IBuildNodeExecutor
        {
        public:
            explicit DefaultBuildNodeExecutor(const BuildExecutionOptions& Options)
                : m_options(Options)
            {
            }

            [[nodiscard]] TExpected<BuildNodeExecutionResult> Execute(const ResolvedBuildRequest& Request,
                                                                      const BuildGraph& Graph,
                                                                      const BuildGraphNode& Node) override
            {
                switch (Node.Type)
                {
                case EBuildNodeType::LoadProject:
                case EBuildNodeType::ValidateResolvedRequest:
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Validated planned node inputs.",
                        .Outputs = Node.Outputs,
                    };

                case EBuildNodeType::ResolveExecutionEnvironment:
                {
                    Json Root = Json::object({
                        {"BuildId", Graph.BuildId},
                        {"Platform", Request.Profile.Platform},
                        {"ExecutionEnvironment",
                         Request.Profile.ExecutionEnvironment.empty() ? "host-local"
                                                                      : Request.Profile.ExecutionEnvironment},
                    });
                    const std::filesystem::path OutputPath = FirstOutputPath(Node);
                    if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Resolved execution environment metadata.",
                        .Outputs = {NormalizePathString(OutputPath)},
                    };
                }

                case EBuildNodeType::ResolveModuleSet:
                {
                    Json Modules = Json::array();
                    for (const ProjectModuleDescriptor& Module : Request.Project.Descriptor.Modules)
                    {
                        Modules.push_back(Json::object({
                            {"Name", Module.Name},
                            {"Root", Module.Root},
                            {"LoadInEditor", Module.LoadInEditor},
                            {"LoadInRuntime", Module.LoadInRuntime},
                        }));
                    }

                    Json Root = Json::object({
                        {"BuildId", Graph.BuildId},
                        {"Modules", std::move(Modules)},
                    });
                    const std::filesystem::path OutputPath = FirstOutputPath(Node);
                    if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Resolved participating modules.",
                        .Outputs = {NormalizePathString(OutputPath)},
                    };
                }

                case EBuildNodeType::ResolveAssetSelection:
                {
                    if (m_options.AssetCook.Enabled)
                    {
                        auto Result = AssetCookServiceAdapter::ExecuteResolveAssetSelection(Request, Graph, Node);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    Json Root = Json::object({
                        {"BuildId", Graph.BuildId},
                        {"SelectedLevels", Request.Profile.SelectedLevels},
                        {"ExplicitAssets", Request.Profile.ExplicitAssets},
                        {"IncludeFolders", Request.Profile.IncludeFolders},
                        {"ExcludeFolders", Request.Profile.ExcludeFolders},
                    });
                    const std::filesystem::path OutputPath = FirstOutputPath(Node);
                    if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Resolved asset selection.",
                        .Outputs = {NormalizePathString(OutputPath)},
                    };
                }

                case EBuildNodeType::EnumerateAssets:
                {
                    if (m_options.AssetCook.Enabled)
                    {
                        auto Result = AssetCookServiceAdapter::ExecuteEnumerateAssets(Request, Graph, Node);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    Json Root = Json::object({
                        {"BuildId", Graph.BuildId},
                        {"SelectedLevels", Request.Profile.SelectedLevels},
                        {"ExplicitAssets", Request.Profile.ExplicitAssets},
                        {"IncludeFolders", Request.Profile.IncludeFolders},
                        {"ExcludeFolders", Request.Profile.ExcludeFolders},
                    });
                    const std::filesystem::path OutputPath = FirstOutputPath(Node);
                    if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Enumerated candidate assets.",
                        .Outputs = {NormalizePathString(OutputPath)},
                    };
                }

                case EBuildNodeType::GenerateProjectBuildFiles:
                {
                    auto Result = CodeBuildServiceAdapter::ExecuteGenerateProjectBuildFiles(Request.Project);
                    if (!Result)
                    {
                        return std::unexpected(Result.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = std::move(Result->Message),
                        .Outputs = std::move(Result->Outputs),
                    };
                }

                case EBuildNodeType::ConfigureCMake:
                {
                    if (m_options.CodeBuild.Enabled)
                    {
                        const std::filesystem::path LogFile = Graph.HistoryDirectory / "Logs" / "CodeBuild.Configure.log";
                        auto Result =
                            CodeBuildServiceAdapter::ExecuteConfigureCMake(Request, Node, m_options.CodeBuild, LogFile);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    const std::filesystem::path OutputPath = FirstOutputPath(Node);
                    const std::string Text =
                        "# Placeholder CMake cache written by BuildExecutionService.\n"
                        "CMAKE_BUILD_TYPE=Development\n";
                    if (Result WriteResult = WriteTextFile(OutputPath, Text); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Configured placeholder CMake build tree.",
                        .Outputs = {NormalizePathString(OutputPath)},
                    };
                }

                case EBuildNodeType::BuildCode:
                {
                    if (m_options.CodeBuild.Enabled)
                    {
                        const std::filesystem::path LogFile = Graph.HistoryDirectory / "Logs" / "CodeBuild.Build.log";
                        auto Result =
                            CodeBuildServiceAdapter::ExecuteBuildCode(Request, Node, m_options.CodeBuild, LogFile);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    const std::filesystem::path OutputDirectory = FirstOutputPath(Node);
                    if (Result DirectoryResult = EnsureDirectory(OutputDirectory); !DirectoryResult)
                    {
                        return std::unexpected(DirectoryResult.error());
                    }

                    const std::filesystem::path MarkerFile =
                        OutputDirectory / (Request.Project.Descriptor.Project.Name + ".runtime.placeholder.txt");
                    const std::string Text =
                        "Placeholder runtime binary artifact emitted by BuildExecutionService.\n";
                    if (Result WriteResult = WriteTextFile(MarkerFile, Text); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }

                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Materialized placeholder runtime build artifacts.",
                        .Outputs = {NormalizePathString(MarkerFile)},
                    };
                }

                case EBuildNodeType::CookAssets:
                {
                    if (m_options.AssetCook.Enabled)
                    {
                        auto Result = AssetCookServiceAdapter::ExecuteCookAssets(Request, Graph, Node);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    const std::filesystem::path OutputDirectory = FirstOutputPath(Node);
                    if (Result DirectoryResult = EnsureDirectory(OutputDirectory); !DirectoryResult)
                    {
                        return std::unexpected(DirectoryResult.error());
                    }

                    const std::filesystem::path MarkerFile = OutputDirectory / "CookedAssets.index.json";
                    Json Root = Json::object({
                        {"BuildId", Graph.BuildId},
                        {"Platform", Request.Profile.Platform},
                        {"Configuration", Request.ProfileName.empty() ? std::string("AdHoc") : Request.ProfileName},
                    });
                    if (Result WriteResult = WriteTextFile(MarkerFile, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Materialized placeholder cooked asset outputs.",
                        .Outputs = {NormalizePathString(MarkerFile)},
                    };
                }

                case EBuildNodeType::WriteCookManifest:
                {
                    if (m_options.AssetCook.Enabled)
                    {
                        auto Result = AssetCookServiceAdapter::ExecuteWriteCookManifest(Request, Graph, Node);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    const std::filesystem::path OutputPath = FirstOutputPath(Node);
                    Json Root = Json::object({
                        {"BuildId", Graph.BuildId},
                        {"Platform", Request.Profile.Platform},
                        {"SelectedLevels", Request.Profile.SelectedLevels},
                    });
                    if (Result WriteResult = WriteTextFile(OutputPath, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Wrote placeholder cook manifest.",
                        .Outputs = {NormalizePathString(OutputPath)},
                    };
                }

                case EBuildNodeType::WriteSnpak:
                {
                    if (m_options.AssetCook.Enabled)
                    {
                        auto Result = AssetCookServiceAdapter::ExecuteWriteSnpak(Request, Graph, Node);
                        if (!Result)
                        {
                            return std::unexpected(Result.error());
                        }
                        return BuildNodeExecutionResult{
                            .CacheHit = false,
                            .Message = std::move(Result->Message),
                            .Outputs = std::move(Result->Outputs),
                        };
                    }

                    const std::filesystem::path OutputDirectory = FirstOutputPath(Node);
                    if (Result DirectoryResult = EnsureDirectory(OutputDirectory); !DirectoryResult)
                    {
                        return std::unexpected(DirectoryResult.error());
                    }

                    const std::filesystem::path PackFile = OutputDirectory / MakePlaceholderSnpakName(Request);
                    const std::string Text = "Placeholder snpak payload emitted by BuildExecutionService.\n";
                    if (Result WriteResult = WriteTextFile(PackFile, Text); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Materialized placeholder snpak bundle.",
                        .Outputs = {NormalizePathString(PackFile)},
                    };
                }

                case EBuildNodeType::CreateStageTree:
                {
                    for (const std::string& Output : Node.Outputs)
                    {
                        if (Result DirectoryResult = EnsureDirectory(std::filesystem::path(Output)); !DirectoryResult)
                        {
                            return std::unexpected(DirectoryResult.error());
                        }
                    }
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Created staged output tree.",
                        .Outputs = Node.Outputs,
                    };
                }

                case EBuildNodeType::StageBinaries:
                case EBuildNodeType::StageAssets:
                {
                    if (Node.Inputs.empty() || Node.Outputs.empty())
                    {
                        return std::unexpected(
                            MakeError(EErrorCode::InvalidArgument, "Stage node requires at least one input and output"));
                    }

                    const std::filesystem::path SourceDirectory = std::filesystem::path(Node.Inputs.front());
                    const std::filesystem::path DestinationDirectory = std::filesystem::path(Node.Outputs.front());
                    auto CopyResult = CopyRegularFiles(SourceDirectory, DestinationDirectory);
                    if (!CopyResult)
                    {
                        return std::unexpected(CopyResult.error());
                    }

                    std::vector<std::string> Outputs{};
                    Outputs.reserve(CopyResult->size() + 1u);
                    for (const std::filesystem::path& OutputPath : *CopyResult)
                    {
                        Outputs.push_back(NormalizePathString(OutputPath));
                    }

                    if (Node.Type == EBuildNodeType::StageBinaries && !m_options.CodeBuild.Enabled)
                    {
                        const std::filesystem::path PlaceholderFile = DestinationDirectory / "Runtime.placeholder.txt";
                        if (!std::filesystem::exists(PlaceholderFile))
                        {
                            const std::string Text =
                                "Placeholder staged runtime binary emitted by BuildExecutionService.\n";
                            if (Result WriteResult = WriteTextFile(PlaceholderFile, Text); !WriteResult)
                            {
                                return std::unexpected(WriteResult.error());
                            }
                        }
                        Outputs.push_back(NormalizePathString(PlaceholderFile));
                    }
                    else if (!m_options.AssetCook.Enabled)
                    {
                        const std::filesystem::path PlaceholderFile = DestinationDirectory / MakePlaceholderSnpakName(Request);
                        if (!std::filesystem::exists(PlaceholderFile))
                        {
                            const std::string Text =
                                "Placeholder staged snpak emitted by BuildExecutionService.\n";
                            if (Result WriteResult = WriteTextFile(PlaceholderFile, Text); !WriteResult)
                            {
                                return std::unexpected(WriteResult.error());
                            }
                        }
                        Outputs.push_back(NormalizePathString(PlaceholderFile));
                    }

                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = Node.Type == EBuildNodeType::StageBinaries ? "Staged runtime binaries."
                                                                              : "Staged asset bundles.",
                        .Outputs = std::move(Outputs),
                    };
                }

                case EBuildNodeType::StageConfigs:
                {
                    const std::filesystem::path SourceDirectory =
                        Node.Inputs.empty() ? Request.Project.ConfigRootDirectory : std::filesystem::path(Node.Inputs.front());
                    const std::filesystem::path DestinationDirectory = FirstOutputPath(Node);
                    if (Result DirectoryResult = EnsureDirectory(DestinationDirectory); !DirectoryResult)
                    {
                        return std::unexpected(DirectoryResult.error());
                    }

                    auto CopyResult = CopyRegularFiles(SourceDirectory, DestinationDirectory);
                    if (!CopyResult)
                    {
                        return std::unexpected(CopyResult.error());
                    }

                    const std::filesystem::path ConfigFile = DestinationDirectory / "ResolvedRuntimeConfig.json";
                    Json Root = BuildResolvedRuntimeConfig(Request, Graph);
                    if (Result WriteResult = WriteTextFile(ConfigFile, Root.dump(2) + "\n"); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }

                    std::vector<std::string> Outputs{};
                    Outputs.reserve(CopyResult->size() + 1u);
                    for (const std::filesystem::path& OutputPath : *CopyResult)
                    {
                        Outputs.push_back(NormalizePathString(OutputPath));
                    }
                    Outputs.push_back(NormalizePathString(ConfigFile));

                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = CopyResult->empty() ? "Staged resolved runtime configuration."
                                                       : "Staged authored config files and resolved runtime configuration.",
                        .Outputs = std::move(Outputs),
                    };
                }

                case EBuildNodeType::WritePackageManifest:
                {
                    if (Node.Outputs.empty())
                    {
                        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                         "WritePackageManifest node requires at least one output path"));
                    }

                    auto Manifest = PackageManifestService::Create(Request, Graph);
                    if (!Manifest)
                    {
                        return std::unexpected(Manifest.error());
                    }

                    auto ManifestText = PackageManifestService::Serialize(*Manifest, 2);
                    if (!ManifestText)
                    {
                        return std::unexpected(ManifestText.error());
                    }

                    auto StageHashesText = PackageManifestService::SerializeStageFileHashes(*Manifest, 2);
                    if (!StageHashesText)
                    {
                        return std::unexpected(StageHashesText.error());
                    }

                    const std::filesystem::path PackageManifestPath = std::filesystem::path(Node.Outputs.front());
                    if (Result WriteResult = WriteTextFile(PackageManifestPath, *ManifestText); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }

                    const std::filesystem::path StageFileHashesPath =
                        Node.Outputs.size() > 1u ? std::filesystem::path(Node.Outputs[1])
                                                 : (PackageManifestPath.parent_path() / "StageFileHashes.json");
                    if (Result WriteResult = WriteTextFile(StageFileHashesPath, *StageHashesText); !WriteResult)
                    {
                        return std::unexpected(WriteResult.error());
                    }

                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Wrote package manifest and staged file hashes.",
                        .Outputs = {NormalizePathString(PackageManifestPath), NormalizePathString(StageFileHashesPath)},
                    };
                }

                case EBuildNodeType::WriteBuildReport:
                    return BuildNodeExecutionResult{
                        .CacheHit = false,
                        .Message = "Build report will be finalized after graph execution.",
                        .Outputs = Node.Outputs,
                    };
                }

                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported build node type"));
            }

        private:
            const BuildExecutionOptions& m_options;
        };

        /**
         * @brief Format one structured build event as a log line.
         * @param Event Event to format.
         * @return Single-line UTF-8 log string.
         */
        [[nodiscard]] std::string FormatEventLogLine(const BuildEvent& Event)
        {
            std::ostringstream Stream{};
            Stream << "[" << Event.TimestampUtc << "] " << ToString(Event.Kind);
            if (Event.NodeId != 0u)
            {
                Stream << " node=" << Event.NodeId;
            }
            Stream << " stage=" << ToString(Event.Stage) << " " << Event.Message;
            if (!Event.Payload.empty())
            {
                Stream << " | " << Event.Payload.dump();
            }
            Stream << "\n";
            return Stream.str();
        }

        /**
         * @brief Append one event line to the per-stage log file.
         * @param HistoryDirectory Build history directory.
         * @param Event Event to write.
         * @param OutLogFiles Set of log files touched during execution.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result WriteStageLogLine(const std::filesystem::path& HistoryDirectory, const BuildEvent& Event,
                                               std::set<std::filesystem::path>& OutLogFiles)
        {
            const std::filesystem::path LogFile = HistoryDirectory / "Logs" / (ToString(Event.Stage) + ".log");
            if (Result DirectoryResult = EnsureDirectory(LogFile.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::ofstream Output(LogFile, std::ios::binary | std::ios::app);
            if (!Output.is_open())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open stage log file"));
            }

            const std::string Line = FormatEventLogLine(Event);
            Output.write(Line.data(), static_cast<std::streamsize>(Line.size()));
            if (!Output.good())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to append stage log file"));
            }
            Output.flush();
            if (!Output.good())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to flush stage log file"));
            }

            OutLogFiles.insert(LogFile.lexically_normal());
            return Ok();
        }

        /**
         * @brief Emit one structured build event to the event sink and stage logs.
         * @param HistoryDirectory Build history directory.
         * @param Event Event to emit.
         * @param Options Execution options providing the optional event sink.
         * @param OutEventCount Event counter to increment.
         * @param OutLogFiles Set of stage log files touched during execution.
         * @return Success or a structured infrastructure error.
         */
        [[nodiscard]] Result EmitEvent(const std::filesystem::path& HistoryDirectory, const BuildEvent& Event,
                                       const BuildExecutionOptions& Options, std::uint64_t& OutEventCount,
                                       std::set<std::filesystem::path>& OutLogFiles)
        {
            ++OutEventCount;

            if (Options.WriteHistoryArtifacts)
            {
                if (Result LogResult = WriteStageLogLine(HistoryDirectory, Event, OutLogFiles); !LogResult)
                {
                    return LogResult;
                }
            }

            if (Options.EventSink)
            {
                Options.EventSink(Event);
            }

            return Ok();
        }

        /**
         * @brief Convert one node execution record into ordered JSON.
         * @param Record Node execution record to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeNodeRecord(const BuildNodeExecutionRecord& Record)
        {
            return Json::object({
                {"NodeId", Record.NodeId},
                {"Stage", ToString(Record.Stage)},
                {"Type", ToString(Record.Type)},
                {"Name", Record.Name},
                {"Status", ToString(Record.Status)},
                {"CacheHit", Record.CacheHit},
                {"StartedAtUtc", Record.StartedAtUtc},
                {"FinishedAtUtc", Record.FinishedAtUtc},
                {"DurationMilliseconds", Record.DurationMilliseconds},
                {"Message", Record.Message},
                {"Outputs", Record.Outputs},
            });
        }

        /**
         * @brief Collect regular output files referenced by the report.
         * @param Report Report whose output files should be collected.
         * @return Sorted unique normalized output file paths.
         */
        [[nodiscard]] std::vector<std::string> CollectOutputFiles(const BuildExecutionReport& Report)
        {
            std::set<std::string> OutputFiles{};

            auto AddFileIfPresent = [&](const std::filesystem::path& FilePath)
            {
                std::error_code Error{};
                if (!FilePath.empty() && std::filesystem::exists(FilePath, Error) && !Error &&
                    std::filesystem::is_regular_file(FilePath, Error) && !Error)
                {
                    OutputFiles.insert(NormalizePathString(FilePath));
                }
            };

            auto AddRegularFilesUnderDirectory = [&](const std::filesystem::path& DirectoryPath)
            {
                std::error_code Error{};
                if (DirectoryPath.empty() || !std::filesystem::exists(DirectoryPath, Error) || Error ||
                    !std::filesystem::is_directory(DirectoryPath, Error) || Error)
                {
                    return;
                }

                for (const auto& Entry : std::filesystem::recursive_directory_iterator(DirectoryPath, Error))
                {
                    if (Error)
                    {
                        return;
                    }
                    if (Entry.is_regular_file())
                    {
                        OutputFiles.insert(NormalizePathString(Entry.path()));
                    }
                }
            };

            AddFileIfPresent(Report.BuildRequestFilePath);
            AddFileIfPresent(Report.BuildPlanFilePath);
            AddFileIfPresent(Report.BuildReportFilePath);
            AddFileIfPresent(Report.BuildSummaryFilePath);
            AddFileIfPresent(Report.ArchiveFilePath);
            for (const std::filesystem::path& LogFile : Report.StageLogFilePaths)
            {
                AddFileIfPresent(LogFile);
            }
            AddRegularFilesUnderDirectory(Report.PackageDirectoryPath);
            for (const BuildNodeExecutionRecord& Record : Report.NodeRecords)
            {
                for (const std::string& Output : Record.Outputs)
                {
                    AddFileIfPresent(std::filesystem::path(Output));
                }
            }

            return std::vector<std::string>(OutputFiles.begin(), OutputFiles.end());
        }

        /**
         * @brief Append cancelled node records for the remaining unexecuted nodes.
         * @param Graph Planned graph that owns the remaining nodes.
         * @param StartIndex First node index that did not execute.
         * @param TimestampUtc Shared start/finish timestamp for the synthetic records.
         * @param Message Human-readable skip/cancel message.
         * @param OutRecords Destination execution-record list.
         */
        void AppendUnstartedNodeRecords(const BuildGraph& Graph,
                                        const std::size_t StartIndex,
                                        const std::string_view TimestampUtc,
                                        const std::string_view Message,
                                        std::vector<BuildNodeExecutionRecord>& OutRecords)
        {
            for (std::size_t Index = StartIndex; Index < Graph.Nodes.size(); ++Index)
            {
                const BuildGraphNode& Node = Graph.Nodes[Index];
                OutRecords.push_back(BuildNodeExecutionRecord{
                    .NodeId = Node.Id,
                    .Stage = Node.Stage,
                    .Type = Node.Type,
                    .Name = Node.Name,
                    .Status = EBuildNodeExecutionStatus::Cancelled,
                    .CacheHit = false,
                    .StartedAtUtc = std::string(TimestampUtc),
                    .FinishedAtUtc = std::string(TimestampUtc),
                    .DurationMilliseconds = 0u,
                    .Message = std::string(Message),
                    .Outputs = {},
                });
            }
        }

    } // namespace

    TExpected<BuildExecutionReport> BuildExecutionService::Execute(const ResolvedBuildRequest& Request,
                                                                   const BuildGraph& Graph,
                                                                   const BuildExecutionOptions& Options)
    {
        const auto RequestIssues = BuildRequestService::Validate(Request);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(RequestIssues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        const auto GraphIssues = BuildPlannerService::Validate(Graph);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(GraphIssues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        if (Graph.RequestHash != Request.RequestHash)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             std::string(kRuleGraphRequestMismatch) +
                                                 ": Planned graph request hash does not match the frozen request."));
        }

        BuildExecutionReport Report{};
        Report.BuildId = Graph.BuildId;
        Report.RequestHash = Request.RequestHash;
        Report.Status = EBuildExecutionStatus::Succeeded;
        Report.CancellationReason = EBuildCancellationReason::None;
        Report.StartedAtUtc = MakeUtcTimestamp();
        Report.HistoryDirectory = Graph.HistoryDirectory;
        Report.StageDirectory = Graph.StageDirectory;
        Report.BuildRequestFilePath = Graph.HistoryDirectory / "BuildRequest.json";
        Report.BuildPlanFilePath = Graph.HistoryDirectory / "BuildPlan.json";
        Report.BuildReportFilePath = Graph.HistoryDirectory / "BuildReport.json";
        Report.BuildSummaryFilePath = Graph.HistoryDirectory / "BuildSummary.md";
        Report.ValidationIssues = RequestIssues;

        std::set<std::filesystem::path> StageLogFiles{};
        const auto BuildStartTime = std::chrono::steady_clock::now();

        if (Options.WriteHistoryArtifacts)
        {
            if (Result DirectoryResult = EnsureDirectory(Graph.HistoryDirectory); !DirectoryResult)
            {
                return std::unexpected(DirectoryResult.error());
            }
        }

        auto BuildRequestJson = BuildRequestService::SerializeResolved(Request, 2);
        if (!BuildRequestJson)
        {
            return std::unexpected(BuildRequestJson.error());
        }
        auto BuildPlanJson = BuildPlannerService::Serialize(Graph, 2);
        if (!BuildPlanJson)
        {
            return std::unexpected(BuildPlanJson.error());
        }

        if (Options.WriteHistoryArtifacts)
        {
            if (Result WriteResult = WriteTextFile(Report.BuildRequestFilePath, *BuildRequestJson); !WriteResult)
            {
                return std::unexpected(WriteResult.error());
            }
            if (Result WriteResult = WriteTextFile(Report.BuildPlanFilePath, *BuildPlanJson); !WriteResult)
            {
                return std::unexpected(WriteResult.error());
            }
        }

        DefaultBuildNodeExecutor DefaultExecutor(Options);
        IBuildNodeExecutor* Executor = Options.NodeExecutor != nullptr ? Options.NodeExecutor : &DefaultExecutor;

        auto Emit = [&](BuildEvent Event) -> Result
        {
            if (Event.TimestampUtc.empty())
            {
                Event.TimestampUtc = MakeUtcTimestamp();
            }
            return EmitEvent(Report.HistoryDirectory, Event, Options, Report.EventCount, StageLogFiles);
        };

        if (Result EventResult = Emit(BuildEvent{
                .Kind = EBuildEventKind::BuildStarted,
                .Severity = EBuildValidationSeverity::Info,
                .TimestampUtc = Report.StartedAtUtc,
                .Stage = EBuildStage::Preflight,
                .NodeId = 0u,
                .Message = "Build execution started.",
                .Payload = Json::object({{"BuildId", Graph.BuildId}, {"RequestHash", Graph.RequestHash}}),
            });
            !EventResult)
        {
            return std::unexpected(EventResult.error());
        }

        if (Result EventResult = Emit(BuildEvent{
                .Kind = EBuildEventKind::BuildPlanReady,
                .Severity = EBuildValidationSeverity::Info,
                .Stage = EBuildStage::Preflight,
                .NodeId = 0u,
                .Message = "Frozen request and build plan artifacts are ready.",
                .Payload = Json::object({
                    {"BuildRequest", NormalizePathString(Report.BuildRequestFilePath)},
                    {"BuildPlan", NormalizePathString(Report.BuildPlanFilePath)},
                }),
            });
            !EventResult)
        {
            return std::unexpected(EventResult.error());
        }

        for (const BuildValidationIssue& Issue : Report.ValidationIssues)
        {
            if (Result EventResult = Emit(BuildEvent{
                    .Kind = EBuildEventKind::ValidationIssueRaised,
                    .Severity = Issue.Severity,
                    .Stage = EBuildStage::Preflight,
                    .NodeId = 0u,
                    .Message = Issue.RuleId + ": " + Issue.Message,
                    .Payload = Json::object({{"RuleId", Issue.RuleId}}),
                });
                !EventResult)
            {
                return std::unexpected(EventResult.error());
            }
        }

        const bool HasResumeBaseline =
            Options.ResumeSucceededNodes && Options.ResumeBaselineReport != nullptr &&
            Options.ResumeBaselineReport->RequestHash == Request.RequestHash;

        for (std::size_t NodeIndex = 0; NodeIndex < Graph.Nodes.size(); ++NodeIndex)
        {
            const BuildGraphNode& Node = Graph.Nodes[NodeIndex];

            if (Options.CancellationRequested && Options.CancellationRequested())
            {
                Report.Status = EBuildExecutionStatus::Cancelled;
                Report.CancellationReason = Options.CancellationReasonOnRequest;

                if (Result EventResult = Emit(BuildEvent{
                        .Kind = EBuildEventKind::BuildCancelled,
                        .Severity = EBuildValidationSeverity::Warning,
                        .Stage = Node.Stage,
                        .NodeId = 0u,
                        .Message = "Build execution cancelled before node '" + Node.Name + "' started.",
                        .Payload = Json::object({{"Reason", ToString(Report.CancellationReason)}}),
                    });
                    !EventResult)
                {
                    return std::unexpected(EventResult.error());
                }

                const std::string CancelledAtUtc = MakeUtcTimestamp();
                AppendUnstartedNodeRecords(Graph,
                                           NodeIndex,
                                           CancelledAtUtc,
                                           "Build cancelled before node execution started.",
                                           Report.NodeRecords);
                break;
            }

            if (Result EventResult = Emit(BuildEvent{
                    .Kind = EBuildEventKind::NodeQueued,
                    .Severity = EBuildValidationSeverity::Info,
                    .Stage = Node.Stage,
                    .NodeId = Node.Id,
                    .Message = "Queued node '" + Node.Name + "'.",
                    .Payload = Json::object({{"Type", ToString(Node.Type)}}),
                });
                !EventResult)
            {
                return std::unexpected(EventResult.error());
            }

            const auto NodeStartTime = std::chrono::steady_clock::now();
            const std::string NodeStartedAtUtc = MakeUtcTimestamp();

            if (Result EventResult = Emit(BuildEvent{
                    .Kind = EBuildEventKind::NodeStarted,
                    .Severity = EBuildValidationSeverity::Info,
                    .Stage = Node.Stage,
                    .NodeId = Node.Id,
                    .Message = "Started node '" + Node.Name + "'.",
                    .Payload = Json::object({{"Type", ToString(Node.Type)}}),
                });
                !EventResult)
            {
                return std::unexpected(EventResult.error());
            }

            TExpected<BuildNodeExecutionResult> NodeResult =
                std::unexpected(MakeError(EErrorCode::InternalError, "Node execution was not attempted."));
            bool HasNodeResult = false;

            if (HasResumeBaseline && Report.Status == EBuildExecutionStatus::Succeeded)
            {
                auto ResumeResult = TryResumeNodeFromBaseline(Graph, Node, *Options.ResumeBaselineReport);
                if (!ResumeResult)
                {
                    NodeResult = std::unexpected(ResumeResult.error());
                    HasNodeResult = true;
                }
                else if (ResumeResult->has_value())
                {
                    NodeResult = std::move(**ResumeResult);
                    HasNodeResult = true;
                }
            }

            if (!HasNodeResult && Options.EnablePersistentNodeCache && Report.Status == EBuildExecutionStatus::Succeeded)
            {
                auto CacheRestore = BuildCacheService::TryRestore(Request, Node);
                if (!CacheRestore)
                {
                    NodeResult = std::unexpected(CacheRestore.error());
                    HasNodeResult = true;
                }
                else if (CacheRestore->has_value())
                {
                    NodeResult = BuildNodeExecutionResult{
                        .CacheHit = true,
                        .Message = std::move((*CacheRestore)->Message),
                        .Outputs = std::move((*CacheRestore)->Outputs),
                    };
                    HasNodeResult = true;
                }
            }

            if (!HasNodeResult)
            {
                NodeResult = Executor->Execute(Request, Graph, Node);
            }
            const std::string NodeFinishedAtUtc = MakeUtcTimestamp();
            const auto NodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - NodeStartTime);

            if (!NodeResult)
            {
                BuildNodeExecutionRecord Record{
                    .NodeId = Node.Id,
                    .Stage = Node.Stage,
                    .Type = Node.Type,
                    .Name = Node.Name,
                    .Status = EBuildNodeExecutionStatus::Failed,
                    .CacheHit = false,
                    .StartedAtUtc = NodeStartedAtUtc,
                    .FinishedAtUtc = NodeFinishedAtUtc,
                    .DurationMilliseconds = static_cast<std::uint64_t>(NodeDuration.count()),
                    .Message = NodeResult.error().Message,
                    .Outputs = {},
                };
                Report.NodeRecords.push_back(std::move(Record));
                Report.Status = EBuildExecutionStatus::Failed;

                if (Result EventResult = Emit(BuildEvent{
                        .Kind = EBuildEventKind::NodeFailed,
                        .Severity = EBuildValidationSeverity::Error,
                        .Stage = Node.Stage,
                        .NodeId = Node.Id,
                        .Message = "Node '" + Node.Name + "' failed: " + NodeResult.error().Message,
                        .Payload = Json::object({{"Type", ToString(Node.Type)}}),
                    });
                    !EventResult)
                {
                    return std::unexpected(EventResult.error());
                }

                const std::string SkippedAtUtc = MakeUtcTimestamp();
                AppendUnstartedNodeRecords(Graph,
                                           NodeIndex + 1u,
                                           SkippedAtUtc,
                                           "Skipped because a dependency failed earlier in the build.",
                                           Report.NodeRecords);
                break;
            }

            if (NodeResult->Outputs.empty())
            {
                NodeResult->Outputs = Node.Outputs;
            }

            BuildNodeExecutionRecord Record{
                .NodeId = Node.Id,
                .Stage = Node.Stage,
                .Type = Node.Type,
                .Name = Node.Name,
                .Status = EBuildNodeExecutionStatus::Succeeded,
                .CacheHit = NodeResult->CacheHit,
                .StartedAtUtc = NodeStartedAtUtc,
                .FinishedAtUtc = NodeFinishedAtUtc,
                .DurationMilliseconds = static_cast<std::uint64_t>(NodeDuration.count()),
                .Message = NodeResult->Message,
                .Outputs = std::move(NodeResult->Outputs),
            };

            if (Record.CacheHit)
            {
                if (Result EventResult = Emit(BuildEvent{
                        .Kind = EBuildEventKind::NodeCacheHit,
                        .Severity = EBuildValidationSeverity::Info,
                        .Stage = Node.Stage,
                        .NodeId = Node.Id,
                        .Message = "Node '" + Node.Name + "' reused cached outputs.",
                        .Payload = Json::object({{"Type", ToString(Node.Type)}}),
                    });
                    !EventResult)
                {
                    return std::unexpected(EventResult.error());
                }
            }

            if (Result EventResult = Emit(BuildEvent{
                    .Kind = EBuildEventKind::NodeFinished,
                    .Severity = EBuildValidationSeverity::Info,
                    .Stage = Node.Stage,
                    .NodeId = Node.Id,
                    .Message = "Finished node '" + Node.Name + "'.",
                    .Payload = Json::object({{"Type", ToString(Node.Type)}}),
                });
                !EventResult)
            {
                return std::unexpected(EventResult.error());
            }

            Report.NodeRecords.push_back(std::move(Record));

            if (Options.EnablePersistentNodeCache && !Report.NodeRecords.back().CacheHit)
            {
                if (Result CacheStoreResult = BuildCacheService::Store(Request, Node); !CacheStoreResult)
                {
                    AppendIssue(Report.ValidationIssues, EBuildValidationSeverity::Warning, kRuleNodeCacheStoreFailed,
                                "Failed to persist node cache for '" + Node.Name + "': " +
                                    CacheStoreResult.error().Message);

                    if (Result EventResult = Emit(BuildEvent{
                            .Kind = EBuildEventKind::ValidationIssueRaised,
                            .Severity = EBuildValidationSeverity::Warning,
                            .Stage = Node.Stage,
                            .NodeId = Node.Id,
                            .Message = std::string(kRuleNodeCacheStoreFailed) + ": Failed to persist node cache for '" +
                                       Node.Name + "': " + CacheStoreResult.error().Message,
                            .Payload = Json::object({{"RuleId", std::string(kRuleNodeCacheStoreFailed)}}),
                        });
                        !EventResult)
                    {
                        return std::unexpected(EventResult.error());
                    }
                }
            }
        }

        if (Report.Status == EBuildExecutionStatus::Succeeded)
        {
            auto PackageOutput = PackageOutputService::Finalize(Request, Graph, Options.PackageOutput);
            if (!PackageOutput)
            {
                Report.Status = EBuildExecutionStatus::Failed;
                AppendIssue(Report.ValidationIssues, EBuildValidationSeverity::Error, kRulePackageOutputFinalizeFailed,
                            PackageOutput.error().Message);

                if (Result EventResult = Emit(BuildEvent{
                        .Kind = EBuildEventKind::ValidationIssueRaised,
                        .Severity = EBuildValidationSeverity::Error,
                        .Stage = EBuildStage::Finalize,
                        .NodeId = 0u,
                        .Message = std::string(kRulePackageOutputFinalizeFailed) + ": " + PackageOutput.error().Message,
                        .Payload = Json::object({{"RuleId", std::string(kRulePackageOutputFinalizeFailed)}}),
                    });
                    !EventResult)
                {
                    return std::unexpected(EventResult.error());
                }
            }
            else
            {
                Report.PackageOutputRootDirectory = PackageOutput->OutputRootDirectory;
                Report.PackageDirectoryPath = PackageOutput->PackageDirectoryPath;
                Report.ArchiveFilePath = PackageOutput->ArchiveFilePath;
            }
        }

        Report.FinishedAtUtc = MakeUtcTimestamp();
        Report.DurationMilliseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - BuildStartTime)
                .count());

        if (Result EventResult = Emit(BuildEvent{
                .Kind = EBuildEventKind::BuildFinished,
                .Severity = Report.Status == EBuildExecutionStatus::Succeeded   ? EBuildValidationSeverity::Info
                            : Report.Status == EBuildExecutionStatus::Cancelled ? EBuildValidationSeverity::Warning
                                                                                : EBuildValidationSeverity::Error,
                .Stage = EBuildStage::Finalize,
                .NodeId = 0u,
                .Message = "Build execution finished with status " + ToString(Report.Status) + ".",
                .Payload = Json::object({
                    {"Status", ToString(Report.Status)},
                    {"CancellationReason", ToString(Report.CancellationReason)},
                }),
            });
            !EventResult)
        {
            return std::unexpected(EventResult.error());
        }

        Report.StageLogFilePaths.assign(StageLogFiles.begin(), StageLogFiles.end());
        auto SummaryText = SerializeSummary(Report);
        if (!SummaryText)
        {
            return std::unexpected(SummaryText.error());
        }

        Report.OutputFiles = CollectOutputFiles(Report);
        auto ReportJson = SerializeReport(Report);
        if (!ReportJson)
        {
            return std::unexpected(ReportJson.error());
        }

        if (Options.WriteHistoryArtifacts)
        {
            if (Result WriteResult = WriteTextFile(Report.BuildReportFilePath, *ReportJson); !WriteResult)
            {
                return std::unexpected(WriteResult.error());
            }
            if (Result WriteResult = WriteTextFile(Report.BuildSummaryFilePath, *SummaryText); !WriteResult)
            {
                return std::unexpected(WriteResult.error());
            }
        }

        Report.OutputFiles = CollectOutputFiles(Report);
        if (Options.WriteHistoryArtifacts)
        {
            auto FinalReportJson = SerializeReport(Report);
            if (!FinalReportJson)
            {
                return std::unexpected(FinalReportJson.error());
            }
            if (Result WriteResult = WriteTextFile(Report.BuildReportFilePath, *FinalReportJson); !WriteResult)
            {
                return std::unexpected(WriteResult.error());
            }
        }
        return Report;
    }

    std::vector<BuildValidationIssue> BuildExecutionService::Validate(const BuildExecutionReport& Report)
    {
        std::vector<BuildValidationIssue> Issues{};

        if (TrimCopy(Report.BuildId).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleBuildIdMissing,
                        "Build execution reports require a non-empty build id.");
        }
        if (TrimCopy(Report.RequestHash).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleRequestHashMissing,
                        "Build execution reports require a non-empty request hash.");
        }
        if (Report.HistoryDirectory.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleHistoryDirectoryMissing,
                        "Build execution reports require a non-empty history directory.");
        }
        if (Report.StageDirectory.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleStageDirectoryMissing,
                        "Build execution reports require a non-empty stage directory.");
        }
        if (Report.BuildReportFilePath.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleReportPathMissing,
                        "Build execution reports require a non-empty report file path.");
        }
        if (Report.Status == EBuildExecutionStatus::Cancelled && Report.CancellationReason == EBuildCancellationReason::None)
        {
            AppendIssue(Issues,
                        EBuildValidationSeverity::Error,
                        kRuleCancellationReasonMissing,
                        "Cancelled build execution reports must record a cancellation reason.");
        }

        return Issues;
    }

    TExpected<std::string> BuildExecutionService::SerializeReport(const BuildExecutionReport& Report, const int Indent)
    {
        try
        {
            const auto Issues = Validate(Report);
            if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
            }

            Json Root = Json::object({
                {"BuildId", Report.BuildId},
                {"RequestHash", Report.RequestHash},
                {"Status", ToString(Report.Status)},
                {"CancellationReason", ToString(Report.CancellationReason)},
                {"StartedAtUtc", Report.StartedAtUtc},
                {"FinishedAtUtc", Report.FinishedAtUtc},
                {"DurationMilliseconds", Report.DurationMilliseconds},
                {"EventCount", Report.EventCount},
                {"HistoryDirectory", NormalizePathString(Report.HistoryDirectory)},
                {"StageDirectory", NormalizePathString(Report.StageDirectory)},
                {"BuildRequestFile", NormalizePathString(Report.BuildRequestFilePath)},
                {"BuildPlanFile", NormalizePathString(Report.BuildPlanFilePath)},
                {"BuildReportFile", NormalizePathString(Report.BuildReportFilePath)},
                {"BuildSummaryFile", NormalizePathString(Report.BuildSummaryFilePath)},
                {"PackageOutputRootDirectory", NormalizePathString(Report.PackageOutputRootDirectory)},
                {"PackageDirectoryPath", NormalizePathString(Report.PackageDirectoryPath)},
                {"ArchiveFilePath", NormalizePathString(Report.ArchiveFilePath)},
                {"StageLogFiles", Json::array()},
                {"ValidationIssues", Json::array()},
                {"Nodes", Json::array()},
                {"OutputFiles", Report.OutputFiles},
            });

            for (const std::filesystem::path& LogFile : Report.StageLogFilePaths)
            {
                Root["StageLogFiles"].push_back(NormalizePathString(LogFile));
            }
            for (const BuildValidationIssue& Issue : Report.ValidationIssues)
            {
                Root["ValidationIssues"].push_back(Json::object({
                    {"Severity", Issue.Severity == EBuildValidationSeverity::Info   ? "Info"
                                     : Issue.Severity == EBuildValidationSeverity::Warning ? "Warning"
                                                                                            : "Error"},
                    {"RuleId", Issue.RuleId},
                    {"Message", Issue.Message},
                }));
            }
            for (const BuildNodeExecutionRecord& Record : Report.NodeRecords)
            {
                Root["Nodes"].push_back(SerializeNodeRecord(Record));
            }

            return Root.dump(Indent) + "\n";
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

    TExpected<std::string> BuildExecutionService::SerializeSummary(const BuildExecutionReport& Report)
    {
        try
        {
            const auto Issues = Validate(Report);
            if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
            }

            std::ostringstream Stream{};
            Stream << "# Build Summary\n\n";
            Stream << "- BuildId: `" << Report.BuildId << "`\n";
            Stream << "- RequestHash: `" << Report.RequestHash << "`\n";
            Stream << "- Status: `" << ToString(Report.Status) << "`\n";
            Stream << "- CancellationReason: `" << ToString(Report.CancellationReason) << "`\n";
            Stream << "- DurationMs: `" << Report.DurationMilliseconds << "`\n";
            Stream << "- HistoryDirectory: `" << NormalizePathString(Report.HistoryDirectory) << "`\n";
            Stream << "- StageDirectory: `" << NormalizePathString(Report.StageDirectory) << "`\n";
            Stream << "- EventCount: `" << Report.EventCount << "`\n\n";
            if (!Report.PackageDirectoryPath.empty())
            {
                Stream << "- PackageDirectory: `" << NormalizePathString(Report.PackageDirectoryPath) << "`\n";
            }
            if (!Report.ArchiveFilePath.empty())
            {
                Stream << "- ArchiveFile: `" << NormalizePathString(Report.ArchiveFilePath) << "`\n";
            }
            if (!Report.PackageOutputRootDirectory.empty())
            {
                Stream << "- PackageOutputRoot: `" << NormalizePathString(Report.PackageOutputRootDirectory) << "`\n";
            }
            Stream << "\n";

            Stream << "## Nodes\n\n";
            Stream << "| Id | Stage | Type | Status | CacheHit |\n";
            Stream << "|---|---|---|---|---|\n";
            for (const BuildNodeExecutionRecord& Record : Report.NodeRecords)
            {
                Stream << "| " << Record.NodeId << " | " << ToString(Record.Stage) << " | " << ToString(Record.Type)
                       << " | " << ToString(Record.Status) << " | " << (Record.CacheHit ? "Yes" : "No") << " |\n";
            }

            if (!Report.ValidationIssues.empty())
            {
                Stream << "\n## Validation Issues\n\n";
                for (const BuildValidationIssue& Issue : Report.ValidationIssues)
                {
                    Stream << "- `" << Issue.RuleId << "`: " << Issue.Message << "\n";
                }
            }

            return Stream.str();
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

} // namespace SnAPI::GameFramework
