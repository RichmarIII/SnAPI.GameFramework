#include "Editor/EditorBuildService.h"

#include "Editor/EditorAssetService.h"
#include "ProjectDescriptor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <typeindex>
#include <utility>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace SnAPI::GameFramework::Editor
{
namespace
{

constexpr std::size_t kMaxConsoleLogBytes = 512u * 1024u;
constexpr std::size_t kMaxConsoleDisplayBytes = 16u * 1024u;
constexpr std::size_t kMaxConsoleDisplayLines = 600u;

/**
 * @brief Trim ASCII whitespace from both ends of one string view.
 * @param Text Input text.
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
 * @brief Return `true` when one byte should survive into the plain-text console log.
 * @param Character Candidate byte value.
 * @return `true` when the byte is printable plain text or horizontal tab.
 */
[[nodiscard]] bool IsConsolePrintable(const unsigned char Character)
{
    return Character == '\t' || (Character >= 0x20u && Character != 0x7Fu);
}

/**
 * @brief Strip leading blank runs and collapse whitespace-only blank lines in one console buffer.
 * @param Text Console buffer to normalize in place.
 *
 * This keeps intentional indentation on non-empty lines, but removes leading empty lines and
 * collapses blank-line runs so the editor console does not expand into large empty regions.
 */
void NormalizeConsoleBuffer(std::string& Text)
{
    if (Text.empty())
    {
        return;
    }

    std::string Normalized{};
    Normalized.reserve(Text.size());

    bool SawVisibleContent = false;
    bool CurrentLineHasVisibleContent = false;
    std::size_t BlankLinesEmitted = 0u;

    const auto TrimCurrentLineTrailingWhitespace = [&Normalized]()
    {
        while (!Normalized.empty() && Normalized.back() != '\n' &&
               (Normalized.back() == ' ' || Normalized.back() == '\t'))
        {
            Normalized.pop_back();
        }
    };

    for (const unsigned char Character : Text)
    {
        if (Character == '\r')
        {
            continue;
        }

        if (Character == '\n')
        {
            TrimCurrentLineTrailingWhitespace();
            if (!CurrentLineHasVisibleContent)
            {
                if (!SawVisibleContent || BlankLinesEmitted >= 1u)
                {
                    CurrentLineHasVisibleContent = false;
                    continue;
                }

                Normalized.push_back('\n');
                BlankLinesEmitted = 1u;
                CurrentLineHasVisibleContent = false;
                continue;
            }

            Normalized.push_back('\n');
            SawVisibleContent = true;
            BlankLinesEmitted = 0u;
            CurrentLineHasVisibleContent = false;
            continue;
        }

        Normalized.push_back(static_cast<char>(Character));
        if (std::isspace(Character) == 0)
        {
            CurrentLineHasVisibleContent = true;
            SawVisibleContent = true;
        }
    }

    Text.swap(Normalized);
}

/**
 * @brief Build one UI-safe recent tail window from the retained raw console transcript.
 * @param Text Retained raw console transcript.
 * @return Recent normalized console slice suitable for `UIText` rendering.
 */
[[nodiscard]] std::string BuildConsoleDisplayText(const std::string_view Text)
{
    if (Text.empty())
    {
        return {};
    }

    std::size_t Start = 0u;
    bool Truncated = false;

    if (Text.size() > kMaxConsoleDisplayBytes)
    {
        Start = Text.size() - kMaxConsoleDisplayBytes;
        while (Start < Text.size() && (static_cast<unsigned char>(Text[Start]) & 0xC0u) == 0x80u)
        {
            ++Start;
        }

        if (const std::size_t Newline = Text.find('\n', Start); Newline != std::string_view::npos && Newline + 1u < Text.size())
        {
            Start = Newline + 1u;
        }
        Truncated = true;
    }

    std::size_t LinesSeen = 0u;
    for (std::size_t Index = Text.size(); Index > Start; --Index)
    {
        if (Text[Index - 1u] != '\n')
        {
            continue;
        }

        ++LinesSeen;
        if (LinesSeen > kMaxConsoleDisplayLines)
        {
            Start = Index;
            Truncated = true;
            break;
        }
    }

    std::string Visible(Text.substr(Start));
    NormalizeConsoleBuffer(Visible);
    if (!Truncated)
    {
        return Visible;
    }

    static constexpr std::string_view kPrefix = "[Console truncated to recent output]\n";
    if (Visible.empty())
    {
        return std::string(kPrefix);
    }
    return std::string(kPrefix) + Visible;
}

/**
 * @brief Append one captured stdout/stderr chunk into the rolling plain-text console log.
 * @param Destination Rolling console log text.
 * @param Text Newly captured terminal text.
 * @remarks
 * This sanitizer strips ANSI escape sequences, treats carriage-return progress
 * updates as in-place line rewrites, and drops control bytes that would
 * otherwise explode into empty lines or garbage glyphs in the editor text view.
 */
void AppendSanitizedConsoleChunk(std::string& Destination, const std::string_view Text)
{
    enum class EEscapeState : std::uint8_t
    {
        None = 0,
        Escape,
        Csi,
    };

    EEscapeState EscapeState = EEscapeState::None;
    std::size_t ConsecutiveNewlines = 0u;
    if (!Destination.empty())
    {
        for (std::size_t Index = Destination.size(); Index > 0u; --Index)
        {
            if (Destination[Index - 1u] == '\n')
            {
                ++ConsecutiveNewlines;
                continue;
            }
            break;
        }
    }

    for (const unsigned char Character : Text)
    {
        if (EscapeState == EEscapeState::Escape)
        {
            if (Character == '[')
            {
                EscapeState = EEscapeState::Csi;
                continue;
            }
            EscapeState = EEscapeState::None;
        }
        else if (EscapeState == EEscapeState::Csi)
        {
            if (Character >= 0x40u && Character <= 0x7Eu)
            {
                EscapeState = EEscapeState::None;
            }
            continue;
        }

        if (Character == 0x1Bu)
        {
            EscapeState = EEscapeState::Escape;
            continue;
        }

        if (Character == '\r')
        {
            while (!Destination.empty() && Destination.back() != '\n')
            {
                Destination.pop_back();
            }
            ConsecutiveNewlines = 0u;
            continue;
        }

        if (Character == '\n')
        {
            if (ConsecutiveNewlines >= 1u)
            {
                continue;
            }
            Destination.push_back('\n');
            ++ConsecutiveNewlines;
            continue;
        }

        if (!IsConsolePrintable(Character))
        {
            continue;
        }

        Destination.push_back(static_cast<char>(Character));
        ConsecutiveNewlines = 0u;
    }
}

/**
 * @brief Return the active editor project descriptor path when one is loaded.
 * @param Context Borrowed editor-service context.
 * @return Loaded active project file path or an empty path.
 */
[[nodiscard]] std::filesystem::path CurrentProjectFilePath(EditorServiceContext& Context)
{
    if (auto* AssetService = Context.GetService<EditorAssetService>())
    {
        const auto& CurrentProject = AssetService->CurrentProject();
        if (CurrentProject.IsLoaded && !TrimCopy(CurrentProject.ProjectFilePath).empty())
        {
            return std::filesystem::path(CurrentProject.ProjectFilePath).lexically_normal();
        }
    }

    return {};
}

/**
 * @brief Return the canonical host-platform identifier used by the build system.
 * @return Host platform string used for default build-request generation.
 */
[[nodiscard]] std::string HostPlatformName()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "MacOS";
#else
    return "Linux";
#endif
}

/**
 * @brief Build the default host-local development profile name for the current platform.
 * @return Conventional profile name such as `WindowsDevelopment` or `LinuxDevelopment`.
 */
[[nodiscard]] std::string HostDevelopmentProfileName()
{
    return HostPlatformName() + "Development";
}

/**
 * @brief Convert one canonical build configuration into a compact user-facing label.
 * @param Configuration Build configuration value.
 * @return Stable display label.
 */
[[nodiscard]] std::string_view BuildConfigurationLabel(const EBuildConfiguration Configuration)
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
    default:
        return "Development";
    }
}

