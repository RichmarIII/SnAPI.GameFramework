#include "BuildCli.h"

#include "BuildHistory.h"
#include "ModuleCreationService.h"
#include "PluginCreationService.h"
#include "ProjectCreationService.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

namespace SnAPI::GameFramework
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        /**
         * @brief Parsed global CLI-output options applied before command dispatch.
         */
        struct CliGlobalOptions
        {
            bool JsonOutput = false;
            bool IncludeEventPayloads = false;
        };

        /**
         * @brief Parsed command-line options shared by validate/package commands.
         */
        struct BuildInvocationArguments
        {
            BuildRequest Request{};
            std::filesystem::path OutputRootDirectory{};
            std::filesystem::path EngineSourceDirectory{};
            std::string Generator{};
            std::vector<std::string> BuildTargets{};
            std::string BuildId{};
            bool PlanOnly = false;
            bool SkipCode = false;
            bool SkipAssets = false;
            bool ArchiveRequested = false;
        };

        /**
         * @brief Parsed retry command options.
         */
        struct RetryInvocationArguments
        {
            std::filesystem::path ProjectFilePath{};
            std::string SourceBuildId{};
            std::filesystem::path OutputRootDirectory{};
            std::filesystem::path EngineSourceDirectory{};
            std::string Generator{};
            std::vector<std::string> BuildTargets{};
            std::string BuildId{};
            bool PlanOnly = false;
            bool RebuildAll = false;
            bool SkipCode = false;
            bool SkipAssets = false;
            bool ArchiveRequested = false;
        };

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
         * @brief Convert one string to lowercase ASCII for option parsing.
         * @param Text Source text.
         * @return Lowercase copy.
         */
        [[nodiscard]] std::string LowercaseCopy(const std::string_view Text)
        {
            std::string Result(Text);
            std::ranges::transform(Result, Result.begin(), [](const unsigned char Character)
                                   { return static_cast<char>(std::tolower(Character)); });
            return Result;
        }

        /**
         * @brief Parse and remove global output flags from one CLI invocation.
         * @param Arguments Full argument list after the optional `build` token.
         * @param OutCommandArguments Command arguments with global flags removed.
         * @return Parsed global output options.
         */
        [[nodiscard]] CliGlobalOptions ParseGlobalOptions(const std::vector<std::string>& Arguments,
                                                          std::vector<std::string>& OutCommandArguments)
        {
            CliGlobalOptions Options{};
            OutCommandArguments.clear();
            OutCommandArguments.reserve(Arguments.size());
            for (const std::string& Arg : Arguments)
            {
                if (Arg == "--json")
                {
                    Options.JsonOutput = true;
                    continue;
                }
                if (Arg == "--json-events")
                {
                    Options.JsonOutput = true;
                    Options.IncludeEventPayloads = true;
                    continue;
                }
                OutCommandArguments.push_back(Arg);
            }
            return Options;
        }

        /**
         * @brief Resolve one possibly-relative path against a working directory.
         * @param CurrentWorkingDirectory Working directory used for relative paths.
         * @param InputPath Raw path text.
         * @return Normalized absolute-like path.
         */
        [[nodiscard]] std::filesystem::path ResolvePathAgainst(const std::filesystem::path& CurrentWorkingDirectory,
                                                               const std::filesystem::path& InputPath)
        {
            if (InputPath.empty())
            {
                return {};
            }

            if (InputPath.is_absolute())
            {
                return InputPath.lexically_normal();
            }

            const std::filesystem::path Base = CurrentWorkingDirectory.empty() ? std::filesystem::current_path()
                                                                               : CurrentWorkingDirectory;
            return (Base / InputPath).lexically_normal();
        }

        /**
         * @brief Resolve the parent directory used by project/plugin creation from a destination path.
         * @param CurrentWorkingDirectory Working directory used for relative paths.
         * @param DestinationPath CLI destination argument.
         * @param ExpectedLeafName Requested project or plugin name.
         * @return Parent directory passed to the creation service.
         *
         * The CLI accepts either a parent directory or an explicit final root path.
         * When the destination leaf already matches the requested name, the service
         * receives that path's parent so the resulting workspace lands exactly there.
         */
        [[nodiscard]] std::filesystem::path ResolveCreationParentDirectory(
            const std::filesystem::path& CurrentWorkingDirectory, const std::filesystem::path& DestinationPath,
            const std::string_view ExpectedLeafName)
        {
            const std::filesystem::path ResolvedDestination =
                ResolvePathAgainst(CurrentWorkingDirectory, DestinationPath).lexically_normal();
            if (ResolvedDestination.filename() == ExpectedLeafName)
            {
                return ResolvedDestination.parent_path().empty() ? std::filesystem::current_path()
                                                                 : ResolvedDestination.parent_path().lexically_normal();
            }
            return ResolvedDestination;
        }

        /**
         * @brief Append one repeated string option into a profile list patch.
         * @param Destination Destination list patch.
         * @param Value Option value to append.
         */
        void AppendListValue(BuildProfileStringList& Destination, std::string Value)
        {
            Destination.IsSet = true;
            Destination.Values.push_back(TrimCopy(Value));
        }

        /**
         * @brief Build one authored scalar override with a concrete string value.
         * @param Value Concrete override value.
         * @return Authored scalar override.
         */
        [[nodiscard]] BuildProfileValue<std::string> SetStringValue(std::string Value)
        {
            return BuildProfileValue<std::string>{
                .IsSet = true,
                .Value = TrimCopy(Value),
            };
        }

        /**
         * @brief Build one authored scalar override with a concrete boolean value.
         * @param Value Concrete override value.
         * @return Authored scalar override.
         */
        [[nodiscard]] BuildProfileValue<bool> SetBoolValue(const bool Value)
        {
            return BuildProfileValue<bool>{
                .IsSet = true,
                .Value = Value,
            };
        }

        /**
         * @brief Build one authored scalar override with a concrete build configuration value.
         * @param Value Concrete override value.
         * @return Authored scalar override.
         */
        [[nodiscard]] BuildProfileValue<EBuildConfiguration> SetConfigurationValue(const EBuildConfiguration Value)
        {
            return BuildProfileValue<EBuildConfiguration>{
                .IsSet = true,
                .Value = Value,
            };
        }

        /**
         * @brief Parse one CLI build-configuration token.
         * @param Text Raw option text.
         * @return Parsed build configuration or a structured validation error.
         */
        [[nodiscard]] TExpected<EBuildConfiguration> ParseBuildConfiguration(const std::string_view Text)
        {
            const std::string Lower = LowercaseCopy(Text);
            if (Lower == "debug")
            {
                return EBuildConfiguration::Debug;
            }
            if (Lower == "development")
            {
                return EBuildConfiguration::Development;
            }
            if (Lower == "test")
            {
                return EBuildConfiguration::Test;
            }
            if (Lower == "shipping")
            {
                return EBuildConfiguration::Shipping;
            }
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Unknown build configuration: " + std::string(Text)));
        }

        /**
         * @brief Parse one CLI module-type token.
         * @param Text Raw option text.
         * @return Parsed module type or a structured validation error.
         */
        [[nodiscard]] TExpected<EProjectModuleType> ParseModuleType(const std::string_view Text)
        {
            const std::string Lower = LowercaseCopy(Text);
            if (Lower == "runtime")
            {
                return EProjectModuleType::Runtime;
            }
            if (Lower == "editor")
            {
                return EProjectModuleType::Editor;
            }
            if (Lower == "shared")
            {
                return EProjectModuleType::Shared;
            }
            if (Lower == "developer")
            {
                return EProjectModuleType::Developer;
            }
            if (Lower == "test")
            {
                return EProjectModuleType::Test;
            }
            if (Lower == "program")
            {
                return EProjectModuleType::Program;
            }
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown module type: " + std::string(Text)));
        }

        /**
         * @brief Return the default CLI working directory used for relative paths.
         * @param Options Command options provided to the CLI service.
         * @return Effective working directory.
         */
        [[nodiscard]] std::filesystem::path EffectiveWorkingDirectory(const BuildCliOptions& Options)
        {
            return Options.CurrentWorkingDirectory.empty() ? std::filesystem::current_path()
                                                           : Options.CurrentWorkingDirectory.lexically_normal();
        }

        /**
         * @brief Load one resolved project descriptor from a project-file path.
         * @param ProjectFilePath Project descriptor file path.
         * @return Resolved descriptor or a structured error.
         */
        [[nodiscard]] TExpected<ResolvedProjectDescriptor> LoadResolvedProject(const std::filesystem::path& ProjectFilePath)
        {
            if (ProjectFilePath.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project descriptor path is empty."));
            }
            return ProjectDescriptorService::LoadResolved(ProjectFilePath.string());
        }

        /**
         * @brief Resolve the canonical build-report path for one project history entry.
         * @param ProjectFilePath Project descriptor file path.
         * @param BuildId Build invocation id under the project's history directory.
         * @return Build-report path or a structured descriptor error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveBuildReportPath(const std::filesystem::path& ProjectFilePath,
                                                                              const std::string_view BuildId)
        {
            auto Project = LoadResolvedProject(ProjectFilePath);
            if (!Project)
            {
                return std::unexpected(Project.error());
            }
            return (Project->SavedRootDirectory / "BuildHistory" / TrimCopy(BuildId) / "BuildReport.json").lexically_normal();
        }

        /**
         * @brief Resolve the canonical frozen-request path for one project history entry.
         * @param ProjectFilePath Project descriptor file path.
         * @param BuildId Build invocation id under the project's history directory.
         * @return Build-request artifact path or a structured descriptor error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveBuildRequestPath(const std::filesystem::path& ProjectFilePath,
                                                                               const std::string_view BuildId)
        {
            auto Project = LoadResolvedProject(ProjectFilePath);
            if (!Project)
            {
                return std::unexpected(Project.error());
            }
            return (Project->SavedRootDirectory / "BuildHistory" / TrimCopy(BuildId) / "BuildRequest.json").lexically_normal();
        }

        /**
         * @brief Read the next required option value from one argument list.
         * @param Arguments Full argument list.
         * @param Index Current parsing index, advanced on success.
         * @param OptionName Name of the current option for diagnostics.
         * @return Option value or a structured parsing error.
         */
        [[nodiscard]] TExpected<std::string> TakeRequiredValue(const std::vector<std::string>& Arguments,
                                                               std::size_t& Index, const std::string_view OptionName)
        {
            if (Index + 1u >= Arguments.size())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Missing value for option '" + std::string(OptionName) + "'."));
            }
            return Arguments[++Index];
        }

        /**
         * @brief Collect validation issues into one human-readable text block.
         * @param Issues Validation issues to print.
         * @return Multi-line issue text.
         */
        [[nodiscard]] std::string FormatIssues(const std::vector<BuildValidationIssue>& Issues)
        {
            std::ostringstream Stream{};
            for (const BuildValidationIssue& Issue : Issues)
            {
                const char* Severity = Issue.Severity == EBuildValidationSeverity::Info    ? "info"
                                       : Issue.Severity == EBuildValidationSeverity::Warning ? "warning"
                                                                                              : "error";
                Stream << "- [" << Severity << "] " << Issue.RuleId << ": " << Issue.Message << "\n";
            }
            return Stream.str();
        }

        /**
         * @brief Convert one framework error into a CLI exit code.
         * @param ErrorValue Framework error to classify.
         * @return CLI exit code.
         */
        [[nodiscard]] EBuildCliExitCode ExitCodeFromError(const Error& ErrorValue)
        {
            switch (ErrorValue.Code)
            {
            case EErrorCode::InvalidArgument:
            case EErrorCode::NotFound:
            case EErrorCode::AlreadyExists:
            case EErrorCode::OutOfRange:
            case EErrorCode::TypeMismatch:
                return EBuildCliExitCode::ValidationFailed;
            case EErrorCode::NotReady:
            case EErrorCode::InternalError:
                return EBuildCliExitCode::InternalError;
            case EErrorCode::None:
            default:
                return EBuildCliExitCode::Success;
            }
        }

        /**
         * @brief Convert one build-event kind into canonical text for CLI JSON output.
         * @param Kind Event kind to stringify.
         * @return Canonical event-kind text.
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
         * @brief Convert one build stage into canonical text for CLI JSON output.
         * @param Stage Stage to stringify.
         * @return Canonical stage text.
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
         * @brief Convert one validation severity into canonical text for CLI JSON output.
         * @param Severity Validation severity to stringify.
         * @return Canonical severity text.
         */
        [[nodiscard]] std::string ToString(const EBuildValidationSeverity Severity)
        {
            switch (Severity)
            {
            case EBuildValidationSeverity::Info:
                return "Info";
            case EBuildValidationSeverity::Warning:
                return "Warning";
            case EBuildValidationSeverity::Error:
                return "Error";
            }
            return "Info";
        }

        /**
         * @brief Convert one execution status into canonical text for CLI JSON output.
         * @param Status Execution status to stringify.
         * @return Canonical status text.
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
         * @brief Convert one CLI exit code into canonical text for JSON output.
         * @param ExitCode Exit code to stringify.
         * @return Canonical exit-code text.
         */
        [[nodiscard]] std::string ToString(const EBuildCliExitCode ExitCode)
        {
            switch (ExitCode)
            {
            case EBuildCliExitCode::Success:
                return "Success";
            case EBuildCliExitCode::InvalidArguments:
                return "InvalidArguments";
            case EBuildCliExitCode::ValidationFailed:
                return "ValidationFailed";
            case EBuildCliExitCode::BuildFailed:
                return "BuildFailed";
            case EBuildCliExitCode::InternalError:
                return "InternalError";
            }
            return "Success";
        }

        /**
         * @brief Serialize one structured build event into ordered JSON.
         * @param Event Event to serialize.
         * @return Ordered JSON object.
         */
        [[nodiscard]] Json SerializeEvent(const BuildEvent& Event)
        {
            return Json::object({
                {"Kind", ToString(Event.Kind)},
                {"Severity", ToString(Event.Severity)},
                {"TimestampUtc", Event.TimestampUtc},
                {"Stage", ToString(Event.Stage)},
                {"NodeId", Event.NodeId},
                {"Message", Event.Message},
                {"Payload", Event.Payload},
            });
        }

        /**
         * @brief Append structured build events when the caller requested them.
         * @param Root Destination JSON object.
         * @param GlobalOptions Global CLI-output options.
         * @param Events Captured structured events.
         */
        void AppendEventsIfRequested(Json& Root, const CliGlobalOptions& GlobalOptions, const std::vector<BuildEvent>& Events)
        {
            if (!GlobalOptions.IncludeEventPayloads)
            {
                return;
            }

            Root["Events"] = Json::array();
            for (const BuildEvent& Event : Events)
            {
                Root["Events"].push_back(SerializeEvent(Event));
            }
        }

        /**
         * @brief Append standard request-override options into one build invocation model.
         * @param Arguments Remaining command arguments.
         * @param CurrentWorkingDirectory Working directory used for relative paths.
         * @param Invocation Destination invocation model.
         * @return Success or a structured parsing error.
         */
        [[nodiscard]] Result ParseBuildInvocationArguments(const std::vector<std::string>& Arguments,
                                                           const std::filesystem::path& CurrentWorkingDirectory,
                                                           BuildInvocationArguments& Invocation)
        {
            for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
            {
                const std::string& Arg = Arguments[Index];
                if (Arg == "--project")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.Request.ProjectFilePath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--profile")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.Request.ProfileName = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--platform")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.Request.Overrides.Platform = SetStringValue(*Value);
                    continue;
                }
                if (Arg == "--container" || Arg == "--execution-environment")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.Request.Overrides.ExecutionEnvironment = SetStringValue(*Value);
                    continue;
                }
                if (Arg == "--config")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    auto Parsed = ParseBuildConfiguration(*Value);
                    if (!Parsed)
                    {
                        return std::unexpected(Parsed.error());
                    }
                    Invocation.Request.Overrides.Configuration = SetConfigurationValue(*Parsed);
                    continue;
                }
                if (Arg == "--level" || Arg == "--levels")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.SelectedLevels, *Value);
                    continue;
                }
                if (Arg == "--asset")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.ExplicitAssets, *Value);
                    continue;
                }
                if (Arg == "--include-folder")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.IncludeFolders, *Value);
                    continue;
                }
                if (Arg == "--exclude-folder")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.ExcludeFolders, *Value);
                    continue;
                }
                if (Arg == "--include-label")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.IncludeAssetLabels, *Value);
                    continue;
                }
                if (Arg == "--exclude-label")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.ExcludeAssetLabels, *Value);
                    continue;
                }
                if (Arg == "--include-kind")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.IncludeAssetKinds, *Value);
                    continue;
                }
                if (Arg == "--exclude-kind")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    AppendListValue(Invocation.Request.Overrides.ExcludeAssetKinds, *Value);
                    continue;
                }
                if (Arg == "--archive")
                {
                    Invocation.ArchiveRequested = true;
                    Invocation.Request.Overrides.Archive.IsSet = true;
                    Invocation.Request.Overrides.Archive.Enabled = SetBoolValue(true);
                    Invocation.Request.Overrides.Archive.Format = SetStringValue("zip");
                    continue;
                }
                if (Arg == "--archive-format")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.ArchiveRequested = true;
                    Invocation.Request.Overrides.Archive.IsSet = true;
                    Invocation.Request.Overrides.Archive.Enabled = SetBoolValue(true);
                    Invocation.Request.Overrides.Archive.Format = SetStringValue(*Value);
                    continue;
                }
                if (Arg == "--dest")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.OutputRootDirectory = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--engine-root")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.EngineSourceDirectory = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--generator")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.Generator = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--build-target")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.BuildTargets.push_back(TrimCopy(*Value));
                    continue;
                }
                if (Arg == "--build-id")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.BuildId = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--plan-only")
                {
                    Invocation.PlanOnly = true;
                    continue;
                }
                if (Arg == "--skip-code")
                {
                    Invocation.SkipCode = true;
                    continue;
                }
                if (Arg == "--skip-assets")
                {
                    Invocation.SkipAssets = true;
                    continue;
                }

                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Unknown option for build command: " + Arg));
            }

            if (Invocation.Request.ProjectFilePath.empty())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "The --project option is required for this command."));
            }

            return Ok();
        }

        /**
         * @brief Parse the retry command options.
         * @param Arguments Remaining command arguments.
         * @param CurrentWorkingDirectory Working directory used for relative paths.
         * @param Invocation Destination retry invocation model.
         * @return Success or a structured parsing error.
         */
        [[nodiscard]] Result ParseRetryInvocationArguments(const std::vector<std::string>& Arguments,
                                                           const std::filesystem::path& CurrentWorkingDirectory,
                                                           RetryInvocationArguments& Invocation)
        {
            for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
            {
                const std::string& Arg = Arguments[Index];
                if (Arg == "--project")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.ProjectFilePath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--from-build-id")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.SourceBuildId = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--dest")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.OutputRootDirectory = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--engine-root")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.EngineSourceDirectory = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--generator")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.Generator = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--build-target")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.BuildTargets.push_back(TrimCopy(*Value));
                    continue;
                }
                if (Arg == "--build-id")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.BuildId = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--plan-only")
                {
                    Invocation.PlanOnly = true;
                    continue;
                }
                if (Arg == "--rebuild-all")
                {
                    Invocation.RebuildAll = true;
                    continue;
                }
                if (Arg == "--skip-code")
                {
                    Invocation.SkipCode = true;
                    continue;
                }
                if (Arg == "--skip-assets")
                {
                    Invocation.SkipAssets = true;
                    continue;
                }
                if (Arg == "--archive")
                {
                    Invocation.ArchiveRequested = true;
                    continue;
                }
                if (Arg == "--archive-format")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        return std::unexpected(Value.error());
                    }
                    Invocation.ArchiveRequested = true;
                    continue;
                }

                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown option for retry: " + Arg));
            }

            if (Invocation.ProjectFilePath.empty() || TrimCopy(Invocation.SourceBuildId).empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "The --project and --from-build-id options are required for retry."));
            }

            return Ok();
        }

        /**
         * @brief Run one validate command using the shared request/planner services.
         * @param Arguments Command arguments after the `validate` token.
         * @param Options Base CLI options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunValidate(const std::vector<std::string>& Arguments,
                                                 const BuildCliOptions& Options,
                                                 const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);

            BuildInvocationArguments Invocation{};
            if (const Result ParseResult = ParseBuildInvocationArguments(Arguments, CurrentWorkingDirectory, Invocation);
                !ParseResult)
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = ParseResult.error().Message + "\n";
                return CliResult;
            }

            auto Resolved = BuildRequestService::Resolve(Invocation.Request, Options.MaxProfileInheritanceDepth);
            if (!Resolved)
            {
                CliResult.ExitCode = ExitCodeFromError(Resolved.error());
                CliResult.StandardError = Resolved.error().Message + "\n";
                return CliResult;
            }

            BuildPlannerOptions PlannerOptions = Options.Planner;
            if (!TrimCopy(Invocation.BuildId).empty())
            {
                PlannerOptions.BuildId = Invocation.BuildId;
            }

            auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
            if (!Graph)
            {
                CliResult.ExitCode = ExitCodeFromError(Graph.error());
                CliResult.StandardError = Graph.error().Message + "\n";
                return CliResult;
            }

            const auto RequestIssues = BuildRequestService::Validate(*Resolved);
            const auto GraphIssues = BuildPlannerService::Validate(*Graph);
            std::vector<BuildValidationIssue> Issues = RequestIssues;
            Issues.insert(Issues.end(), GraphIssues.begin(), GraphIssues.end());

            CliResult.PlannedGraph = *Graph;
            CliResult.ArtifactPaths.push_back(Resolved->Project.ProjectFilePath);

            const bool HasErrors = std::ranges::any_of(Issues, [](const BuildValidationIssue& Issue)
                                                       { return Issue.Severity == EBuildValidationSeverity::Error; });
            CliResult.ExitCode = HasErrors ? EBuildCliExitCode::ValidationFailed : EBuildCliExitCode::Success;
            if (GlobalOptions.JsonOutput)
            {
                auto GraphText = BuildPlannerService::Serialize(*Graph, 2);
                Json Root = Json::object({
                    {"Command", "validate"},
                    {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                    {"ExitCodeName", ToString(CliResult.ExitCode)},
                    {"Succeeded", !HasErrors},
                    {"ProjectFile", Resolved->Project.ProjectFilePath.lexically_normal().generic_string()},
                    {"BuildId", Graph->BuildId},
                    {"RequestHash", Resolved->RequestHash},
                    {"Issues", Json::array()},
                });
                if (GraphText)
                {
                    Root["Plan"] = Json::parse(*GraphText, nullptr, false);
                }
                for (const BuildValidationIssue& Issue : Issues)
                {
                    Root["Issues"].push_back(Json::object({
                        {"Severity", ToString(Issue.Severity)},
                        {"RuleId", Issue.RuleId},
                        {"Message", Issue.Message},
                    }));
                }
                CliResult.StandardOutput = Root.dump(2) + "\n";
            }
            else
            {
                std::ostringstream Stream{};
                Stream << "Validation ";
                Stream << (HasErrors ? "failed" : "succeeded") << " for `"
                       << Resolved->Project.ProjectFilePath.lexically_normal().string() << "`.\n";
                Stream << "BuildId: `" << Graph->BuildId << "`\n";
                Stream << "RequestHash: `" << Resolved->RequestHash << "`\n";
                if (!Issues.empty())
                {
                    Stream << FormatIssues(Issues);
                }
                CliResult.StandardOutput = Stream.str();
            }
            return CliResult;
        }

        /**
         * @brief Run one package command using the shared request/planner/execution services.
         * @param Arguments Command arguments after the `package` token.
         * @param Options Base CLI options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunPackage(const std::vector<std::string>& Arguments,
                                                const BuildCliOptions& Options,
                                                const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);

            BuildInvocationArguments Invocation{};
            if (const Result ParseResult = ParseBuildInvocationArguments(Arguments, CurrentWorkingDirectory, Invocation);
                !ParseResult)
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = ParseResult.error().Message + "\n";
                return CliResult;
            }

            auto Resolved = BuildRequestService::Resolve(Invocation.Request, Options.MaxProfileInheritanceDepth);
            if (!Resolved)
            {
                CliResult.ExitCode = ExitCodeFromError(Resolved.error());
                CliResult.StandardError = Resolved.error().Message + "\n";
                return CliResult;
            }

            BuildPlannerOptions PlannerOptions = Options.Planner;
            if (!TrimCopy(Invocation.BuildId).empty())
            {
                PlannerOptions.BuildId = Invocation.BuildId;
            }

            auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
            if (!Graph)
            {
                CliResult.ExitCode = ExitCodeFromError(Graph.error());
                CliResult.StandardError = Graph.error().Message + "\n";
                return CliResult;
            }

            CliResult.PlannedGraph = *Graph;
            if (Invocation.PlanOnly)
            {
                CliResult.ExitCode = EBuildCliExitCode::Success;
                if (GlobalOptions.JsonOutput)
                {
                    auto GraphText = BuildPlannerService::Serialize(*Graph, 2);
                    Json Root = Json::object({
                        {"Command", "package"},
                        {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                        {"ExitCodeName", ToString(CliResult.ExitCode)},
                        {"PlanOnly", true},
                        {"ProjectFile", Resolved->Project.ProjectFilePath.lexically_normal().generic_string()},
                        {"BuildId", Graph->BuildId},
                        {"RequestHash", Resolved->RequestHash},
                        {"HistoryDirectory", Graph->HistoryDirectory.lexically_normal().generic_string()},
                        {"StageDirectory", Graph->StageDirectory.lexically_normal().generic_string()},
                    });
                    if (GraphText)
                    {
                        Root["Plan"] = Json::parse(*GraphText, nullptr, false);
                    }
                    CliResult.StandardOutput = Root.dump(2) + "\n";
                }
                else
                {
                    std::ostringstream Stream{};
                    Stream << "Build plan created for `" << Resolved->Project.ProjectFilePath.lexically_normal().string()
                           << "`.\n";
                    Stream << "BuildId: `" << Graph->BuildId << "`\n";
                    Stream << "RequestHash: `" << Resolved->RequestHash << "`\n";
                    Stream << "HistoryDirectory: `" << Graph->HistoryDirectory.lexically_normal().string() << "`\n";
                    Stream << "StageDirectory: `" << Graph->StageDirectory.lexically_normal().string() << "`\n";
                    CliResult.StandardOutput = Stream.str();
                }
                return CliResult;
            }

            BuildExecutionOptions ExecutionOptions = Options.Execution;
            std::vector<BuildEvent> Events{};
            const auto ExistingSink = ExecutionOptions.EventSink;
            if (GlobalOptions.JsonOutput || GlobalOptions.IncludeEventPayloads)
            {
                ExecutionOptions.EventSink = [&](const BuildEvent& Event)
                {
                    Events.push_back(Event);
                    if (ExistingSink)
                    {
                        ExistingSink(Event);
                    }
                };
            }
            ExecutionOptions.CodeBuild.Enabled = !Invocation.SkipCode;
            ExecutionOptions.AssetCook.Enabled = !Invocation.SkipAssets;
            if (!Invocation.EngineSourceDirectory.empty())
            {
                ExecutionOptions.CodeBuild.EngineSourceDirectory = Invocation.EngineSourceDirectory;
            }
            if (!TrimCopy(Invocation.Generator).empty())
            {
                ExecutionOptions.CodeBuild.Generator = Invocation.Generator;
            }
            if (!Invocation.BuildTargets.empty())
            {
                ExecutionOptions.CodeBuild.BuildTargets = Invocation.BuildTargets;
            }
            if (!Invocation.OutputRootDirectory.empty())
            {
                ExecutionOptions.PackageOutput.OutputRootDirectory = Invocation.OutputRootDirectory;
            }
            if (Invocation.ArchiveRequested)
            {
                ExecutionOptions.PackageOutput.ArchiveEnabled = true;
                if (Invocation.Request.Overrides.Archive.Format.IsSet &&
                    Invocation.Request.Overrides.Archive.Format.Value.has_value())
                {
                    ExecutionOptions.PackageOutput.ArchiveFormat =
                        *Invocation.Request.Overrides.Archive.Format.Value;
                }
            }

            auto Report = BuildExecutionService::Execute(*Resolved, *Graph, ExecutionOptions);
            if (!Report)
            {
                CliResult.ExitCode = ExitCodeFromError(Report.error());
                CliResult.StandardError = Report.error().Message + "\n";
                return CliResult;
            }

            CliResult.ExecutionReport = *Report;
            CliResult.ArtifactPaths.push_back(Report->BuildReportFilePath);
            if (!Report->PackageDirectoryPath.empty())
            {
                CliResult.ArtifactPaths.push_back(Report->PackageDirectoryPath);
            }
            if (!Report->ArchiveFilePath.empty())
            {
                CliResult.ArtifactPaths.push_back(Report->ArchiveFilePath);
            }

            CliResult.ExitCode = Report->Status == EBuildExecutionStatus::Succeeded ? EBuildCliExitCode::Success
                                                                                    : EBuildCliExitCode::BuildFailed;
            if (GlobalOptions.JsonOutput)
            {
                auto ReportText = BuildExecutionService::SerializeReport(*Report, 2);
                Json Root = Json::object({
                    {"Command", "package"},
                    {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                    {"ExitCodeName", ToString(CliResult.ExitCode)},
                    {"PlanOnly", false},
                });
                if (ReportText)
                {
                    Root["Report"] = Json::parse(*ReportText, nullptr, false);
                }
                AppendEventsIfRequested(Root, GlobalOptions, Events);
                CliResult.StandardOutput = Root.dump(2) + "\n";
            }
            else
            {
                std::ostringstream Stream{};
                Stream << "Package build finished with status `" << ToString(Report->Status) << "`.\n";
                Stream << "BuildId: `" << Report->BuildId << "`\n";
                Stream << "HistoryDirectory: `" << Report->HistoryDirectory.lexically_normal().string() << "`\n";
                Stream << "StageDirectory: `" << Report->StageDirectory.lexically_normal().string() << "`\n";
                if (!Report->PackageDirectoryPath.empty())
                {
                    Stream << "PackageDirectory: `" << Report->PackageDirectoryPath.lexically_normal().string() << "`\n";
                }
                if (!Report->ArchiveFilePath.empty())
                {
                    Stream << "Archive: `" << Report->ArchiveFilePath.lexically_normal().string() << "`\n";
                }
                CliResult.StandardOutput = Stream.str();
            }
            return CliResult;
        }

        /**
         * @brief Run one create-project command using `ProjectCreationService`.
         * @param Arguments Command arguments after the `create-project` token.
         * @param Options Base CLI options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunCreateProject(const std::vector<std::string>& Arguments,
                                                      const BuildCliOptions& Options,
                                                      const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);

            std::string ProjectName{};
            std::filesystem::path DestinationPath{};
            std::string DisplayName{};
            std::string Company{};
            std::string Description{};
            std::string StartupLevel{};
            std::string RuntimeModuleName{};
            std::string NamespaceRoot{};
            bool CreateEditorModule = false;
            std::string EditorModuleName{};

            for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
            {
                const std::string& Arg = Arguments[Index];
                if (Arg == "--name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    ProjectName = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--dest")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    DestinationPath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--display-name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    DisplayName = *Value;
                    continue;
                }
                if (Arg == "--company")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    Company = *Value;
                    continue;
                }
                if (Arg == "--description")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    Description = *Value;
                    continue;
                }
                if (Arg == "--startup-level")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    StartupLevel = *Value;
                    continue;
                }
                if (Arg == "--runtime-module")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    RuntimeModuleName = *Value;
                    continue;
                }
                if (Arg == "--namespace")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    NamespaceRoot = *Value;
                    continue;
                }
                if (Arg == "--editor-module")
                {
                    CreateEditorModule = true;
                    continue;
                }
                if (Arg == "--editor-module-name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    EditorModuleName = *Value;
                    continue;
                }

                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "Unknown option for create-project: " + Arg + "\n";
                return CliResult;
            }

            if (ProjectName.empty() || DestinationPath.empty())
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "The --name and --dest options are required for create-project.\n";
                return CliResult;
            }

            auto Descriptor = ProjectCreationService::BuildDefaultDescriptor(ProjectName);
            if (!Descriptor)
            {
                CliResult.ExitCode = ExitCodeFromError(Descriptor.error());
                CliResult.StandardError = Descriptor.error().Message + "\n";
                return CliResult;
            }

            if (!TrimCopy(DisplayName).empty())
            {
                Descriptor->Project.DisplayName = TrimCopy(DisplayName);
            }
            if (!TrimCopy(Company).empty())
            {
                Descriptor->Project.Company = TrimCopy(Company);
            }
            if (!TrimCopy(Description).empty())
            {
                Descriptor->Project.Description = TrimCopy(Description);
            }
            if (!TrimCopy(StartupLevel).empty())
            {
                Descriptor->Startup.StartupLevelAsset = TrimCopy(StartupLevel);
            }

            ProjectCreationRequest Request{};
            Request.ProjectName = ProjectName;
            Request.ParentDirectory = ResolveCreationParentDirectory(CurrentWorkingDirectory, DestinationPath, ProjectName);
            Request.Descriptor = *Descriptor;
            Request.Code.RuntimeModuleName = RuntimeModuleName;
            Request.Code.NamespaceRoot = NamespaceRoot;
            Request.Code.CreateStarterEditorModule = CreateEditorModule;
            Request.Code.EditorModuleName = EditorModuleName;

            ProjectCreationResult CreateResult{};
            const Result CreateProjectResult = ProjectCreationService::CreateProject(Request, &CreateResult);
            if (!CreateProjectResult)
            {
                CliResult.ExitCode = ExitCodeFromError(CreateProjectResult.error());
                CliResult.StandardError = CreateProjectResult.error().Message + "\n";
                return CliResult;
            }

            CliResult.ArtifactPaths.push_back(CreateResult.Project.ProjectFilePath);
            CliResult.ArtifactPaths.insert(CliResult.ArtifactPaths.end(), CreateResult.GeneratedFiles.begin(),
                                           CreateResult.GeneratedFiles.end());

            CliResult.ExitCode = EBuildCliExitCode::Success;
            if (GlobalOptions.JsonOutput)
            {
                Json Root = Json::object({
                    {"Command", "create-project"},
                    {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                    {"ExitCodeName", ToString(CliResult.ExitCode)},
                    {"ProjectName", CreateResult.Project.Descriptor.Project.Name},
                    {"ProjectFile", CreateResult.Project.ProjectFilePath.lexically_normal().generic_string()},
                    {"ProjectRoot", CreateResult.Project.ProjectRootDirectory.lexically_normal().generic_string()},
                    {"Artifacts", Json::array()},
                });
                for (const auto& Path : CliResult.ArtifactPaths)
                {
                    Root["Artifacts"].push_back(Path.lexically_normal().generic_string());
                }
                CliResult.StandardOutput = Root.dump(2) + "\n";
            }
            else
            {
                std::ostringstream Stream{};
                Stream << "Created project `" << CreateResult.Project.Descriptor.Project.Name << "`.\n";
                Stream << "ProjectFile: `" << CreateResult.Project.ProjectFilePath.lexically_normal().string()
                       << "`\n";
                Stream << "ProjectRoot: `" << CreateResult.Project.ProjectRootDirectory.lexically_normal().string()
                       << "`\n";
                CliResult.StandardOutput = Stream.str();
            }
            return CliResult;
        }

        /**
         * @brief Run one create-plugin command using `PluginCreationService`.
         * @param Arguments Command arguments after the `create-plugin` token.
         * @param Options Base CLI options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunCreatePlugin(const std::vector<std::string>& Arguments,
                                                     const BuildCliOptions& Options,
                                                     const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);

            std::string PluginName{};
            std::filesystem::path DestinationPath{};
            std::string DisplayName{};
            std::string Company{};
            std::string Description{};
            std::string RuntimeModuleName{};
            std::string NamespaceRoot{};
            bool CreateEditorModule = false;
            std::string EditorModuleName{};

            for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
            {
                const std::string& Arg = Arguments[Index];
                if (Arg == "--name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    PluginName = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--dest")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    DestinationPath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--display-name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    DisplayName = *Value;
                    continue;
                }
                if (Arg == "--company")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    Company = *Value;
                    continue;
                }
                if (Arg == "--description")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    Description = *Value;
                    continue;
                }
                if (Arg == "--runtime-module")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    RuntimeModuleName = *Value;
                    continue;
                }
                if (Arg == "--namespace")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    NamespaceRoot = *Value;
                    continue;
                }
                if (Arg == "--editor-module")
                {
                    CreateEditorModule = true;
                    continue;
                }
                if (Arg == "--editor-module-name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    EditorModuleName = *Value;
                    continue;
                }

                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "Unknown option for create-plugin: " + Arg + "\n";
                return CliResult;
            }

            if (PluginName.empty() || DestinationPath.empty())
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "The --name and --dest options are required for create-plugin.\n";
                return CliResult;
            }

            auto Descriptor = PluginCreationService::BuildDefaultDescriptor(PluginName);
            if (!Descriptor)
            {
                CliResult.ExitCode = ExitCodeFromError(Descriptor.error());
                CliResult.StandardError = Descriptor.error().Message + "\n";
                return CliResult;
            }

            if (!TrimCopy(DisplayName).empty())
            {
                Descriptor->Plugin.DisplayName = TrimCopy(DisplayName);
            }
            if (!TrimCopy(Company).empty())
            {
                Descriptor->Plugin.Company = TrimCopy(Company);
            }
            if (!TrimCopy(Description).empty())
            {
                Descriptor->Plugin.Description = TrimCopy(Description);
            }

            PluginCreationRequest Request{};
            Request.PluginName = PluginName;
            Request.ParentDirectory = ResolveCreationParentDirectory(CurrentWorkingDirectory, DestinationPath, PluginName);
            Request.Descriptor = *Descriptor;
            Request.Code.RuntimeModuleName = RuntimeModuleName;
            Request.Code.NamespaceRoot = NamespaceRoot;
            Request.Code.CreateStarterEditorModule = CreateEditorModule;
            Request.Code.EditorModuleName = EditorModuleName;

            PluginCreationResult CreateResult{};
            const Result CreatePluginResult = PluginCreationService::CreatePlugin(Request, &CreateResult);
            if (!CreatePluginResult)
            {
                CliResult.ExitCode = ExitCodeFromError(CreatePluginResult.error());
                CliResult.StandardError = CreatePluginResult.error().Message + "\n";
                return CliResult;
            }

            CliResult.ArtifactPaths.push_back(CreateResult.Plugin.PluginFilePath);
            CliResult.ArtifactPaths.insert(CliResult.ArtifactPaths.end(), CreateResult.GeneratedFiles.begin(),
                                           CreateResult.GeneratedFiles.end());

            CliResult.ExitCode = EBuildCliExitCode::Success;
            if (GlobalOptions.JsonOutput)
            {
                Json Root = Json::object({
                    {"Command", "create-plugin"},
                    {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                    {"ExitCodeName", ToString(CliResult.ExitCode)},
                    {"PluginName", CreateResult.Plugin.Descriptor.Plugin.Name},
                    {"PluginFile", CreateResult.Plugin.PluginFilePath.lexically_normal().generic_string()},
                    {"PluginRoot", CreateResult.Plugin.PluginRootDirectory.lexically_normal().generic_string()},
                    {"Artifacts", Json::array()},
                });
                for (const auto& Path : CliResult.ArtifactPaths)
                {
                    Root["Artifacts"].push_back(Path.lexically_normal().generic_string());
                }
                CliResult.StandardOutput = Root.dump(2) + "\n";
            }
            else
            {
                std::ostringstream Stream{};
                Stream << "Created plugin `" << CreateResult.Plugin.Descriptor.Plugin.Name << "`.\n";
                Stream << "PluginFile: `" << CreateResult.Plugin.PluginFilePath.lexically_normal().string() << "`\n";
                Stream << "PluginRoot: `" << CreateResult.Plugin.PluginRootDirectory.lexically_normal().string()
                       << "`\n";
                CliResult.StandardOutput = Stream.str();
            }
            return CliResult;
        }

        /**
         * @brief Run one add-module command using `ModuleCreationService`.
         * @param Arguments Command arguments after the `add-module` token.
         * @param Options Base CLI options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunAddModule(const std::vector<std::string>& Arguments,
                                                  const BuildCliOptions& Options,
                                                  const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);

            std::filesystem::path ProjectFilePath{};
            std::filesystem::path PluginFilePath{};
            std::string ModuleName{};
            EProjectModuleType ModuleType = EProjectModuleType::Runtime;
            std::string ModuleRoot{};
            std::string NamespaceRoot{};
            std::vector<std::string> PublicDependencies{};
            std::vector<std::string> PrivateDependencies{};
            std::vector<std::string> Platforms{};
            std::vector<std::string> Definitions{};
            bool UseReflectionGen = false;
            bool UseSwig = false;

            for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
            {
                const std::string& Arg = Arguments[Index];
                if (Arg == "--project")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    ProjectFilePath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--plugin")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    PluginFilePath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--name")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    ModuleName = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--type")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    auto Parsed = ParseModuleType(*Value);
                    if (!Parsed)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Parsed.error().Message + "\n";
                        return CliResult;
                    }
                    ModuleType = *Parsed;
                    continue;
                }
                if (Arg == "--root")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    ModuleRoot = *Value;
                    continue;
                }
                if (Arg == "--namespace")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    NamespaceRoot = *Value;
                    continue;
                }
                if (Arg == "--public-dep")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    PublicDependencies.push_back(TrimCopy(*Value));
                    continue;
                }
                if (Arg == "--private-dep")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    PrivateDependencies.push_back(TrimCopy(*Value));
                    continue;
                }
                if (Arg == "--platform")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    Platforms.push_back(TrimCopy(*Value));
                    continue;
                }
                if (Arg == "--define")
                {
                    auto Value = TakeRequiredValue(Arguments, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    Definitions.push_back(TrimCopy(*Value));
                    continue;
                }
                if (Arg == "--reflection")
                {
                    UseReflectionGen = true;
                    continue;
                }
                if (Arg == "--swig")
                {
                    UseSwig = true;
                    continue;
                }

                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "Unknown option for add-module: " + Arg + "\n";
                return CliResult;
            }

            if (ModuleName.empty() || (ProjectFilePath.empty() == PluginFilePath.empty()))
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError =
                    "The --name option and exactly one of --project or --plugin are required for add-module.\n";
                return CliResult;
            }

            if (!ProjectFilePath.empty())
            {
                ModuleCreationRequest Request{};
                Request.ProjectFilePath = ProjectFilePath;
                Request.ModuleName = ModuleName;
                Request.ModuleType = ModuleType;
                Request.ModuleRoot = ModuleRoot;
                Request.NamespaceRoot = NamespaceRoot;
                Request.PublicDependencies = PublicDependencies;
                Request.PrivateDependencies = PrivateDependencies;
                Request.Platforms = Platforms;
                Request.PreprocessorDefinitions = Definitions;
                Request.UseReflectionGen = UseReflectionGen;
                Request.UseSWIG = UseSwig;

                ModuleCreationResult CreateResult{};
                const Result CreateModuleResult = ModuleCreationService::CreateModule(Request, &CreateResult);
                if (!CreateModuleResult)
                {
                    CliResult.ExitCode = ExitCodeFromError(CreateModuleResult.error());
                    CliResult.StandardError = CreateModuleResult.error().Message + "\n";
                    return CliResult;
                }

                CliResult.ArtifactPaths.push_back(CreateResult.Project.ProjectFilePath);
                CliResult.ArtifactPaths.insert(CliResult.ArtifactPaths.end(), CreateResult.GeneratedFiles.begin(),
                                               CreateResult.GeneratedFiles.end());

                CliResult.ExitCode = EBuildCliExitCode::Success;
                if (GlobalOptions.JsonOutput)
                {
                    Json Root = Json::object({
                        {"Command", "add-module"},
                        {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                        {"ExitCodeName", ToString(CliResult.ExitCode)},
                        {"ProjectName", CreateResult.Project.Descriptor.Project.Name},
                        {"ModuleName", CreateResult.Module.Name},
                        {"ModuleRoot", CreateResult.ModuleDirectory.lexically_normal().generic_string()},
                        {"Artifacts", Json::array()},
                    });
                    for (const auto& Path : CliResult.ArtifactPaths)
                    {
                        Root["Artifacts"].push_back(Path.lexically_normal().generic_string());
                    }
                    CliResult.StandardOutput = Root.dump(2) + "\n";
                }
                else
                {
                    std::ostringstream Stream{};
                    Stream << "Added module `" << CreateResult.Module.Name << "` to project `"
                           << CreateResult.Project.Descriptor.Project.Name << "`.\n";
                    Stream << "ModuleRoot: `" << CreateResult.ModuleDirectory.lexically_normal().string() << "`\n";
                    CliResult.StandardOutput = Stream.str();
                }
                return CliResult;
            }

            PluginModuleCreationRequest Request{};
            Request.PluginFilePath = PluginFilePath;
            Request.ModuleName = ModuleName;
            Request.ModuleType = ModuleType;
            Request.ModuleRoot = ModuleRoot;
            Request.NamespaceRoot = NamespaceRoot;
            Request.PublicDependencies = PublicDependencies;
            Request.PrivateDependencies = PrivateDependencies;
            Request.Platforms = Platforms;
            Request.PreprocessorDefinitions = Definitions;
            Request.UseReflectionGen = UseReflectionGen;
            Request.UseSWIG = UseSwig;

            PluginModuleCreationResult CreateResult{};
            const Result CreateModuleResult = ModuleCreationService::CreatePluginModule(Request, &CreateResult);
            if (!CreateModuleResult)
            {
                CliResult.ExitCode = ExitCodeFromError(CreateModuleResult.error());
                CliResult.StandardError = CreateModuleResult.error().Message + "\n";
                return CliResult;
            }

            CliResult.ArtifactPaths.push_back(CreateResult.Plugin.PluginFilePath);
            CliResult.ArtifactPaths.insert(CliResult.ArtifactPaths.end(), CreateResult.GeneratedFiles.begin(),
                                           CreateResult.GeneratedFiles.end());

            CliResult.ExitCode = EBuildCliExitCode::Success;
            if (GlobalOptions.JsonOutput)
            {
                Json Root = Json::object({
                    {"Command", "add-module"},
                    {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                    {"ExitCodeName", ToString(CliResult.ExitCode)},
                    {"PluginName", CreateResult.Plugin.Descriptor.Plugin.Name},
                    {"ModuleName", CreateResult.Module.Name},
                    {"ModuleRoot", CreateResult.ModuleDirectory.lexically_normal().generic_string()},
                    {"Artifacts", Json::array()},
                });
                for (const auto& Path : CliResult.ArtifactPaths)
                {
                    Root["Artifacts"].push_back(Path.lexically_normal().generic_string());
                }
                CliResult.StandardOutput = Root.dump(2) + "\n";
            }
            else
            {
                std::ostringstream Stream{};
                Stream << "Added module `" << CreateResult.Module.Name << "` to plugin `"
                       << CreateResult.Plugin.Descriptor.Plugin.Name << "`.\n";
                Stream << "ModuleRoot: `" << CreateResult.ModuleDirectory.lexically_normal().string() << "`\n";
                CliResult.StandardOutput = Stream.str();
            }
            return CliResult;
        }

        /**
         * @brief Run one build-history command over project-local history artifacts.
         * @param Arguments Command arguments after the `history` token.
         * @param Options Base CLI options.
         * @param GlobalOptions Global CLI-output options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunHistory(const std::vector<std::string>& Arguments,
                                                const BuildCliOptions& Options,
                                                const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);
            if (Arguments.empty())
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "History requires one subcommand: list | show | compare.\n";
                return CliResult;
            }

            const std::string Subcommand = LowercaseCopy(Arguments.front());
            const std::vector<std::string> SubArgs(Arguments.begin() + 1, Arguments.end());

            std::filesystem::path ProjectFilePath{};
            std::string BuildId{};
            std::string LeftBuildId{};
            std::string RightBuildId{};
            std::size_t MaxEntries = 0u;
            bool IncludeIncomplete = true;

            for (std::size_t Index = 0; Index < SubArgs.size(); ++Index)
            {
                const std::string& Arg = SubArgs[Index];
                if (Arg == "--project")
                {
                    auto Value = TakeRequiredValue(SubArgs, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    ProjectFilePath = ResolvePathAgainst(CurrentWorkingDirectory, *Value);
                    continue;
                }
                if (Arg == "--build-id")
                {
                    auto Value = TakeRequiredValue(SubArgs, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    BuildId = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--left")
                {
                    auto Value = TakeRequiredValue(SubArgs, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    LeftBuildId = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--right")
                {
                    auto Value = TakeRequiredValue(SubArgs, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    RightBuildId = TrimCopy(*Value);
                    continue;
                }
                if (Arg == "--max")
                {
                    auto Value = TakeRequiredValue(SubArgs, Index, Arg);
                    if (!Value)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = Value.error().Message + "\n";
                        return CliResult;
                    }
                    try
                    {
                        MaxEntries = static_cast<std::size_t>(std::stoull(*Value));
                    }
                    catch (const std::exception&)
                    {
                        CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                        CliResult.StandardError = "The --max value must be one unsigned integer.\n";
                        return CliResult;
                    }
                    continue;
                }
                if (Arg == "--complete-only")
                {
                    IncludeIncomplete = false;
                    continue;
                }

                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "Unknown option for history: " + Arg + "\n";
                return CliResult;
            }

            if (ProjectFilePath.empty())
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = "The --project option is required for history commands.\n";
                return CliResult;
            }

            auto Project = LoadResolvedProject(ProjectFilePath);
            if (!Project)
            {
                CliResult.ExitCode = ExitCodeFromError(Project.error());
                CliResult.StandardError = Project.error().Message + "\n";
                return CliResult;
            }

            if (Subcommand == "list")
            {
                BuildHistoryListOptions ListOptions{};
                ListOptions.MaxEntries = MaxEntries;
                ListOptions.IncludeIncomplete = IncludeIncomplete;
                auto Entries = BuildHistoryService::List(Project->SavedRootDirectory, ListOptions);
                if (!Entries)
                {
                    CliResult.ExitCode = ExitCodeFromError(Entries.error());
                    CliResult.StandardError = Entries.error().Message + "\n";
                    return CliResult;
                }

                CliResult.ExitCode = EBuildCliExitCode::Success;
                if (GlobalOptions.JsonOutput)
                {
                    Json Root = Json::object({
                        {"Command", "history-list"},
                        {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                        {"ExitCodeName", ToString(CliResult.ExitCode)},
                        {"Entries", Json::array()},
                    });
                    for (const BuildHistoryEntry& Entry : *Entries)
                    {
                        Root["Entries"].push_back(Json::object({
                            {"BuildId", Entry.BuildId},
                            {"State", Entry.State == EBuildHistoryEntryState::Complete ? "Complete" : "Incomplete"},
                            {"Status", ToString(Entry.Status)},
                            {"StartedAtUtc", Entry.StartedAtUtc},
                            {"FinishedAtUtc", Entry.FinishedAtUtc},
                            {"NodeCount", Entry.NodeCount},
                            {"OutputFileCount", Entry.OutputFileCount},
                            {"HistoryDirectory", Entry.HistoryDirectory.lexically_normal().generic_string()},
                            {"BuildReportFile", Entry.BuildReportFilePath.lexically_normal().generic_string()},
                        }));
                    }
                    CliResult.StandardOutput = Root.dump(2) + "\n";
                }
                else
                {
                    std::ostringstream Stream{};
                    Stream << "Build history for `" << Project->ProjectFilePath.lexically_normal().string() << "`:\n";
                    for (const BuildHistoryEntry& Entry : *Entries)
                    {
                        Stream << "- " << Entry.BuildId << " ["
                               << (Entry.State == EBuildHistoryEntryState::Complete ? ToString(Entry.Status) : "Incomplete")
                               << "]\n";
                    }
                    CliResult.StandardOutput = Stream.str();
                }
                return CliResult;
            }

            if (Subcommand == "show")
            {
                if (TrimCopy(BuildId).empty())
                {
                    CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                    CliResult.StandardError = "The --build-id option is required for history show.\n";
                    return CliResult;
                }

                auto ReportPath = ResolveBuildReportPath(ProjectFilePath, BuildId);
                if (!ReportPath)
                {
                    CliResult.ExitCode = ExitCodeFromError(ReportPath.error());
                    CliResult.StandardError = ReportPath.error().Message + "\n";
                    return CliResult;
                }

                auto Report = BuildHistoryService::LoadReport(*ReportPath);
                if (!Report)
                {
                    CliResult.ExitCode = ExitCodeFromError(Report.error());
                    CliResult.StandardError = Report.error().Message + "\n";
                    return CliResult;
                }

                CliResult.ExecutionReport = *Report;
                CliResult.ArtifactPaths.push_back(*ReportPath);
                CliResult.ExitCode = Report->Status == EBuildExecutionStatus::Succeeded ? EBuildCliExitCode::Success
                                                                                        : EBuildCliExitCode::BuildFailed;
                if (GlobalOptions.JsonOutput)
                {
                    auto ReportText = BuildExecutionService::SerializeReport(*Report, 2);
                    Json Root = Json::object({
                        {"Command", "history-show"},
                        {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                        {"ExitCodeName", ToString(CliResult.ExitCode)},
                    });
                    if (ReportText)
                    {
                        Root["Report"] = Json::parse(*ReportText, nullptr, false);
                    }
                    CliResult.StandardOutput = Root.dump(2) + "\n";
                }
                else
                {
                    CliResult.StandardOutput = "Loaded build report `" + Report->BuildId + "`.\n";
                }
                return CliResult;
            }

            if (Subcommand == "compare")
            {
                if (TrimCopy(LeftBuildId).empty() || TrimCopy(RightBuildId).empty())
                {
                    CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                    CliResult.StandardError = "The --left and --right options are required for history compare.\n";
                    return CliResult;
                }

                auto LeftPath = ResolveBuildReportPath(ProjectFilePath, LeftBuildId);
                auto RightPath = ResolveBuildReportPath(ProjectFilePath, RightBuildId);
                if (!LeftPath || !RightPath)
                {
                    const Error ErrorValue = LeftPath ? RightPath.error() : LeftPath.error();
                    CliResult.ExitCode = ExitCodeFromError(ErrorValue);
                    CliResult.StandardError = ErrorValue.Message + "\n";
                    return CliResult;
                }

                auto Left = BuildHistoryService::LoadReport(*LeftPath);
                auto Right = BuildHistoryService::LoadReport(*RightPath);
                if (!Left || !Right)
                {
                    const Error ErrorValue = Left ? Right.error() : Left.error();
                    CliResult.ExitCode = ExitCodeFromError(ErrorValue);
                    CliResult.StandardError = ErrorValue.Message + "\n";
                    return CliResult;
                }

                const BuildHistoryComparison Comparison = BuildHistoryService::Compare(*Left, *Right);
                CliResult.ExitCode = EBuildCliExitCode::Success;
                if (GlobalOptions.JsonOutput)
                {
                    Json Root = Json::object({
                        {"Command", "history-compare"},
                        {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                        {"ExitCodeName", ToString(CliResult.ExitCode)},
                        {"LeftBuildId", Comparison.LeftBuildId},
                        {"RightBuildId", Comparison.RightBuildId},
                        {"SameRequestHash", Comparison.SameRequestHash},
                        {"SameStatus", Comparison.SameStatus},
                        {"AddedOutputFiles", Comparison.AddedOutputFiles},
                        {"RemovedOutputFiles", Comparison.RemovedOutputFiles},
                        {"NodeDeltas", Json::array()},
                    });
                    for (const BuildHistoryNodeDelta& Delta : Comparison.NodeDeltas)
                    {
                        Root["NodeDeltas"].push_back(Json::object({
                            {"NodeId", Delta.NodeId},
                            {"Name", Delta.Name},
                            {"LeftPresent", Delta.LeftPresent},
                            {"RightPresent", Delta.RightPresent},
                            {"LeftCacheHit", Delta.LeftCacheHit},
                            {"RightCacheHit", Delta.RightCacheHit},
                        }));
                    }
                    CliResult.StandardOutput = Root.dump(2) + "\n";
                }
                else
                {
                    std::ostringstream Stream{};
                    Stream << "Compared `" << Comparison.LeftBuildId << "` -> `" << Comparison.RightBuildId << "`.\n";
                    Stream << "Added outputs: " << Comparison.AddedOutputFiles.size() << "\n";
                    Stream << "Removed outputs: " << Comparison.RemovedOutputFiles.size() << "\n";
                    Stream << "Node deltas: " << Comparison.NodeDeltas.size() << "\n";
                    CliResult.StandardOutput = Stream.str();
                }
                return CliResult;
            }

            CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
            CliResult.StandardError = "Unknown history subcommand: " + Arguments.front() + "\n";
            return CliResult;
        }

        /**
         * @brief Retry one prior build request by reloading its frozen request from build history.
         * @param Arguments Command arguments after the `retry` token.
         * @param Options Base CLI options.
         * @param GlobalOptions Global CLI-output options.
         * @return Captured CLI result.
         */
        [[nodiscard]] BuildCliResult RunRetry(const std::vector<std::string>& Arguments,
                                              const BuildCliOptions& Options,
                                              const CliGlobalOptions& GlobalOptions)
        {
            BuildCliResult CliResult{};
            const std::filesystem::path CurrentWorkingDirectory = EffectiveWorkingDirectory(Options);

            RetryInvocationArguments Invocation{};
            if (const Result ParseResult = ParseRetryInvocationArguments(Arguments, CurrentWorkingDirectory, Invocation);
                !ParseResult)
            {
                CliResult.ExitCode = EBuildCliExitCode::InvalidArguments;
                CliResult.StandardError = ParseResult.error().Message + "\n";
                return CliResult;
            }

            auto BuildRequestPath = ResolveBuildRequestPath(Invocation.ProjectFilePath, Invocation.SourceBuildId);
            if (!BuildRequestPath)
            {
                CliResult.ExitCode = ExitCodeFromError(BuildRequestPath.error());
                CliResult.StandardError = BuildRequestPath.error().Message + "\n";
                return CliResult;
            }

            auto Resolved = BuildRequestService::LoadResolved(*BuildRequestPath);
            if (!Resolved)
            {
                CliResult.ExitCode = ExitCodeFromError(Resolved.error());
                CliResult.StandardError = Resolved.error().Message + "\n";
                return CliResult;
            }

            std::optional<BuildExecutionReport> SourceReport{};
            if (!Invocation.RebuildAll)
            {
                auto BuildReportPath = ResolveBuildReportPath(Invocation.ProjectFilePath, Invocation.SourceBuildId);
                if (!BuildReportPath)
                {
                    CliResult.ExitCode = ExitCodeFromError(BuildReportPath.error());
                    CliResult.StandardError = BuildReportPath.error().Message + "\n";
                    return CliResult;
                }

                auto LoadedReport = BuildHistoryService::LoadReport(*BuildReportPath);
                if (!LoadedReport)
                {
                    CliResult.ExitCode = ExitCodeFromError(LoadedReport.error());
                    CliResult.StandardError = LoadedReport.error().Message + "\n";
                    return CliResult;
                }

                SourceReport = std::move(*LoadedReport);
            }

            if (Resolved->Project.ProjectFilePath.lexically_normal() != Invocation.ProjectFilePath.lexically_normal())
            {
                CliResult.ExitCode = EBuildCliExitCode::ValidationFailed;
                CliResult.StandardError =
                    "The requested retry project does not match the stored BuildRequest project.\n";
                return CliResult;
            }

            BuildPlannerOptions PlannerOptions = Options.Planner;
            if (!TrimCopy(Invocation.BuildId).empty())
            {
                PlannerOptions.BuildId = Invocation.BuildId;
            }

            auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
            if (!Graph)
            {
                CliResult.ExitCode = ExitCodeFromError(Graph.error());
                CliResult.StandardError = Graph.error().Message + "\n";
                return CliResult;
            }

            CliResult.PlannedGraph = *Graph;
            if (Invocation.PlanOnly)
            {
                CliResult.ExitCode = EBuildCliExitCode::Success;
                if (GlobalOptions.JsonOutput)
                {
                    auto GraphText = BuildPlannerService::Serialize(*Graph, 2);
                    Json Root = Json::object({
                        {"Command", "retry"},
                        {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                        {"ExitCodeName", ToString(CliResult.ExitCode)},
                        {"SourceBuildId", Invocation.SourceBuildId},
                        {"BuildId", Graph->BuildId},
                        {"RebuildAll", Invocation.RebuildAll},
                    });
                    if (GraphText)
                    {
                        Root["Plan"] = Json::parse(*GraphText, nullptr, false);
                    }
                    CliResult.StandardOutput = Root.dump(2) + "\n";
                }
                else
                {
                    CliResult.StandardOutput = "Retry plan created from `" + Invocation.SourceBuildId + "`.\n";
                }
                return CliResult;
            }

            BuildExecutionOptions ExecutionOptions = Options.Execution;
            std::vector<BuildEvent> Events{};
            const auto ExistingSink = ExecutionOptions.EventSink;
            if (GlobalOptions.JsonOutput || GlobalOptions.IncludeEventPayloads)
            {
                ExecutionOptions.EventSink = [&](const BuildEvent& Event)
                {
                    Events.push_back(Event);
                    if (ExistingSink)
                    {
                        ExistingSink(Event);
                    }
                };
            }
            ExecutionOptions.CodeBuild.Enabled = !Invocation.SkipCode;
            ExecutionOptions.AssetCook.Enabled = !Invocation.SkipAssets;
            if (!Invocation.EngineSourceDirectory.empty())
            {
                ExecutionOptions.CodeBuild.EngineSourceDirectory = Invocation.EngineSourceDirectory;
            }
            if (!TrimCopy(Invocation.Generator).empty())
            {
                ExecutionOptions.CodeBuild.Generator = Invocation.Generator;
            }
            if (!Invocation.BuildTargets.empty())
            {
                ExecutionOptions.CodeBuild.BuildTargets = Invocation.BuildTargets;
            }
            if (!Invocation.OutputRootDirectory.empty())
            {
                ExecutionOptions.PackageOutput.OutputRootDirectory = Invocation.OutputRootDirectory;
            }
            if (Invocation.ArchiveRequested)
            {
                ExecutionOptions.PackageOutput.ArchiveEnabled = true;
            }
            if (SourceReport.has_value())
            {
                ExecutionOptions.ResumeBaselineReport = std::addressof(*SourceReport);
            }
            if (Invocation.RebuildAll)
            {
                ExecutionOptions.ResumeBaselineReport = nullptr;
                ExecutionOptions.ResumeSucceededNodes = false;
            }

            auto Report = BuildExecutionService::Execute(*Resolved, *Graph, ExecutionOptions);
            if (!Report)
            {
                CliResult.ExitCode = ExitCodeFromError(Report.error());
                CliResult.StandardError = Report.error().Message + "\n";
                return CliResult;
            }

            CliResult.ExecutionReport = *Report;
            CliResult.ExitCode = Report->Status == EBuildExecutionStatus::Succeeded ? EBuildCliExitCode::Success
                                                                                    : EBuildCliExitCode::BuildFailed;
            if (GlobalOptions.JsonOutput)
            {
                auto ReportText = BuildExecutionService::SerializeReport(*Report, 2);
                Json Root = Json::object({
                    {"Command", "retry"},
                    {"ExitCode", static_cast<int>(CliResult.ExitCode)},
                    {"ExitCodeName", ToString(CliResult.ExitCode)},
                    {"SourceBuildId", Invocation.SourceBuildId},
                    {"RebuildAll", Invocation.RebuildAll},
                });
                if (ReportText)
                {
                    Root["Report"] = Json::parse(*ReportText, nullptr, false);
                }
                AppendEventsIfRequested(Root, GlobalOptions, Events);
                CliResult.StandardOutput = Root.dump(2) + "\n";
            }
            else
            {
                CliResult.StandardOutput = "Retried build request from `" + Invocation.SourceBuildId + "`.\n";
            }
            return CliResult;
        }

    } // namespace

    std::string BuildCliService::Usage(const std::string_view ExecutableName)
    {
        std::ostringstream Stream{};
        Stream << "Usage: " << ExecutableName << " [build] <command> [options]\n"
               << "\n"
               << "Commands:\n"
               << "  create-project  Create one new project workspace.\n"
               << "  create-plugin   Create one new plugin workspace.\n"
               << "  add-module      Add one new module to an existing project or plugin.\n"
               << "  validate        Resolve and validate one build request.\n"
               << "  package         Plan and optionally execute one package build.\n"
               << "  retry           Re-run one prior frozen build request from history.\n"
               << "  history         List, show, or compare prior build-history entries.\n"
               << "\n"
               << "Common build options:\n"
               << "  --json                  Emit structured JSON output.\n"
               << "  --json-events           Include structured build events in JSON package/retry output.\n"
               << "  --project <path>          Project descriptor file.\n"
               << "  --profile <name>          Named build profile.\n"
               << "  --platform <name>         Ad hoc target platform override.\n"
               << "  --config <name>           Debug | Development | Test | Shipping.\n"
               << "  --container <value>       Execution environment such as docker://image:tag.\n"
               << "  --level <asset>           Selected level asset.\n"
               << "  --include-folder <path>   Include-folder rule.\n"
               << "  --exclude-folder <path>   Exclude-folder rule.\n"
               << "  --archive                 Enable final archive output.\n"
               << "  --archive-format <name>   Archive format, currently zip.\n"
               << "  --dest <path>             Output/package destination root.\n"
               << "  --build-id <value>        Explicit build id for deterministic runs.\n"
               << "  --from-build-id <value>   Source history build id for retry.\n"
               << "  --rebuild-all             Ignore prior successful nodes and re-execute the full retry build.\n"
               << "  --plan-only               Create the build plan without executing it.\n"
               << "  --skip-code               Use placeholder code-build nodes.\n"
               << "  --skip-assets             Use placeholder asset-cook nodes.\n";
        return Stream.str();
    }

    BuildCliResult BuildCliService::Run(const std::vector<std::string>& Arguments, const BuildCliOptions& Options)
    {
        try
        {
            std::vector<std::string> EffectiveArguments = Arguments;
            if (!EffectiveArguments.empty() && EffectiveArguments.front() == "build")
            {
                EffectiveArguments.erase(EffectiveArguments.begin());
            }

            if (EffectiveArguments.empty())
            {
                return BuildCliResult{
                    .ExitCode = EBuildCliExitCode::InvalidArguments,
                    .StandardError = Usage("snapi") + "\n",
                };
            }

            std::vector<std::string> GlobalFilteredArguments{};
            const CliGlobalOptions GlobalOptions = ParseGlobalOptions(EffectiveArguments, GlobalFilteredArguments);
            if (GlobalFilteredArguments.empty())
            {
                return BuildCliResult{
                    .ExitCode = EBuildCliExitCode::InvalidArguments,
                    .StandardError = Usage("snapi") + "\n",
                };
            }

            const std::string Command = LowercaseCopy(GlobalFilteredArguments.front());
            const std::vector<std::string> CommandArguments(GlobalFilteredArguments.begin() + 1,
                                                           GlobalFilteredArguments.end());

            if (Command == "--help" || Command == "-h" || Command == "help")
            {
                return BuildCliResult{
                    .ExitCode = EBuildCliExitCode::Success,
                    .StandardOutput = Usage("snapi"),
                };
            }
            if (Command == "create-project")
            {
                return RunCreateProject(CommandArguments, Options, GlobalOptions);
            }
            if (Command == "create-plugin")
            {
                return RunCreatePlugin(CommandArguments, Options, GlobalOptions);
            }
            if (Command == "add-module")
            {
                return RunAddModule(CommandArguments, Options, GlobalOptions);
            }
            if (Command == "validate")
            {
                return RunValidate(CommandArguments, Options, GlobalOptions);
            }
            if (Command == "package")
            {
                return RunPackage(CommandArguments, Options, GlobalOptions);
            }
            if (Command == "retry")
            {
                return RunRetry(CommandArguments, Options, GlobalOptions);
            }
            if (Command == "history")
            {
                return RunHistory(CommandArguments, Options, GlobalOptions);
            }

            return BuildCliResult{
                .ExitCode = EBuildCliExitCode::InvalidArguments,
                .StandardError = "Unknown command: " + EffectiveArguments.front() + "\n" + Usage("snapi"),
            };
        }
        catch (const std::exception& Ex)
        {
            return BuildCliResult{
                .ExitCode = EBuildCliExitCode::InternalError,
                .StandardError = std::string("Unexpected build CLI failure: ") + Ex.what() + "\n",
            };
        }
        catch (...)
        {
            return BuildCliResult{
                .ExitCode = EBuildCliExitCode::InternalError,
                .StandardError = "Unexpected build CLI failure.\n",
            };
        }
    }

} // namespace SnAPI::GameFramework
