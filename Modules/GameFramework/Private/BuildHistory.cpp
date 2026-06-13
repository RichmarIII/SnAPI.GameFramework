#include "BuildHistory.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
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

        constexpr std::string_view kRuleSavedRootMissing = "BuildHistory.SavedRootMissing";
        constexpr std::string_view kRuleEntryBuildIdMissing = "BuildHistory.EntryBuildIdMissing";
        constexpr std::string_view kRuleEntryHistoryDirectoryMissing = "BuildHistory.EntryHistoryDirectoryMissing";
        constexpr std::string_view kRuleEntryReportPathMissing = "BuildHistory.EntryReportPathMissing";

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
         * @brief Normalize one filesystem path for storage and comparisons.
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
         * @brief Read one UTF-8 text file from disk.
         * @param FilePath File path to read.
         * @return File contents or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::string> ReadTextFile(const std::filesystem::path& FilePath)
        {
            std::ifstream Input(FilePath, std::ios::binary);
            if (!Input.is_open())
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "Failed to open build-history artifact"));
            }

            std::ostringstream Stream{};
            Stream << Input.rdbuf();
            if (!Input.good() && !Input.eof())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to read build-history artifact"));
            }
            return Stream.str();
        }

        /**
         * @brief Read one required string field from ordered JSON.
         * @param Object Source object.
         * @param Key Required key.
         * @return Trimmed string value or a structured parse error.
         */
        [[nodiscard]] TExpected<std::string> ReadRequiredString(const Json& Object, const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_string())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Missing or invalid string field '" + std::string(Key) + "'"));
            }
            return TrimCopy(It->get<std::string>());
        }

        /**
         * @brief Read one optional string field from ordered JSON.
         * @param Object Source object.
         * @param Key Optional key.
         * @return Trimmed string value when present, otherwise an empty string.
         */
        [[nodiscard]] TExpected<std::string> ReadOptionalString(const Json& Object, const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || It->is_null())
            {
                return std::string{};
            }
            if (!It->is_string())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Invalid string field '" + std::string(Key) + "'"));
            }
            return TrimCopy(It->get<std::string>());
        }

        /**
         * @brief Read one required unsigned integer field from ordered JSON.
         * @param Object Source object.
         * @param Key Required key.
         * @return Unsigned integer value or a structured parse error.
         */
        [[nodiscard]] TExpected<std::uint64_t> ReadRequiredUInt64(const Json& Object, const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_number_unsigned())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Missing or invalid unsigned integer field '" + std::string(Key) + "'"));
            }
            return It->get<std::uint64_t>();
        }

        /**
         * @brief Read one required boolean field from ordered JSON.
         * @param Object Source object.
         * @param Key Required key.
         * @return Boolean value or a structured parse error.
         */
        [[nodiscard]] TExpected<bool> ReadRequiredBool(const Json& Object, const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_boolean())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Missing or invalid boolean field '" + std::string(Key) + "'"));
            }
            return It->get<bool>();
        }

        /**
         * @brief Read one required string array from ordered JSON.
         * @param Object Source object.
         * @param Key Required key.
         * @return Ordered string list or a structured parse error.
         */
        [[nodiscard]] TExpected<std::vector<std::string>> ReadRequiredStringArray(const Json& Object,
                                                                                  const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_array())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Missing or invalid array field '" + std::string(Key) + "'"));
            }

            std::vector<std::string> Values{};
            Values.reserve(It->size());
            for (const Json& Value : *It)
            {
                if (!Value.is_string())
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                     "Array field '" + std::string(Key) + "' must contain strings"));
                }
                Values.push_back(TrimCopy(Value.get<std::string>()));
            }
            return Values;
        }

        /**
         * @brief Parse one build-execution status from canonical text.
         * @param Text Canonical status text.
         * @return Parsed status or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildExecutionStatus> ParseExecutionStatus(const std::string_view Text)
        {
            if (Text == "Succeeded")
            {
                return EBuildExecutionStatus::Succeeded;
            }
            if (Text == "Failed")
            {
                return EBuildExecutionStatus::Failed;
            }
            if (Text == "Cancelled")
            {
                return EBuildExecutionStatus::Cancelled;
            }
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown build execution status"));
        }

        /**
         * @brief Parse one node-execution status from canonical text.
         * @param Text Canonical status text.
         * @return Parsed status or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildNodeExecutionStatus> ParseNodeExecutionStatus(const std::string_view Text)
        {
            if (Text == "Succeeded")
            {
                return EBuildNodeExecutionStatus::Succeeded;
            }
            if (Text == "Failed")
            {
                return EBuildNodeExecutionStatus::Failed;
            }
            if (Text == "Cancelled")
            {
                return EBuildNodeExecutionStatus::Cancelled;
            }
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown node execution status"));
        }

        /**
         * @brief Parse one build cancellation reason from canonical text.
         * @param Text Canonical cancellation-reason text.
         * @return Parsed cancellation reason or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildCancellationReason> ParseCancellationReason(const std::string_view Text)
        {
            if (Text == "None")
            {
                return EBuildCancellationReason::None;
            }
            if (Text == "UserRequested")
            {
                return EBuildCancellationReason::UserRequested;
            }
            if (Text == "DependencyFailure")
            {
                return EBuildCancellationReason::DependencyFailure;
            }
            if (Text == "HostShutdown")
            {
                return EBuildCancellationReason::HostShutdown;
            }
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown build cancellation reason"));
        }

        /**
         * @brief Parse one validation severity from canonical text.
         * @param Text Canonical severity text.
         * @return Parsed severity or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildValidationSeverity> ParseValidationSeverity(const std::string_view Text)
        {
            if (Text == "Info")
            {
                return EBuildValidationSeverity::Info;
            }
            if (Text == "Warning")
            {
                return EBuildValidationSeverity::Warning;
            }
            if (Text == "Error")
            {
                return EBuildValidationSeverity::Error;
            }
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown build validation severity"));
        }

        /**
         * @brief Parse one build stage from canonical text.
         * @param Text Canonical stage text.
         * @return Parsed stage or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildStage> ParseBuildStage(const std::string_view Text)
        {
            if (Text == "Preflight")
            {
                return EBuildStage::Preflight;
            }
            if (Text == "Planning")
            {
                return EBuildStage::Planning;
            }
            if (Text == "Code")
            {
                return EBuildStage::Code;
            }
            if (Text == "Assets")
            {
                return EBuildStage::Assets;
            }
            if (Text == "Staging")
            {
                return EBuildStage::Staging;
            }
            if (Text == "Finalize")
            {
                return EBuildStage::Finalize;
            }
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown build stage"));
        }

        /**
         * @brief Parse one build node type from canonical text.
         * @param Text Canonical node-type text.
         * @return Parsed node type or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildNodeType> ParseBuildNodeType(const std::string_view Text)
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
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown build node type"));
        }

        /**
         * @brief Fill lightweight history-entry paths from one history directory.
         * @param HistoryDirectory History directory to inspect.
         * @return Partially populated history entry.
         */
        [[nodiscard]] BuildHistoryEntry BuildEntrySkeleton(const std::filesystem::path& HistoryDirectory)
        {
            BuildHistoryEntry Entry{};
            Entry.BuildId = HistoryDirectory.filename().string();
            Entry.HistoryDirectory = HistoryDirectory.lexically_normal();
            Entry.BuildRequestFilePath = Entry.HistoryDirectory / "BuildRequest.json";
            Entry.BuildPlanFilePath = Entry.HistoryDirectory / "BuildPlan.json";
            Entry.BuildReportFilePath = Entry.HistoryDirectory / "BuildReport.json";
            Entry.BuildSummaryFilePath = Entry.HistoryDirectory / "BuildSummary.md";
            return Entry;
        }

    } // namespace

    TExpected<std::vector<BuildHistoryEntry>> BuildHistoryService::List(const std::filesystem::path& SavedRootDirectory,
                                                                        const BuildHistoryListOptions& Options)
    {
        if (SavedRootDirectory.empty())
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, std::string(kRuleSavedRootMissing) + ": Saved root cannot be empty."));
        }

        const std::filesystem::path HistoryRoot = (SavedRootDirectory / "BuildHistory").lexically_normal();
        std::error_code Error{};
        if (!std::filesystem::exists(HistoryRoot, Error))
        {
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to inspect build-history root: " + Error.message()));
            }
            return std::vector<BuildHistoryEntry>{};
        }

        std::vector<BuildHistoryEntry> Entries{};
        for (const auto& Entry : std::filesystem::directory_iterator(HistoryRoot, Error))
        {
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to enumerate build-history root: " + Error.message()));
            }
            if (!Entry.is_directory())
            {
                continue;
            }

            BuildHistoryEntry HistoryEntry = BuildEntrySkeleton(Entry.path());
            std::error_code ExistsError{};
            const bool HasReport = std::filesystem::exists(HistoryEntry.BuildReportFilePath, ExistsError) && !ExistsError;

            if (HasReport)
            {
                auto Report = LoadReport(HistoryEntry.BuildReportFilePath);
                if (Report)
                {
                    HistoryEntry.State = EBuildHistoryEntryState::Complete;
                    HistoryEntry.RequestHash = Report->RequestHash;
                    HistoryEntry.Status = Report->Status;
                    HistoryEntry.StartedAtUtc = Report->StartedAtUtc;
                    HistoryEntry.FinishedAtUtc = Report->FinishedAtUtc;
                    HistoryEntry.NodeCount = static_cast<std::uint64_t>(Report->NodeRecords.size());
                    HistoryEntry.OutputFileCount = static_cast<std::uint64_t>(Report->OutputFiles.size());
                    HistoryEntry.StageDirectory = Report->StageDirectory;
                    HistoryEntry.BuildRequestFilePath = Report->BuildRequestFilePath;
                    HistoryEntry.BuildPlanFilePath = Report->BuildPlanFilePath;
                    HistoryEntry.BuildReportFilePath = Report->BuildReportFilePath;
                    HistoryEntry.BuildSummaryFilePath = Report->BuildSummaryFilePath;
                }
                else
                {
                    HistoryEntry.State = EBuildHistoryEntryState::Incomplete;
                    HistoryEntry.DiagnosticMessage = Report.error().Message;
                }
            }
            else if (!Options.IncludeIncomplete)
            {
                continue;
            }

            if (HistoryEntry.State == EBuildHistoryEntryState::Incomplete && !Options.IncludeIncomplete)
            {
                continue;
            }

            Entries.push_back(std::move(HistoryEntry));
        }

        std::ranges::sort(Entries, [](const BuildHistoryEntry& Left, const BuildHistoryEntry& Right)
                          {
                              if (!Left.StartedAtUtc.empty() && !Right.StartedAtUtc.empty() &&
                                  Left.StartedAtUtc != Right.StartedAtUtc)
                              {
                                  return Left.StartedAtUtc > Right.StartedAtUtc;
                              }
                              return Left.BuildId > Right.BuildId;
                          });

        if (Options.MaxEntries > 0u && Entries.size() > Options.MaxEntries)
        {
            Entries.resize(Options.MaxEntries);
        }

        return Entries;
    }

    TExpected<BuildExecutionReport> BuildHistoryService::LoadReport(const std::filesystem::path& BuildReportFilePath)
    {
        auto ReportText = ReadTextFile(BuildReportFilePath);
        if (!ReportText)
        {
            return std::unexpected(ReportText.error());
        }

        Json Root = Json::parse(*ReportText, nullptr, false);
        if (Root.is_discarded() || !Root.is_object())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Failed to parse build report JSON"));
        }

        auto BuildId = ReadRequiredString(Root, "BuildId");
        auto RequestHash = ReadRequiredString(Root, "RequestHash");
        auto StatusText = ReadRequiredString(Root, "Status");
        auto CancellationReasonText = ReadOptionalString(Root, "CancellationReason");
        auto StartedAtUtc = ReadRequiredString(Root, "StartedAtUtc");
        auto FinishedAtUtc = ReadRequiredString(Root, "FinishedAtUtc");
        auto DurationMilliseconds = ReadRequiredUInt64(Root, "DurationMilliseconds");
        auto EventCount = ReadRequiredUInt64(Root, "EventCount");
        auto HistoryDirectory = ReadRequiredString(Root, "HistoryDirectory");
        auto StageDirectory = ReadRequiredString(Root, "StageDirectory");
        auto BuildRequestFile = ReadRequiredString(Root, "BuildRequestFile");
        auto BuildPlanFile = ReadRequiredString(Root, "BuildPlanFile");
        auto BuildReportFile = ReadRequiredString(Root, "BuildReportFile");
        auto BuildSummaryFile = ReadRequiredString(Root, "BuildSummaryFile");
        auto PackageOutputRootDirectory = ReadOptionalString(Root, "PackageOutputRootDirectory");
        auto PackageDirectoryPath = ReadOptionalString(Root, "PackageDirectoryPath");
        auto ArchiveFilePath = ReadOptionalString(Root, "ArchiveFilePath");
        auto StageLogFiles = ReadRequiredStringArray(Root, "StageLogFiles");
        auto OutputFiles = ReadRequiredStringArray(Root, "OutputFiles");

        if (!BuildId)
        {
            return std::unexpected(BuildId.error());
        }
        if (!RequestHash)
        {
            return std::unexpected(RequestHash.error());
        }
        if (!StatusText)
        {
            return std::unexpected(StatusText.error());
        }
        if (!CancellationReasonText)
        {
            return std::unexpected(CancellationReasonText.error());
        }
        if (!StartedAtUtc)
        {
            return std::unexpected(StartedAtUtc.error());
        }
        if (!FinishedAtUtc)
        {
            return std::unexpected(FinishedAtUtc.error());
        }
        if (!DurationMilliseconds)
        {
            return std::unexpected(DurationMilliseconds.error());
        }
        if (!EventCount)
        {
            return std::unexpected(EventCount.error());
        }
        if (!HistoryDirectory)
        {
            return std::unexpected(HistoryDirectory.error());
        }
        if (!StageDirectory)
        {
            return std::unexpected(StageDirectory.error());
        }
        if (!BuildRequestFile)
        {
            return std::unexpected(BuildRequestFile.error());
        }
        if (!BuildPlanFile)
        {
            return std::unexpected(BuildPlanFile.error());
        }
        if (!BuildReportFile)
        {
            return std::unexpected(BuildReportFile.error());
        }
        if (!BuildSummaryFile)
        {
            return std::unexpected(BuildSummaryFile.error());
        }
        if (!PackageOutputRootDirectory)
        {
            return std::unexpected(PackageOutputRootDirectory.error());
        }
        if (!PackageDirectoryPath)
        {
            return std::unexpected(PackageDirectoryPath.error());
        }
        if (!ArchiveFilePath)
        {
            return std::unexpected(ArchiveFilePath.error());
        }
        if (!StageLogFiles)
        {
            return std::unexpected(StageLogFiles.error());
        }
        if (!OutputFiles)
        {
            return std::unexpected(OutputFiles.error());
        }

        auto Status = ParseExecutionStatus(*StatusText);
        if (!Status)
        {
            return std::unexpected(Status.error());
        }

        auto CancellationReason =
            ParseCancellationReason(CancellationReasonText->empty() ? std::string_view("None")
                                                                    : std::string_view(*CancellationReasonText));
        if (!CancellationReason)
        {
            return std::unexpected(CancellationReason.error());
        }

        BuildExecutionReport Report{};
        Report.BuildId = *BuildId;
        Report.RequestHash = *RequestHash;
        Report.Status = *Status;
        Report.CancellationReason = *CancellationReason;
        Report.StartedAtUtc = *StartedAtUtc;
        Report.FinishedAtUtc = *FinishedAtUtc;
        Report.DurationMilliseconds = *DurationMilliseconds;
        Report.EventCount = *EventCount;
        Report.HistoryDirectory = std::filesystem::path(*HistoryDirectory);
        Report.StageDirectory = std::filesystem::path(*StageDirectory);
        Report.BuildRequestFilePath = std::filesystem::path(*BuildRequestFile);
        Report.BuildPlanFilePath = std::filesystem::path(*BuildPlanFile);
        Report.BuildReportFilePath = std::filesystem::path(*BuildReportFile);
        Report.BuildSummaryFilePath = std::filesystem::path(*BuildSummaryFile);
        Report.PackageOutputRootDirectory = std::filesystem::path(*PackageOutputRootDirectory);
        Report.PackageDirectoryPath = std::filesystem::path(*PackageDirectoryPath);
        Report.ArchiveFilePath = std::filesystem::path(*ArchiveFilePath);

        Report.StageLogFilePaths.reserve(StageLogFiles->size());
        for (const std::string& LogFile : *StageLogFiles)
        {
            Report.StageLogFilePaths.push_back(std::filesystem::path(LogFile));
        }
        Report.OutputFiles = *OutputFiles;

        const auto ValidationIssuesIt = Root.find("ValidationIssues");
        if (ValidationIssuesIt == Root.end() || !ValidationIssuesIt->is_array())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Missing or invalid array field 'ValidationIssues'"));
        }

        for (const Json& IssueJson : *ValidationIssuesIt)
        {
            if (!IssueJson.is_object())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Validation issue entries must be JSON objects"));
            }

            auto SeverityText = ReadRequiredString(IssueJson, "Severity");
            auto RuleId = ReadRequiredString(IssueJson, "RuleId");
            auto Message = ReadRequiredString(IssueJson, "Message");
            if (!SeverityText)
            {
                return std::unexpected(SeverityText.error());
            }
            if (!RuleId)
            {
                return std::unexpected(RuleId.error());
            }
            if (!Message)
            {
                return std::unexpected(Message.error());
            }

            auto Severity = ParseValidationSeverity(*SeverityText);
            if (!Severity)
            {
                return std::unexpected(Severity.error());
            }

            Report.ValidationIssues.push_back(BuildValidationIssue{
                .Severity = *Severity,
                .RuleId = *RuleId,
                .Message = *Message,
            });
        }

        const auto NodesIt = Root.find("Nodes");
        if (NodesIt == Root.end() || !NodesIt->is_array())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Missing or invalid array field 'Nodes'"));
        }

        for (const Json& NodeJson : *NodesIt)
        {
            if (!NodeJson.is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Build node entries must be JSON objects"));
            }

            auto NodeId = ReadRequiredUInt64(NodeJson, "NodeId");
            auto StageText = ReadRequiredString(NodeJson, "Stage");
            auto TypeText = ReadRequiredString(NodeJson, "Type");
            auto Name = ReadRequiredString(NodeJson, "Name");
            auto StatusTextField = ReadRequiredString(NodeJson, "Status");
            auto CacheHit = ReadRequiredBool(NodeJson, "CacheHit");
            auto NodeStartedAtUtc = ReadRequiredString(NodeJson, "StartedAtUtc");
            auto NodeFinishedAtUtc = ReadRequiredString(NodeJson, "FinishedAtUtc");
            auto NodeDuration = ReadRequiredUInt64(NodeJson, "DurationMilliseconds");
            auto Message = ReadRequiredString(NodeJson, "Message");
            auto Outputs = ReadRequiredStringArray(NodeJson, "Outputs");

            if (!NodeId)
            {
                return std::unexpected(NodeId.error());
            }
            if (!StageText)
            {
                return std::unexpected(StageText.error());
            }
            if (!TypeText)
            {
                return std::unexpected(TypeText.error());
            }
            if (!Name)
            {
                return std::unexpected(Name.error());
            }
            if (!StatusTextField)
            {
                return std::unexpected(StatusTextField.error());
            }
            if (!CacheHit)
            {
                return std::unexpected(CacheHit.error());
            }
            if (!NodeStartedAtUtc)
            {
                return std::unexpected(NodeStartedAtUtc.error());
            }
            if (!NodeFinishedAtUtc)
            {
                return std::unexpected(NodeFinishedAtUtc.error());
            }
            if (!NodeDuration)
            {
                return std::unexpected(NodeDuration.error());
            }
            if (!Message)
            {
                return std::unexpected(Message.error());
            }
            if (!Outputs)
            {
                return std::unexpected(Outputs.error());
            }

            auto Stage = ParseBuildStage(*StageText);
            auto Type = ParseBuildNodeType(*TypeText);
            auto NodeStatus = ParseNodeExecutionStatus(*StatusTextField);
            if (!Stage)
            {
                return std::unexpected(Stage.error());
            }
            if (!Type)
            {
                return std::unexpected(Type.error());
            }
            if (!NodeStatus)
            {
                return std::unexpected(NodeStatus.error());
            }

            Report.NodeRecords.push_back(BuildNodeExecutionRecord{
                .NodeId = static_cast<std::uint32_t>(*NodeId),
                .Stage = *Stage,
                .Type = *Type,
                .Name = *Name,
                .Status = *NodeStatus,
                .CacheHit = *CacheHit,
                .StartedAtUtc = *NodeStartedAtUtc,
                .FinishedAtUtc = *NodeFinishedAtUtc,
                .DurationMilliseconds = *NodeDuration,
                .Message = *Message,
                .Outputs = *Outputs,
            });
        }

        const auto ReportIssues = BuildExecutionService::Validate(Report);
        const auto BlockingIssue = std::ranges::find_if(ReportIssues, [](const BuildValidationIssue& Issue)
                                                        { return Issue.Severity == EBuildValidationSeverity::Error; });
        if (BlockingIssue != ReportIssues.end())
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        return Report;
    }

    std::vector<BuildValidationIssue> BuildHistoryService::Validate(const BuildHistoryEntry& Entry)
    {
        std::vector<BuildValidationIssue> Issues{};

        if (TrimCopy(Entry.BuildId).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleEntryBuildIdMissing,
                        "Build history entries require a non-empty build id.");
        }
        if (Entry.HistoryDirectory.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleEntryHistoryDirectoryMissing,
                        "Build history entries require a non-empty history directory.");
        }
        if (Entry.State == EBuildHistoryEntryState::Complete && Entry.BuildReportFilePath.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleEntryReportPathMissing,
                        "Completed build history entries require a build-report artifact path.");
        }

        return Issues;
    }

    BuildHistoryComparison BuildHistoryService::Compare(const BuildExecutionReport& Left,
                                                        const BuildExecutionReport& Right)
    {
        BuildHistoryComparison Comparison{};
        Comparison.LeftBuildId = Left.BuildId;
        Comparison.RightBuildId = Right.BuildId;
        Comparison.SameRequestHash = Left.RequestHash == Right.RequestHash;
        Comparison.SameStatus = Left.Status == Right.Status;

        std::set<std::string> LeftOutputs(Left.OutputFiles.begin(), Left.OutputFiles.end());
        std::set<std::string> RightOutputs(Right.OutputFiles.begin(), Right.OutputFiles.end());

        std::ranges::set_difference(RightOutputs, LeftOutputs,
                                    std::back_inserter(Comparison.AddedOutputFiles));
        std::ranges::set_difference(LeftOutputs, RightOutputs,
                                    std::back_inserter(Comparison.RemovedOutputFiles));

        std::map<std::uint32_t, const BuildNodeExecutionRecord*> LeftNodes{};
        std::map<std::uint32_t, const BuildNodeExecutionRecord*> RightNodes{};
        for (const BuildNodeExecutionRecord& Record : Left.NodeRecords)
        {
            LeftNodes.emplace(Record.NodeId, &Record);
        }
        for (const BuildNodeExecutionRecord& Record : Right.NodeRecords)
        {
            RightNodes.emplace(Record.NodeId, &Record);
        }

        std::set<std::uint32_t> AllNodeIds{};
        for (const auto& [NodeId, _] : LeftNodes)
        {
            AllNodeIds.insert(NodeId);
        }
        for (const auto& [NodeId, _] : RightNodes)
        {
            AllNodeIds.insert(NodeId);
        }

        for (const std::uint32_t NodeId : AllNodeIds)
        {
            const auto LeftIt = LeftNodes.find(NodeId);
            const auto RightIt = RightNodes.find(NodeId);
            const BuildNodeExecutionRecord* LeftRecord = LeftIt != LeftNodes.end() ? LeftIt->second : nullptr;
            const BuildNodeExecutionRecord* RightRecord = RightIt != RightNodes.end() ? RightIt->second : nullptr;

            const bool LeftPresent = LeftRecord != nullptr;
            const bool RightPresent = RightRecord != nullptr;
            const EBuildNodeExecutionStatus LeftStatus =
                LeftPresent ? LeftRecord->Status : EBuildNodeExecutionStatus::Cancelled;
            const EBuildNodeExecutionStatus RightStatus =
                RightPresent ? RightRecord->Status : EBuildNodeExecutionStatus::Cancelled;
            const bool LeftCacheHit = LeftPresent && LeftRecord->CacheHit;
            const bool RightCacheHit = RightPresent && RightRecord->CacheHit;

            if (LeftPresent == RightPresent && LeftStatus == RightStatus && LeftCacheHit == RightCacheHit)
            {
                continue;
            }

            Comparison.NodeDeltas.push_back(BuildHistoryNodeDelta{
                .NodeId = NodeId,
                .Name = LeftPresent ? LeftRecord->Name : RightRecord->Name,
                .Type = LeftPresent ? LeftRecord->Type : RightRecord->Type,
                .LeftPresent = LeftPresent,
                .RightPresent = RightPresent,
                .LeftStatus = LeftStatus,
                .RightStatus = RightStatus,
                .LeftCacheHit = LeftCacheHit,
                .RightCacheHit = RightCacheHit,
            });
        }

        return Comparison;
    }

} // namespace SnAPI::GameFramework