/**
 * @brief Build one concise resolved-profile summary string for editor UI.
 * @param Profile Fully resolved profile settings.
 * @return Compact multi-field summary.
 */
[[nodiscard]] std::string MakeProfileSummary(const ResolvedBuildProfile& Profile)
{
    std::string Summary = Profile.Platform + " / " + std::string(BuildConfigurationLabel(Profile.Configuration));
    if (!TrimCopy(Profile.ExecutionEnvironment).empty())
    {
        Summary += " / " + Profile.ExecutionEnvironment;
    }
    if (!Profile.SelectedLevels.empty())
    {
        Summary += " / Levels: " + std::to_string(Profile.SelectedLevels.size());
    }
    if (!Profile.ExplicitAssets.empty())
    {
        Summary += " / Assets: " + std::to_string(Profile.ExplicitAssets.size());
    }
    if (Profile.ArchiveEnabled)
    {
        Summary += " / Archive";
        if (!TrimCopy(Profile.ArchiveFormat).empty())
        {
            Summary += " (" + Profile.ArchiveFormat + ")";
        }
    }
    return Summary;
}

/**
 * @brief Return `true` when two resolved project-file paths name the same descriptor.
 * @param Left Left-side path.
 * @param Right Right-side path.
 * @return `true` when the normalized paths are equal.
 */
[[nodiscard]] bool SameProjectFile(const std::filesystem::path& Left, const std::filesystem::path& Right)
{
    return Left.lexically_normal() == Right.lexically_normal();
}

/**
 * @brief Resolve the active project descriptor and validate any explicit request path against it.
 * @param Context Borrowed editor-service context.
 * @param Request Optional caller-supplied request.
 * @return Active resolved descriptor or a structured error.
 */
[[nodiscard]] TExpected<ResolvedProjectDescriptor> ResolveActiveProjectDescriptor(EditorServiceContext& Context,
                                                                                  const BuildRequest* Request = nullptr)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    if (!AssetService)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Editor asset service is not available"));
    }

    const auto& CurrentProject = AssetService->CurrentProject();
    if (!CurrentProject.IsLoaded || TrimCopy(CurrentProject.ProjectFilePath).empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No active editor project is loaded"));
    }

    auto ActiveProject = ProjectDescriptorService::LoadResolved(CurrentProject.ProjectFilePath);
    if (!ActiveProject)
    {
        return std::unexpected(ActiveProject.error());
    }

    if (Request != nullptr && !Request->ProjectFilePath.empty())
    {
        auto RequestedProject = ProjectDescriptorService::LoadResolved(Request->ProjectFilePath.string());
        if (!RequestedProject)
        {
            return std::unexpected(RequestedProject.error());
        }

        if (!SameProjectFile(ActiveProject->ProjectFilePath, RequestedProject->ProjectFilePath))
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument,
                          "Editor build operations only support the currently loaded project descriptor"));
        }
    }

    return ActiveProject;
}

/**
 * @brief Choose the best default profile for the host platform, preferring `<Host>Development`.
 * @param Project Active resolved project descriptor.
 * @param MaxInheritanceDepth Maximum profile inheritance depth allowed during inspection.
 * @return Matching profile name or an empty string when no host-default profile exists.
 */
[[nodiscard]] std::string ChooseDefaultProfileName(const ResolvedProjectDescriptor& Project,
                                                   const std::size_t MaxInheritanceDepth)
{
    const std::string ExactName = HostDevelopmentProfileName();
    const auto ExactIt = std::find_if(
        Project.Descriptor.Profiles.begin(),
        Project.Descriptor.Profiles.end(),
        [&ExactName](const BuildProfile& Profile) { return Profile.Name == ExactName; });
    if (ExactIt != Project.Descriptor.Profiles.end())
    {
        return ExactName;
    }

    const std::string HostPlatform = HostPlatformName();
    for (const BuildProfile& Profile : Project.Descriptor.Profiles)
    {
        if (TrimCopy(Profile.Name).empty())
        {
            continue;
        }

        auto Resolved = BuildProfileService::ResolveProfile(Project.Descriptor.Profiles, Profile.Name, MaxInheritanceDepth);
        if (!Resolved)
        {
            continue;
        }

        if (Resolved->Platform == HostPlatform && Resolved->Configuration == EBuildConfiguration::Development)
        {
            return Profile.Name;
        }
    }

    return {};
}

/**
 * @brief Fill in the active project path and sane default host-local request values.
 * @param Project Active resolved descriptor.
 * @param Request Caller-supplied request.
 * @param MaxInheritanceDepth Maximum profile inheritance depth used when choosing a default profile.
 * @return Effective request ready for `BuildRequestService::Resolve`.
 */
[[nodiscard]] BuildRequest CompleteRequest(const ResolvedProjectDescriptor& Project,
                                           BuildRequest Request,
                                           const std::size_t MaxInheritanceDepth)
{
    Request.ProjectFilePath = Project.ProjectFilePath;

    if (TrimCopy(Request.ProfileName).empty())
    {
        Request.ProfileName = ChooseDefaultProfileName(Project, MaxInheritanceDepth);
    }

    if (TrimCopy(Request.ProfileName).empty())
    {
        if (!Request.Overrides.Platform.IsSet)
        {
            Request.Overrides.Platform = BuildProfileValue<std::string>{
                .IsSet = true,
                .Value = HostPlatformName(),
            };
        }

        if (!Request.Overrides.ExecutionEnvironment.IsSet)
        {
            Request.Overrides.ExecutionEnvironment = BuildProfileValue<std::string>{
                .IsSet = true,
                .Value = std::string("host-local"),
            };
        }

        if (!Request.Overrides.Configuration.IsSet)
        {
            Request.Overrides.Configuration = BuildProfileValue<EBuildConfiguration>{
                .IsSet = true,
                .Value = EBuildConfiguration::Development,
            };
        }
    }

    return Request;
}

/**
 * @brief Build the canonical history-directory path for one active-project build id.
 * @param Project Active resolved project descriptor.
 * @param BuildId Build invocation id.
 * @return Project-local history directory path.
 */
[[nodiscard]] std::filesystem::path BuildHistoryDirectoryFor(const ResolvedProjectDescriptor& Project,
                                                             std::string_view BuildId)
{
    return Project.SavedRootDirectory / "BuildHistory" / std::string(BuildId);
}

/**
 * @brief Build the canonical `BuildReport.json` path for one active-project build id.
 * @param Project Active resolved project descriptor.
 * @param BuildId Build invocation id.
 * @return Project-local report path.
 */
[[nodiscard]] std::filesystem::path BuildReportPathFor(const ResolvedProjectDescriptor& Project, std::string_view BuildId)
{
    return BuildHistoryDirectoryFor(Project, BuildId) / "BuildReport.json";
}

/**
 * @brief Build the canonical `BuildRequest.json` path for one active-project build id.
 * @param Project Active resolved project descriptor.
 * @param BuildId Build invocation id.
 * @return Project-local frozen-request path.
 */
[[nodiscard]] std::filesystem::path BuildRequestPathFor(const ResolvedProjectDescriptor& Project,
                                                        std::string_view BuildId)
{
    return BuildHistoryDirectoryFor(Project, BuildId) / "BuildRequest.json";
}

/**
 * @brief Convert one build validation severity into a compact label.
 * @param Severity Severity to stringify.
 * @return Stable severity label.
 */
[[nodiscard]] std::string_view BuildSeverityLabel(const EBuildValidationSeverity Severity)
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
 * @brief Convert one build stage into a compact label.
 * @param Stage Stage to stringify.
 * @return Stable stage label.
 */
[[nodiscard]] std::string_view BuildStageLabel(const EBuildStage Stage)
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
 * @brief Convert one build event kind into a compact label.
 * @param Kind Event kind to stringify.
 * @return Stable event-kind label.
 */
[[nodiscard]] std::string_view BuildEventKindLabel(const EBuildEventKind Kind)
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
 * @brief Format one structured build event into the editor console log.
 * @param Event Event to format.
 * @return Single-line UTF-8 console log entry.
 */
[[nodiscard]] std::string FormatBuildEventConsoleLine(const BuildEvent& Event)
{
    std::ostringstream Stream{};
    Stream << "[" << Event.TimestampUtc << "]"
           << "[" << BuildSeverityLabel(Event.Severity) << "]"
           << "[" << BuildStageLabel(Event.Stage) << "]"
           << "[" << BuildEventKindLabel(Event.Kind) << "]";
    if (Event.NodeId != 0u)
    {
        Stream << "[Node " << Event.NodeId << "]";
    }
    Stream << ' ' << Event.Message;
    if (!Event.Payload.empty())
    {
        Stream << " | " << Event.Payload.dump();
    }
    Stream << "\n";
    return Stream.str();
}

/**
 * @brief Build one section banner used to visually separate console sessions.
 * @param Title Section title to display.
 * @return UTF-8 console banner.
 */
[[nodiscard]] std::string MakeConsoleBanner(const std::string_view Title)
{
    return "=== " + std::string(Title) + " ===\n";
}

/**
 * @brief Build one stable status label for a terminal build-execution status.
 * @param Status Terminal execution status.
 * @return Borrowed label suitable for status text and summaries.
 */
[[nodiscard]] std::string_view BuildExecutionStatusLabel(const EBuildExecutionStatus Status)
{
    switch (Status)
    {
    case EBuildExecutionStatus::Succeeded:
        return "Succeeded";
    case EBuildExecutionStatus::Failed:
        return "Failed";
    case EBuildExecutionStatus::Cancelled:
        return "Cancelled";
    default:
        return "Unknown";
    }
}

/**
 * @brief Build one stable status label for a terminal build-node status.
 * @param Status Terminal node status.
 * @return Borrowed label suitable for summaries.
 */
[[nodiscard]] std::string_view BuildNodeStatusLabel(const EBuildNodeExecutionStatus Status)
{
    switch (Status)
    {
    case EBuildNodeExecutionStatus::Succeeded:
        return "Succeeded";
    case EBuildNodeExecutionStatus::Failed:
        return "Failed";
    case EBuildNodeExecutionStatus::Cancelled:
        return "Cancelled";
    default:
        return "Unknown";
    }
}

/**
 * @brief Build one concise failure or cancellation reason from one execution report.
 * @param Report Report to inspect.
 * @return Human-readable terminal reason, or an empty string when the report succeeded.
 */
[[nodiscard]] std::string DescribeBuildReportTerminalReason(const BuildExecutionReport& Report)
{
    if (Report.Status == EBuildExecutionStatus::Succeeded)
    {
        return {};
    }

    if (Report.Status == EBuildExecutionStatus::Failed)
    {
        const auto FailedNodeIt = std::find_if(
            Report.NodeRecords.begin(),
            Report.NodeRecords.end(),
            [](const BuildNodeExecutionRecord& Record) { return Record.Status == EBuildNodeExecutionStatus::Failed; });
        if (FailedNodeIt != Report.NodeRecords.end())
        {
            const std::string Message = TrimCopy(FailedNodeIt->Message);
            if (!Message.empty())
            {
                return "Node '" + FailedNodeIt->Name + "' failed: " + Message;
            }

            return "Node '" + FailedNodeIt->Name + "' ended with status " +
                   std::string(BuildNodeStatusLabel(FailedNodeIt->Status)) + ".";
        }

        const auto ValidationIssueIt = std::find_if(
            Report.ValidationIssues.begin(),
            Report.ValidationIssues.end(),
            [](const BuildValidationIssue& Issue) { return Issue.Severity == EBuildValidationSeverity::Error; });
        if (ValidationIssueIt != Report.ValidationIssues.end())
        {
            return ValidationIssueIt->RuleId + ": " + ValidationIssueIt->Message;
        }

        return "Build failed without a recorded node or validation error.";
    }

    const auto CancelledNodeIt =
        std::find_if(Report.NodeRecords.begin(),
                     Report.NodeRecords.end(),
                     [](const BuildNodeExecutionRecord& Record)
                     { return Record.Status == EBuildNodeExecutionStatus::Cancelled; });
    if (CancelledNodeIt != Report.NodeRecords.end())
    {
        const std::string Message = TrimCopy(CancelledNodeIt->Message);
        if (!Message.empty())
        {
            return Message;
        }
    }

    return "Build cancelled before completion.";
}

/**
 * @brief Append one terminal status summary into the rolling editor console log.
 * @param AppendSink Console sink used by the active editor build session.
 * @param StatusMessage Fully formatted status line to append.
 */
void AppendTerminalStatusConsoleLine(const std::function<void(std::string_view)>& AppendSink,
                                     const std::string_view StatusMessage)
{
    if (!AppendSink || StatusMessage.empty())
    {
        return;
    }

    AppendSink("[SnAPI][EditorBuild] " + std::string(StatusMessage) + "\n");
}

/**
 * @brief Build one lowercase copy used by lightweight diagnostic-token matching.
 * @param Text Source text.
 * @return Lowercase copy.
 */
[[nodiscard]] std::string ToLowerCopy(const std::string_view Text)
{
    std::string Lower(Text);
    std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Lower;
}

/**
 * @brief Return `true` when one line looks like a warning or error diagnostic.
 * @param Line Candidate log line.
 * @return `true` when the line should be surfaced in the editor diagnostics summary.
 */
[[nodiscard]] bool ContainsDiagnosticMarker(const std::string_view Line)
{
    if (Line.empty())
    {
        return false;
    }

    const std::string Lower = ToLowerCopy(Line);
    return Lower.find("error") != std::string::npos || Lower.find("warning") != std::string::npos ||
           Lower.find("failed") != std::string::npos || Lower.find("fatal") != std::string::npos;
}

/**
 * @brief Append one compact warning/error summary extracted from one build report.
 * @param Report Completed build report to inspect.
 * @param AppendSink Console sink used by the active editor build session.
 */
void AppendReportDiagnosticsToConsole(const BuildExecutionReport& Report,
                                      const std::function<void(std::string_view)>& AppendSink)
{
    if (!AppendSink)
    {
        return;
    }

    std::vector<std::string> Diagnostics{};
    std::set<std::string> Seen{};
    const auto TryAppend = [&Diagnostics, &Seen](const std::string& Line) {
        const std::string Trimmed = TrimCopy(Line);
        if (Trimmed.empty() || !Seen.insert(Trimmed).second)
        {
            return;
        }
        Diagnostics.push_back(Trimmed);
    };

    for (const BuildValidationIssue& Issue : Report.ValidationIssues)
    {
        if (Issue.Severity == EBuildValidationSeverity::Warning || Issue.Severity == EBuildValidationSeverity::Error)
        {
            TryAppend("[" + std::string(BuildSeverityLabel(Issue.Severity)) + "] " + Issue.RuleId + ": " + Issue.Message);
        }
    }

    for (const BuildNodeExecutionRecord& Record : Report.NodeRecords)
    {
        if ((Record.Status == EBuildNodeExecutionStatus::Failed || Record.Status == EBuildNodeExecutionStatus::Cancelled) &&
            !TrimCopy(Record.Message).empty())
        {
            TryAppend("[" + std::string(BuildNodeStatusLabel(Record.Status)) + "] Node '" + Record.Name + "': " +
                      Record.Message);
        }
    }

    for (const std::filesystem::path& LogFilePath : Report.StageLogFilePaths)
    {
        std::ifstream Input(LogFilePath);
        if (!Input.is_open())
        {
            continue;
        }

        std::string Line{};
        while (std::getline(Input, Line))
        {
            if (ContainsDiagnosticMarker(Line))
            {
                TryAppend("[" + LogFilePath.filename().string() + "] " + Line);
            }
        }
    }

    if (Diagnostics.empty())
    {
        return;
    }

    AppendSink(MakeConsoleBanner("Diagnostics Summary"));
    for (const std::string& Diagnostic : Diagnostics)
    {
        AppendSink(Diagnostic + "\n");
    }
}

/**
 * @brief Best-effort scoped capture that forwards `stdout` and `stderr` into one sink.
 *
 * The editor build flow is still synchronous, so this capture is used to preserve
 * the full configure/build session log for later inspection in the packaging modal.
 * It redirects the host process `stdout` and `stderr` descriptors into one pipe and
 * drains that pipe on a background thread until destruction restores the original
 * stream descriptors.
 */
class ScopedStdStreamCapture final
{
public:
    explicit ScopedStdStreamCapture(std::function<void(std::string_view)> Sink)
        : m_sink(std::move(Sink))
    {
        Start();
    }

    ScopedStdStreamCapture(const ScopedStdStreamCapture&) = delete;
    ScopedStdStreamCapture& operator=(const ScopedStdStreamCapture&) = delete;

    ~ScopedStdStreamCapture()
    {
        Stop();
    }

private:
#if defined(_WIN32)
    static constexpr int kInvalidFd = -1;

    [[nodiscard]] static int StreamFd(FILE* Stream)
    {
        return _fileno(Stream);
    }

    [[nodiscard]] static int DuplicateFd(const int Fd)
    {
        return _dup(Fd);
    }

    [[nodiscard]] static int DuplicateTo(const int SourceFd, const int DestinationFd)
    {
        return _dup2(SourceFd, DestinationFd);
    }

    [[nodiscard]] static int CreatePipe(int Handles[2])
    {
        return _pipe(Handles, 4096, O_BINARY);
    }

    [[nodiscard]] static int ReadFd(const int Fd, char* Buffer, const unsigned int Size)
    {
        return _read(Fd, Buffer, Size);
    }

    static void CloseFd(const int Fd)
    {
        if (Fd != kInvalidFd)
        {
            _close(Fd);
        }
    }
#else
    static constexpr int kInvalidFd = -1;

    [[nodiscard]] static int StreamFd(FILE* Stream)
    {
        return fileno(Stream);
    }

    [[nodiscard]] static int DuplicateFd(const int Fd)
    {
        return dup(Fd);
    }

    [[nodiscard]] static int DuplicateTo(const int SourceFd, const int DestinationFd)
    {
        return dup2(SourceFd, DestinationFd);
    }

    [[nodiscard]] static int CreatePipe(int Handles[2])
    {
        return pipe(Handles);
    }

    [[nodiscard]] static int ReadFd(const int Fd, char* Buffer, const unsigned int Size)
    {
        return static_cast<int>(read(Fd, Buffer, Size));
    }

    static void CloseFd(const int Fd)
    {
        if (Fd != kInvalidFd)
        {
            close(Fd);
        }
    }
#endif

    /**
     * @brief Start the descriptor redirection and reader thread.
     */
    void Start()
    {
        if (!m_sink)
        {
            return;
        }

        const int StdoutFd = StreamFd(stdout);
        const int StderrFd = StreamFd(stderr);
        if (StdoutFd == kInvalidFd || StderrFd == kInvalidFd)
        {
            return;
        }

        FlushStreams();
        m_savedStdoutFd = DuplicateFd(StdoutFd);
        m_savedStderrFd = DuplicateFd(StderrFd);
        if (m_savedStdoutFd == kInvalidFd || m_savedStderrFd == kInvalidFd)
        {
            CleanupDescriptors();
            return;
        }

        int PipeHandles[2]{kInvalidFd, kInvalidFd};
        if (CreatePipe(PipeHandles) != 0)
        {
            CleanupDescriptors();
            return;
        }

        m_pipeReadFd = PipeHandles[0];
        m_pipeWriteFd = PipeHandles[1];
        if (DuplicateTo(m_pipeWriteFd, StdoutFd) != 0 || DuplicateTo(m_pipeWriteFd, StderrFd) != 0)
        {
            (void)DuplicateTo(m_savedStdoutFd, StdoutFd);
            (void)DuplicateTo(m_savedStderrFd, StderrFd);
            CleanupDescriptors();
            return;
        }

        CloseFd(m_pipeWriteFd);
        m_pipeWriteFd = kInvalidFd;
        m_readerThread = std::thread([this]() { ReadLoop(); });
    }

    /**
     * @brief Restore the original descriptors and join the reader thread.
     */
    void Stop()
    {
        if (!m_sink)
        {
            return;
        }

        FlushStreams();
        const int StdoutFd = StreamFd(stdout);
        const int StderrFd = StreamFd(stderr);
        if (m_savedStdoutFd != kInvalidFd && StdoutFd != kInvalidFd)
        {
            (void)DuplicateTo(m_savedStdoutFd, StdoutFd);
        }
        if (m_savedStderrFd != kInvalidFd && StderrFd != kInvalidFd)
        {
            (void)DuplicateTo(m_savedStderrFd, StderrFd);
        }

        CleanupDescriptors();
        if (m_readerThread.joinable())
        {
            m_readerThread.join();
        }
        CloseFd(m_pipeReadFd);
        m_pipeReadFd = kInvalidFd;
        m_sink = {};
    }

    /**
     * @brief Drain redirected bytes from the capture pipe.
     */
    void ReadLoop()
    {
        std::array<char, 4096> Buffer{};
        while (m_pipeReadFd != kInvalidFd)
        {
            const int BytesRead = ReadFd(m_pipeReadFd, Buffer.data(), static_cast<unsigned int>(Buffer.size()));
            if (BytesRead <= 0)
            {
                break;
            }

            m_sink(std::string_view(Buffer.data(), static_cast<std::size_t>(BytesRead)));
        }
    }

    /**
     * @brief Flush C and C++ standard streams before redirecting or restoring them.
     */
    static void FlushStreams()
    {
        std::cout.flush();
        std::cerr.flush();
        std::fflush(stdout);
        std::fflush(stderr);
    }

    /**
     * @brief Close any saved or temporary write descriptors.
     */
    void CleanupDescriptors()
    {
        CloseFd(m_savedStdoutFd);
        CloseFd(m_savedStderrFd);
        CloseFd(m_pipeWriteFd);
        m_savedStdoutFd = kInvalidFd;
        m_savedStderrFd = kInvalidFd;
        m_pipeWriteFd = kInvalidFd;
    }

    std::function<void(std::string_view)> m_sink{};
    int m_savedStdoutFd = kInvalidFd;
    int m_savedStderrFd = kInvalidFd;
    int m_pipeReadFd = kInvalidFd;
    int m_pipeWriteFd = kInvalidFd;
    std::thread m_readerThread{};
};

/**
 * @brief Resolve one frozen request plus planned graph without touching editor-only state.
 * @param EffectiveRequest Fully completed build request with project path and default profile selection applied.
 * @param PlannerOptions Optional planner overrides.
 * @param MaxInheritanceDepth Maximum build-profile inheritance depth allowed during request resolution.
 * @return Planned editor build payload or a structured error.
 */
[[nodiscard]] TExpected<EditorBuildPlan> CreateEditorBuildPlan(const BuildRequest& EffectiveRequest,
                                                               const BuildPlannerOptions& PlannerOptions,
                                                               const std::size_t MaxInheritanceDepth)
{
    auto Resolved = BuildRequestService::Resolve(EffectiveRequest, MaxInheritanceDepth);
    if (!Resolved)
    {
        return std::unexpected(Resolved.error());
    }

    auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    if (!Graph)
    {
        return std::unexpected(Graph.error());
    }

    return EditorBuildPlan{
        .Request = *Resolved,
        .Graph = *Graph,
    };
}

/**
 * @brief Apply console/event sinks to one execution-options payload for editor logging.
 * @param ExecutionOptions Caller-supplied execution options.
 * @param AppendSink Thread-safe editor console sink.
 * @return Execution options with build-event and code-build output forwarding attached.
 */
[[nodiscard]] BuildExecutionOptions AttachEditorConsoleSinks(
    BuildExecutionOptions ExecutionOptions, const std::function<void(std::string_view)>& AppendSink)
{
    const auto ExistingEventSink = ExecutionOptions.EventSink;
    ExecutionOptions.EventSink = [AppendSink, ExistingEventSink](const BuildEvent& Event) {
        if (AppendSink)
        {
            AppendSink(FormatBuildEventConsoleLine(Event));
        }
        if (ExistingEventSink)
        {
            ExistingEventSink(Event);
        }
    };

    const auto ExistingOutputSink = ExecutionOptions.CodeBuild.OutputSink;
    ExecutionOptions.CodeBuild.OutputSink = [AppendSink, ExistingOutputSink](const std::string_view Text) {
        if (AppendSink && !Text.empty())
        {
            AppendSink(Text);
        }
        if (ExistingOutputSink)
        {
            ExistingOutputSink(Text);
        }
    };

    return ExecutionOptions;
}

} // namespace

std::string_view EditorBuildService::Name() const
{
    return "EditorBuildService";
}

std::vector<std::type_index> EditorBuildService::Dependencies() const
{
    return {std::type_index(typeid(EditorAssetService))};
}

Result EditorBuildService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_statusMessage.clear();
    m_lastReportedStatusMessage.clear();
    m_lastPlan.reset();
    m_lastReport.reset();
    ClearConsoleLog();
    return Ok();
}

void EditorBuildService::Tick(EditorServiceContext& Context, float DeltaSeconds)
{
    (void)Context;
    (void)DeltaSeconds;

    if (!m_asyncWorkerCompleted.load())
    {
        return;
    }

    std::optional<AsyncCompletion> Completion{};
    std::thread WorkerToJoin{};
    {
        std::lock_guard Lock(m_asyncMutex);
        if (!m_asyncWorkerCompleted.load())
        {
            return;
        }

        Completion = std::move(m_asyncCompletion);
        m_asyncCompletion.reset();
        WorkerToJoin = std::move(m_asyncWorkerThread);
        m_asyncBusy = false;
        m_asyncWorkerCompleted.store(false);
    }

    if (WorkerToJoin.joinable())
    {
        WorkerToJoin.join();
    }

    if (Completion.has_value())
    {
        ApplyAsyncCompletion(std::move(*Completion));
    }
}

void EditorBuildService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    if (m_asyncWorkerThread.joinable())
    {
        m_asyncWorkerThread.join();
    }
    m_statusMessage.clear();
    m_lastReportedStatusMessage.clear();
    m_lastPlan.reset();
    m_lastReport.reset();
    ClearConsoleLog();
    {
        std::lock_guard Lock(m_asyncMutex);
        m_asyncCompletion.reset();
        m_asyncBusy = false;
        m_historyRefreshRequested = false;
    }
    m_asyncWorkerCompleted.store(false);
}

TExpected<BuildRequest> EditorBuildService::MakeDefaultRequest(EditorServiceContext& Context) const
{
    auto Project = ResolveActiveProjectDescriptor(Context);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    return CompleteRequest(*Project, {}, 4u);
}

TExpected<EditorBuildPlan> EditorBuildService::PlanActiveProject(EditorServiceContext& Context,
                                                                 const BuildRequest& Request,
                                                                 const BuildPlannerOptions& PlannerOptions,
                                                                 const std::size_t MaxInheritanceDepth)
{
    ClearConsoleLog(CurrentProjectFilePath(Context));
    AppendConsoleLog(MakeConsoleBanner("Planning Build"));
    ScopedStdStreamCapture StdCapture([this](const std::string_view Text) { AppendConsoleLog(Text); });

    auto Project = ResolveActiveProjectDescriptor(Context, &Request);
    if (!Project)
    {
        m_statusMessage = Project.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Project.error());
    }

    BuildRequest EffectiveRequest = CompleteRequest(*Project, Request, MaxInheritanceDepth);
    auto Plan = CreateEditorBuildPlan(EffectiveRequest, PlannerOptions, MaxInheritanceDepth);
    if (!Plan)
    {
        m_statusMessage = Plan.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Plan.error());
    }

    m_lastPlan = *Plan;
    m_statusMessage = "Planned build " + Plan->Graph.BuildId + " for " + Project->Descriptor.Project.Name + ".";
    MaybeReportStatusMessageToStdout();
    return *m_lastPlan;
}

TExpected<BuildExecutionReport> EditorBuildService::PackageActiveProject(EditorServiceContext& Context,
                                                                         const BuildRequest& Request,
                                                                         const BuildPlannerOptions& PlannerOptions,
                                                                         const BuildExecutionOptions& ExecutionOptions,
                                                                         const std::size_t MaxInheritanceDepth)
{
    auto Plan = PlanActiveProject(Context, Request, PlannerOptions, MaxInheritanceDepth);
    if (!Plan)
    {
        return std::unexpected(Plan.error());
    }

    AppendConsoleLog(MakeConsoleBanner("Executing Package Build"));
    ScopedStdStreamCapture StdCapture([this](const std::string_view Text) { AppendConsoleLog(Text); });

    BuildExecutionOptions EffectiveExecutionOptions =
        AttachEditorConsoleSinks(ExecutionOptions, [this](const std::string_view Text) { AppendConsoleLog(Text); });

    auto Report = BuildExecutionService::Execute(Plan->Request, Plan->Graph, EffectiveExecutionOptions);
    if (!Report)
    {
        m_statusMessage = Report.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Report.error());
    }

    m_lastReport = *Report;
    AppendReportDiagnosticsToConsole(*Report, [this](const std::string_view Text) { AppendConsoleLog(Text); });
    m_statusMessage = "Packaged build " + Report->BuildId + " with status " +
                      std::string(BuildExecutionStatusLabel(Report->Status)) + ".";
    if (const std::string Reason = DescribeBuildReportTerminalReason(*Report); !Reason.empty())
    {
        m_statusMessage += " " + Reason;
    }
    AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
    MaybeReportStatusMessageToStdout();
    return *m_lastReport;
}

TExpected<BuildExecutionReport> EditorBuildService::RetryBuild(EditorServiceContext& Context,
                                                               std::string_view SourceBuildId,
                                                               const BuildPlannerOptions& PlannerOptions,
                                                               const BuildExecutionOptions& ExecutionOptions)
{
    ClearConsoleLog(CurrentProjectFilePath(Context));
    AppendConsoleLog(MakeConsoleBanner("Retrying Build"));
    ScopedStdStreamCapture StdCapture([this](const std::string_view Text) { AppendConsoleLog(Text); });

    auto Project = ResolveActiveProjectDescriptor(Context);
    if (!Project)
    {
        m_statusMessage = Project.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Project.error());
    }

    auto Resolved = BuildRequestService::LoadResolved(BuildRequestPathFor(*Project, SourceBuildId));
    if (!Resolved)
    {
        m_statusMessage = Resolved.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Resolved.error());
    }

    if (!SameProjectFile(Resolved->Project.ProjectFilePath, Project->ProjectFilePath))
    {
        const Error ErrorValue = MakeError(EErrorCode::InvalidArgument,
                                           "Retry request belongs to a different project descriptor");
        m_statusMessage = ErrorValue.Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(ErrorValue);
    }

    auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    if (!Graph)
    {
        m_statusMessage = Graph.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Graph.error());
    }

    m_lastPlan = EditorBuildPlan{
        .Request = *Resolved,
        .Graph = *Graph,
    };

    std::optional<BuildExecutionReport> SourceReport{};
    BuildExecutionOptions EffectiveExecutionOptions =
        AttachEditorConsoleSinks(ExecutionOptions, [this](const std::string_view Text) { AppendConsoleLog(Text); });

    if (EffectiveExecutionOptions.ResumeBaselineReport == nullptr)
    {
        auto LoadedReport = BuildHistoryService::LoadReport(BuildReportPathFor(*Project, SourceBuildId));
        if (!LoadedReport)
        {
            m_statusMessage = LoadedReport.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            m_statusMessage);
            MaybeReportStatusMessageToStdout();
            return std::unexpected(LoadedReport.error());
        }

        SourceReport = std::move(*LoadedReport);
        EffectiveExecutionOptions.ResumeBaselineReport = std::addressof(*SourceReport);
    }

    auto Report = BuildExecutionService::Execute(*Resolved, *Graph, EffectiveExecutionOptions);
    if (!Report)
    {
        m_statusMessage = Report.error().Message;
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
        MaybeReportStatusMessageToStdout();
        return std::unexpected(Report.error());
    }

    m_lastReport = *Report;
    AppendReportDiagnosticsToConsole(*Report, [this](const std::string_view Text) { AppendConsoleLog(Text); });
    m_statusMessage = "Retried build " + std::string(SourceBuildId) + " as " + Report->BuildId + " with status " +
                      std::string(BuildExecutionStatusLabel(Report->Status)) + ".";
    if (const std::string Reason = DescribeBuildReportTerminalReason(*Report); !Reason.empty())
    {
        m_statusMessage += " " + Reason;
    }
    AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); }, m_statusMessage);
    MaybeReportStatusMessageToStdout();
    return *m_lastReport;
}

Result EditorBuildService::QueuePlanActiveProject(EditorServiceContext& Context,
                                                  const BuildRequest& Request,
                                                  const BuildPlannerOptions& PlannerOptions,
                                                  const std::size_t MaxInheritanceDepth)
{
    auto Project = ResolveActiveProjectDescriptor(Context, &Request);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    const BuildRequest EffectiveRequest = CompleteRequest(*Project, Request, MaxInheritanceDepth);
    ClearConsoleLog(CurrentProjectFilePath(Context));
    AppendConsoleLog(MakeConsoleBanner("Planning Build"));

    const std::string StartStatus = "Planning build for " + Project->Descriptor.Project.Name + " in the background.";
    return StartAsyncOperation(StartStatus, [this, EffectiveRequest, PlannerOptions, MaxInheritanceDepth]() mutable {
        ScopedStdStreamCapture StdCapture([this](const std::string_view Text) { AppendConsoleLog(Text); });

        AsyncCompletion Completion{};
        Completion.Kind = EAsyncOperationKind::Plan;

        auto Plan = CreateEditorBuildPlan(EffectiveRequest, PlannerOptions, MaxInheritanceDepth);
        if (!Plan)
        {
            Completion.SubmissionResult = std::unexpected(Plan.error());
            Completion.StatusMessage = Plan.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        Completion.Plan = *Plan;
        Completion.SubmissionResult = Ok();
        Completion.StatusMessage =
            "Planned build " + Plan->Graph.BuildId + " for " + Plan->Request.Project.Descriptor.Project.Name + ".";
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                        Completion.StatusMessage);
        return Completion;
    });
}

Result EditorBuildService::QueuePackageActiveProject(EditorServiceContext& Context,
                                                     const BuildRequest& Request,
                                                     const BuildPlannerOptions& PlannerOptions,
                                                     const BuildExecutionOptions& ExecutionOptions,
                                                     const std::size_t MaxInheritanceDepth)
{
    auto Project = ResolveActiveProjectDescriptor(Context, &Request);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    const BuildRequest EffectiveRequest = CompleteRequest(*Project, Request, MaxInheritanceDepth);
    ClearConsoleLog(CurrentProjectFilePath(Context));
    const std::string ProjectName = Project->Descriptor.Project.Name;

    const std::string StartStatus = "Packaging " + ProjectName + " in the background.";
    return StartAsyncOperation(StartStatus,
                               [this, EffectiveRequest, PlannerOptions, ExecutionOptions, MaxInheritanceDepth,
                                ProjectName]() mutable {
        ScopedStdStreamCapture StdCapture([this](const std::string_view Text) { AppendConsoleLog(Text); });

        AsyncCompletion Completion{};
        Completion.Kind = EAsyncOperationKind::Package;
        AppendConsoleLog(MakeConsoleBanner("Planning Build"));

        auto Plan = CreateEditorBuildPlan(EffectiveRequest, PlannerOptions, MaxInheritanceDepth);
        if (!Plan)
        {
            Completion.SubmissionResult = std::unexpected(Plan.error());
            Completion.StatusMessage = Plan.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        Completion.Plan = *Plan;
        AppendConsoleLog(MakeConsoleBanner("Executing Package Build"));
        BuildExecutionOptions EffectiveExecutionOptions =
            AttachEditorConsoleSinks(ExecutionOptions, [this](const std::string_view Text) { AppendConsoleLog(Text); });

        auto Report = BuildExecutionService::Execute(Plan->Request, Plan->Graph, EffectiveExecutionOptions);
        if (!Report)
        {
            Completion.SubmissionResult = std::unexpected(Report.error());
            Completion.StatusMessage = Report.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        Completion.Report = *Report;
        Completion.SubmissionResult = Ok();
        Completion.RequestHistoryRefresh = true;
        AppendReportDiagnosticsToConsole(*Report, [this](const std::string_view Text) { AppendConsoleLog(Text); });
        Completion.StatusMessage = "Packaged build " + Report->BuildId + " with status " +
                                   std::string(BuildExecutionStatusLabel(Report->Status)) + ".";
        if (const std::string Reason = DescribeBuildReportTerminalReason(*Report); !Reason.empty())
        {
            Completion.StatusMessage += " " + Reason;
        }
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                        Completion.StatusMessage);
        return Completion;
    });
}

Result EditorBuildService::QueueRetryBuild(EditorServiceContext& Context,
                                           std::string_view SourceBuildId,
                                           const BuildPlannerOptions& PlannerOptions,
                                           const BuildExecutionOptions& ExecutionOptions)
{
    auto Project = ResolveActiveProjectDescriptor(Context);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    ClearConsoleLog(CurrentProjectFilePath(Context));
    AppendConsoleLog(MakeConsoleBanner("Retrying Build"));

    const std::string StartStatus = "Retrying build " + std::string(SourceBuildId) + " in the background.";
    return StartAsyncOperation(StartStatus,
                               [this, Project = *Project, SourceBuildId = std::string(SourceBuildId), PlannerOptions,
                                ExecutionOptions]() mutable {
        ScopedStdStreamCapture StdCapture([this](const std::string_view Text) { AppendConsoleLog(Text); });

        AsyncCompletion Completion{};
        Completion.Kind = EAsyncOperationKind::Retry;

        auto Resolved = BuildRequestService::LoadResolved(BuildRequestPathFor(Project, SourceBuildId));
        if (!Resolved)
        {
            Completion.SubmissionResult = std::unexpected(Resolved.error());
            Completion.StatusMessage = Resolved.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        if (!SameProjectFile(Resolved->Project.ProjectFilePath, Project.ProjectFilePath))
        {
            const Error ErrorValue = MakeError(EErrorCode::InvalidArgument,
                                               "Retry request belongs to a different project descriptor");
            Completion.SubmissionResult = std::unexpected(ErrorValue);
            Completion.StatusMessage = ErrorValue.Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
        if (!Graph)
        {
            Completion.SubmissionResult = std::unexpected(Graph.error());
            Completion.StatusMessage = Graph.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        Completion.Plan = EditorBuildPlan{
            .Request = *Resolved,
            .Graph = *Graph,
        };

        std::optional<BuildExecutionReport> SourceReport{};
        BuildExecutionOptions EffectiveExecutionOptions =
            AttachEditorConsoleSinks(ExecutionOptions, [this](const std::string_view Text) { AppendConsoleLog(Text); });

        if (EffectiveExecutionOptions.ResumeBaselineReport == nullptr)
        {
            auto LoadedReport = BuildHistoryService::LoadReport(BuildReportPathFor(Project, SourceBuildId));
            if (!LoadedReport)
            {
                Completion.SubmissionResult = std::unexpected(LoadedReport.error());
                Completion.StatusMessage = LoadedReport.error().Message;
                AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                                Completion.StatusMessage);
                return Completion;
            }

            SourceReport = std::move(*LoadedReport);
            EffectiveExecutionOptions.ResumeBaselineReport = std::addressof(*SourceReport);
        }

        auto Report = BuildExecutionService::Execute(*Resolved, *Graph, EffectiveExecutionOptions);
        if (!Report)
        {
            Completion.SubmissionResult = std::unexpected(Report.error());
            Completion.StatusMessage = Report.error().Message;
            AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                            Completion.StatusMessage);
            return Completion;
        }

        Completion.Report = *Report;
        Completion.SubmissionResult = Ok();
        Completion.RequestHistoryRefresh = true;
        AppendReportDiagnosticsToConsole(*Report, [this](const std::string_view Text) { AppendConsoleLog(Text); });
        Completion.StatusMessage = "Retried build " + SourceBuildId + " as " + Report->BuildId + " with status " +
                                   std::string(BuildExecutionStatusLabel(Report->Status)) + ".";
        if (const std::string Reason = DescribeBuildReportTerminalReason(*Report); !Reason.empty())
        {
            Completion.StatusMessage += " " + Reason;
        }
        AppendTerminalStatusConsoleLine([this](const std::string_view Text) { AppendConsoleLog(Text); },
                                        Completion.StatusMessage);
        return Completion;
    });
}

TExpected<std::vector<EditorBuildProfileSummary>> EditorBuildService::ListProfiles(EditorServiceContext& Context,
                                                                                   const std::size_t MaxInheritanceDepth) const
{
    auto Project = ResolveActiveProjectDescriptor(Context);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    std::vector<EditorBuildProfileSummary> Profiles{};
    Profiles.reserve(Project->Descriptor.Profiles.size() + 1u);

    const BuildRequest DefaultRequest = CompleteRequest(*Project, {}, MaxInheritanceDepth);
    const std::string DefaultProfileName = DefaultRequest.ProfileName;

    EditorBuildProfileSummary AdHoc{};
    AdHoc.Name.clear();
    AdHoc.Label = "Ad Hoc Host Development";
    AdHoc.Platform = HostPlatformName();
    AdHoc.Configuration = std::string(BuildConfigurationLabel(EBuildConfiguration::Development));
    AdHoc.ExecutionEnvironment = "host-local";
    AdHoc.Summary = AdHoc.Platform + " / " + AdHoc.Configuration + " / " + AdHoc.ExecutionEnvironment;
    AdHoc.DependencyPolicy = EAssetDependencyPolicy::HardAndSoft;
    AdHoc.ChunkStrategy = EAssetChunkStrategy::Monolithic;
    AdHoc.IsDefault = DefaultProfileName.empty();
    AdHoc.IsAdHoc = true;
    Profiles.emplace_back(std::move(AdHoc));

    for (const BuildProfile& Profile : Project->Descriptor.Profiles)
    {
        if (TrimCopy(Profile.Name).empty())
        {
            continue;
        }

        auto Resolved = BuildProfileService::ResolveProfile(Project->Descriptor.Profiles, Profile.Name, MaxInheritanceDepth);
        if (!Resolved)
        {
            continue;
        }

        EditorBuildProfileSummary Summary{};
        Summary.Name = Profile.Name;
        Summary.Label = Profile.Name;
        Summary.Platform = Resolved->Platform;
        Summary.Configuration = std::string(BuildConfigurationLabel(Resolved->Configuration));
        Summary.ExecutionEnvironment = Resolved->ExecutionEnvironment;
        Summary.Summary = MakeProfileSummary(*Resolved);
        Summary.SelectedLevels = Resolved->SelectedLevels;
        Summary.ExplicitAssets = Resolved->ExplicitAssets;
        Summary.IncludeFolders = Resolved->IncludeFolders;
        Summary.ExcludeFolders = Resolved->ExcludeFolders;
        Summary.IncludeAssetLabels = Resolved->IncludeAssetLabels;
        Summary.ExcludeAssetLabels = Resolved->ExcludeAssetLabels;
        Summary.IncludeAssetKinds = Resolved->IncludeAssetKinds;
        Summary.ExcludeAssetKinds = Resolved->ExcludeAssetKinds;
        Summary.DependencyPolicy = Resolved->DependencyPolicy;
        Summary.ChunkStrategy = Resolved->ChunkStrategy;
        Summary.AllowExplicitOverrideExcludes = Resolved->AllowExplicitOverrideExcludes;
        Summary.ArchiveEnabled = Resolved->ArchiveEnabled;
        Summary.ArchiveFormat = Resolved->ArchiveFormat;
        Summary.IsDefault = (Profile.Name == DefaultProfileName);
        Summary.IsAdHoc = false;
        Profiles.emplace_back(std::move(Summary));
    }

    return Profiles;
}

TExpected<std::vector<BuildHistoryEntry>> EditorBuildService::ListHistory(EditorServiceContext& Context,
                                                                          const BuildHistoryListOptions& Options) const
{
    auto Project = ResolveActiveProjectDescriptor(Context);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    return BuildHistoryService::List(Project->SavedRootDirectory, Options);
}

TExpected<BuildExecutionReport> EditorBuildService::LoadHistoryReport(EditorServiceContext& Context,
                                                                      std::string_view BuildId) const
{
    auto Project = ResolveActiveProjectDescriptor(Context);
    if (!Project)
    {
        return std::unexpected(Project.error());
    }

    return BuildHistoryService::LoadReport(BuildReportPathFor(*Project, BuildId));
}

TExpected<BuildHistoryComparison> EditorBuildService::CompareHistory(EditorServiceContext& Context,
                                                                     std::string_view LeftBuildId,
                                                                     std::string_view RightBuildId) const
{
    auto LeftReport = LoadHistoryReport(Context, LeftBuildId);
    if (!LeftReport)
    {
        return std::unexpected(LeftReport.error());
    }

    auto RightReport = LoadHistoryReport(Context, RightBuildId);
    if (!RightReport)
    {
        return std::unexpected(RightReport.error());
    }

    return BuildHistoryService::Compare(*LeftReport, *RightReport);
}

const std::string& EditorBuildService::StatusMessage() const
{
    MaybeReportStatusMessageToStdout();
    return m_statusMessage;
}

bool EditorBuildService::IsBusy() const
{
    std::lock_guard Lock(m_asyncMutex);
    return m_asyncBusy;
}

bool EditorBuildService::ConsumeHistoryRefreshRequested()
{
    std::lock_guard Lock(m_asyncMutex);
    const bool Requested = m_historyRefreshRequested;
    m_historyRefreshRequested = false;
    return Requested;
}

std::string EditorBuildService::ConsoleLogText() const
{
    std::lock_guard Lock(m_consoleLogMutex);
    return BuildConsoleDisplayText(m_consoleLogText);
}

std::uint64_t EditorBuildService::ConsoleLogRevision() const
{
    std::lock_guard Lock(m_consoleLogMutex);
    return m_consoleLogRevision;
}

std::filesystem::path EditorBuildService::ConsoleLogProjectFilePath() const
{
    std::lock_guard Lock(m_consoleLogMutex);
    return m_consoleLogProjectFilePath;
}

void EditorBuildService::MaybeReportStatusMessageToStdout() const
{
    if (m_statusMessage.empty() || m_statusMessage == m_lastReportedStatusMessage)
    {
        return;
    }

    std::printf("[SnAPI][EditorBuild] %s\n", m_statusMessage.c_str());
    std::fflush(stdout);
    m_lastReportedStatusMessage = m_statusMessage;
}

Result EditorBuildService::StartAsyncOperation(std::string StartStatus, std::function<AsyncCompletion()> Work)
{
    std::thread WorkerToJoin{};
    {
        std::lock_guard Lock(m_asyncMutex);
        if (m_asyncBusy)
        {
            return std::unexpected(
                MakeError(EErrorCode::NotReady, "A build or packaging task is already running in the background"));
        }

        if (m_asyncWorkerThread.joinable())
        {
            WorkerToJoin = std::move(m_asyncWorkerThread);
        }

        m_asyncCompletion.reset();
        m_asyncBusy = true;
        m_asyncWorkerCompleted.store(false);
    }

    if (WorkerToJoin.joinable())
    {
        WorkerToJoin.join();
    }

    m_statusMessage = std::move(StartStatus);
    MaybeReportStatusMessageToStdout();
    m_asyncWorkerThread = std::thread([this, Work = std::move(Work)]() mutable {
        AsyncCompletion Completion = Work();
        {
            std::lock_guard Lock(m_asyncMutex);
            m_asyncCompletion = std::move(Completion);
        }
        m_asyncWorkerCompleted.store(true);
    });
    return Ok();
}

void EditorBuildService::ApplyAsyncCompletion(AsyncCompletion Completion)
{
    if (Completion.Plan.has_value())
    {
        m_lastPlan = std::move(Completion.Plan);
    }
    if (Completion.Report.has_value())
    {
        m_lastReport = std::move(Completion.Report);
    }

    m_statusMessage = std::move(Completion.StatusMessage);
    {
        std::lock_guard Lock(m_asyncMutex);
        if (Completion.RequestHistoryRefresh)
        {
            m_historyRefreshRequested = true;
        }
    }
    MaybeReportStatusMessageToStdout();
}

void EditorBuildService::ClearConsoleLog(const std::filesystem::path& ProjectFilePath)
{
    std::lock_guard Lock(m_consoleLogMutex);
    m_consoleLogText.clear();
    m_consoleLogRevision += 1u;
    m_consoleLogProjectFilePath = ProjectFilePath.lexically_normal();
}

void EditorBuildService::AppendConsoleLog(const std::string_view Text)
{
    if (Text.empty())
    {
        return;
    }

    std::lock_guard Lock(m_consoleLogMutex);
    AppendSanitizedConsoleChunk(m_consoleLogText, Text);
    TrimConsoleLogLocked();
    NormalizeConsoleBuffer(m_consoleLogText);
    m_consoleLogRevision += 1u;
}

void EditorBuildService::TrimConsoleLogLocked()
{
    if (m_consoleLogText.size() <= kMaxConsoleLogBytes)
    {
        return;
    }

    const std::size_t Overflow = m_consoleLogText.size() - kMaxConsoleLogBytes;
    std::size_t TrimCount = Overflow;
    const std::size_t Newline = m_consoleLogText.find('\n', Overflow);
    if (Newline != std::string::npos && Newline + 1u < m_consoleLogText.size())
    {
        TrimCount = Newline + 1u;
    }

    m_consoleLogText.erase(0u, TrimCount);
}

} // namespace SnAPI::GameFramework::Editor
