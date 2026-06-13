#pragma once

#include "BuildExecution.h"
#include "BuildHistory.h"
#include "BuildPlanner.h"
#include "BuildRequest.h"
#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"

#include <optional>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <typeindex>
#include <vector>

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Frozen editor build plan produced for the currently loaded project.
 *
 * The editor build service deliberately reuses the same resolved request and build
 * graph types as the CLI and backend build services so editor-triggered builds do
 * not drift into a second planning model.
 */
struct EditorBuildPlan
{
    ResolvedBuildRequest Request{}; /**< @brief Frozen request resolved from the active project plus optional overrides. */
    BuildGraph Graph{}; /**< @brief Deterministic build graph planned from `Request`. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Lightweight resolved profile summary surfaced to editor UI.
 */
struct EditorBuildProfileSummary
{
    std::string Name{}; /**< @brief Stable authored profile name, or empty for the ad hoc host-default fallback. */
    std::string Label{}; /**< @brief User-facing label suitable for selectors and menus. */
    std::string Summary{}; /**< @brief Concise resolved profile summary. */
    std::string Platform{}; /**< @brief Resolved target platform label. */
    std::string Configuration{}; /**< @brief Resolved build configuration label. */
    std::string ExecutionEnvironment{}; /**< @brief Resolved execution-environment label. */
    std::vector<std::string> SelectedLevels{}; /**< @brief Resolved selected-level seed values. */
    std::vector<std::string> ExplicitAssets{}; /**< @brief Resolved explicit-asset seed values. */
    std::vector<std::string> IncludeFolders{}; /**< @brief Resolved include-folder seed values. */
    std::vector<std::string> ExcludeFolders{}; /**< @brief Resolved exclude-folder seed values. */
    std::vector<std::string> IncludeAssetLabels{}; /**< @brief Resolved include-label seed values. */
    std::vector<std::string> ExcludeAssetLabels{}; /**< @brief Resolved exclude-label seed values. */
    std::vector<std::string> IncludeAssetKinds{}; /**< @brief Resolved include-kind seed values. */
    std::vector<std::string> ExcludeAssetKinds{}; /**< @brief Resolved exclude-kind seed values. */
    EAssetDependencyPolicy DependencyPolicy = EAssetDependencyPolicy::HardOnly; /**< @brief Resolved asset dependency policy. */
    EAssetChunkStrategy ChunkStrategy = EAssetChunkStrategy::Monolithic; /**< @brief Resolved asset chunk strategy. */
    bool AllowExplicitOverrideExcludes = false; /**< @brief Resolved explicit-include precedence flag. */
    bool ArchiveEnabled = false; /**< @brief Resolved archive toggle. */
    std::string ArchiveFormat{}; /**< @brief Resolved archive format. */
    bool IsDefault = false; /**< @brief `true` when the profile matches the current default request. */
    bool IsAdHoc = false; /**< @brief `true` when the row represents the host-local fallback request. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Editor-facing build, packaging, retry, and history backend.
 *
 * `EditorBuildService` is the editor module's bridge onto the shared production
 * build pipeline. It does not shell out through the CLI. Instead it resolves the
 * active project into the same `BuildRequest`, `ResolvedBuildRequest`, `BuildGraph`,
 * `BuildExecutionReport`, and `BuildHistory` types used by the CLI and automation
 * surfaces.
 *
 * Core responsibilities:
 * - derive one build request from the currently loaded editor project
 * - plan and execute package builds through the shared backend services
 * - reload frozen build requests from history for retry flows
 * - enumerate, load, and compare project-local build history
 * - retain lightweight last-operation state for editor-shell integration
 * - queue heavyweight planning and packaging work onto a background thread so the
 *   editor shell remains responsive while configure/build/cook/package work runs
 *
 * Core semantics:
 * - The service only operates on the project currently loaded by `EditorAssetService`.
 * - When the caller does not provide a fully specified request, the service chooses
 *   one sane default profile or host-local development request so packaging is still
 *   invokable directly from the editor shell.
 * - The synchronous APIs remain available for tests and direct service use, while
 *   the editor shell queues heavyweight operations onto a worker thread and polls
 *   completion during `Tick()`.
 *
 * Ownership and lifetime:
 * - Owned by `GameEditor` through the `IEditorService` contract.
 * - Returned reports, plans, and history entries are value types.
 *
 * Threading model:
 * - UI-facing state is polled and applied on the main thread.
 * - Heavyweight planning/execution work may run on one background worker thread.
 *
 * @see BuildRequestService
 * @see BuildPlannerService
 * @see BuildExecutionService
 * @see BuildHistoryService
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorBuildService final : public IEditorService
{
public:
    /** @brief Stable service name for diagnostics. @return Borrowed static string view. */
    [[nodiscard]] std::string_view Name() const override;

    /**
     * @brief Declare required editor-service dependencies.
     * @return Exact concrete dependency types.
     * @remarks The service requires `EditorAssetService` so it can resolve the active project.
     */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;

    /**
     * @brief Initialize editor build state.
     * @param Context Borrowed editor-service context.
     * @return Success or an initialization error.
     */
    Result Initialize(EditorServiceContext& Context) override;

    /**
     * @brief Poll background build work and apply completed results to editor-visible state.
     * @param Context Borrowed editor-service context.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;

    /**
     * @brief Shutdown the build service and clear cached last-operation state.
     * @param Context Borrowed editor-service context.
     */
    void Shutdown(EditorServiceContext& Context) override;

    /**
     * @brief Build one default request for the currently loaded project.
     * @param Context Borrowed editor-service context.
     * @return Concrete request seeded with the active project file and default profile/host settings.
     */
    [[nodiscard]] TExpected<BuildRequest> MakeDefaultRequest(EditorServiceContext& Context) const;

    /**
     * @brief Plan one build for the currently loaded project.
     * @param Context Borrowed editor-service context.
     * @param Request Optional partial request. The active project file is injected when omitted.
     * @param PlannerOptions Optional planner overrides such as a fixed build id.
     * @param MaxInheritanceDepth Maximum allowed build-profile inheritance depth.
     * @return Frozen request plus planned graph, or a structured error.
     */
    [[nodiscard]] TExpected<EditorBuildPlan> PlanActiveProject(EditorServiceContext& Context,
                                                               const BuildRequest& Request = {},
                                                               const BuildPlannerOptions& PlannerOptions = {},
                                                               std::size_t MaxInheritanceDepth = 4u);

    /**
     * @brief Execute one package build for the currently loaded project.
     * @param Context Borrowed editor-service context.
     * @param Request Optional partial request. The active project file is injected when omitted.
     * @param PlannerOptions Optional planner overrides such as a fixed build id.
     * @param ExecutionOptions Optional execution options.
     * @param MaxInheritanceDepth Maximum allowed build-profile inheritance depth.
     * @return Final build report or a structured error.
     */
    [[nodiscard]] TExpected<BuildExecutionReport> PackageActiveProject(EditorServiceContext& Context,
                                                                       const BuildRequest& Request = {},
                                                                       const BuildPlannerOptions& PlannerOptions = {},
                                                                       const BuildExecutionOptions& ExecutionOptions = {},
                                                                       std::size_t MaxInheritanceDepth = 4u);

    /**
     * @brief Retry one prior build from the active project's stored frozen request.
     * @param Context Borrowed editor-service context.
     * @param SourceBuildId Build id whose `BuildRequest.json` should be reloaded.
     * @param PlannerOptions Optional planner overrides such as a new build id.
     * @param ExecutionOptions Optional execution options.
     * @return Final build report or a structured error.
     */
    [[nodiscard]] TExpected<BuildExecutionReport> RetryBuild(EditorServiceContext& Context,
                                                             std::string_view SourceBuildId,
                                                             const BuildPlannerOptions& PlannerOptions = {},
                                                             const BuildExecutionOptions& ExecutionOptions = {});

    /**
     * @brief Queue one background planning task for the active project.
     * @param Context Borrowed editor-service context.
     * @param Request Optional partial request. The active project file is injected when omitted.
     * @param PlannerOptions Optional planner overrides such as a fixed build id.
     * @param MaxInheritanceDepth Maximum allowed build-profile inheritance depth.
     * @return `Ok()` when the task was queued or a structured submission error.
     */
    [[nodiscard]] Result QueuePlanActiveProject(EditorServiceContext& Context,
                                                const BuildRequest& Request = {},
                                                const BuildPlannerOptions& PlannerOptions = {},
                                                std::size_t MaxInheritanceDepth = 4u);

    /**
     * @brief Queue one background package build for the active project.
     * @param Context Borrowed editor-service context.
     * @param Request Optional partial request. The active project file is injected when omitted.
     * @param PlannerOptions Optional planner overrides such as a fixed build id.
     * @param ExecutionOptions Optional execution options.
     * @param MaxInheritanceDepth Maximum allowed build-profile inheritance depth.
     * @return `Ok()` when the task was queued or a structured submission error.
     */
    [[nodiscard]] Result QueuePackageActiveProject(EditorServiceContext& Context,
                                                   const BuildRequest& Request = {},
                                                   const BuildPlannerOptions& PlannerOptions = {},
                                                   const BuildExecutionOptions& ExecutionOptions = {},
                                                   std::size_t MaxInheritanceDepth = 4u);

    /**
     * @brief Queue one background retry/rebuild task for the active project.
     * @param Context Borrowed editor-service context.
     * @param SourceBuildId Build id whose `BuildRequest.json` should be reloaded.
     * @param PlannerOptions Optional planner overrides such as a new build id.
     * @param ExecutionOptions Optional execution options.
     * @return `Ok()` when the task was queued or a structured submission error.
     */
    [[nodiscard]] Result QueueRetryBuild(EditorServiceContext& Context,
                                         std::string_view SourceBuildId,
                                         const BuildPlannerOptions& PlannerOptions = {},
                                         const BuildExecutionOptions& ExecutionOptions = {});

    /**
     * @brief Resolve profile summaries for the active project.
     * @param Context Borrowed editor-service context.
     * @param MaxInheritanceDepth Maximum allowed build-profile inheritance depth.
     * @return Resolved profile summaries in UI order, including the ad hoc host-default fallback.
     */
    [[nodiscard]] TExpected<std::vector<EditorBuildProfileSummary>> ListProfiles(EditorServiceContext& Context,
                                                                                 std::size_t MaxInheritanceDepth = 4) const;

    /**
     * @brief List project-local build history for the active project.
     * @param Context Borrowed editor-service context.
     * @param Options Optional history-list filters.
     * @return Ordered build-history entries or a structured error.
     */
    [[nodiscard]] TExpected<std::vector<BuildHistoryEntry>> ListHistory(
        EditorServiceContext& Context, const BuildHistoryListOptions& Options = {}) const;

    /**
     * @brief Load one build report from the active project's history.
     * @param Context Borrowed editor-service context.
     * @param BuildId Build id whose `BuildReport.json` should be loaded.
     * @return Parsed build report or a structured error.
     */
    [[nodiscard]] TExpected<BuildExecutionReport> LoadHistoryReport(EditorServiceContext& Context,
                                                                    std::string_view BuildId) const;

    /**
     * @brief Compare two active-project build-history entries.
     * @param Context Borrowed editor-service context.
     * @param LeftBuildId Left-side build id.
     * @param RightBuildId Right-side build id.
     * @return Computed history comparison or a structured error.
     */
    [[nodiscard]] TExpected<BuildHistoryComparison> CompareHistory(EditorServiceContext& Context,
                                                                   std::string_view LeftBuildId,
                                                                   std::string_view RightBuildId) const;

    /**
     * @brief Access the latest editor build status message.
     * @return Borrowed status string reference.
     */
    [[nodiscard]] const std::string& StatusMessage() const;

    /** @brief Access the most recent successful plan or execution planning result. */
    [[nodiscard]] const std::optional<EditorBuildPlan>& LastPlan() const { return m_lastPlan; }

    /** @brief Access the most recent execution report, when any. */
    [[nodiscard]] const std::optional<BuildExecutionReport>& LastReport() const { return m_lastReport; }

    /**
     * @brief Snapshot the active editor build console log.
     * @return Copy of the UI-safe recent console text captured for the active project.
     * @remarks
     * The service retains a larger bounded raw transcript internally, but this accessor returns a
     * normalized recent tail window suitable for immediate `UIText` presentation without exhausting
     * the renderer packet glyph budget.
     */
    [[nodiscard]] std::string ConsoleLogText() const;

    /**
     * @brief Query the revision token for the rolling console log.
     * @return Monotonic revision incremented whenever the captured console text changes.
     */
    [[nodiscard]] std::uint64_t ConsoleLogRevision() const;

    /**
     * @brief Query which project descriptor currently owns the console session.
     * @return Absolute project-file path associated with the captured console text, or an empty path when unset.
     */
    [[nodiscard]] std::filesystem::path ConsoleLogProjectFilePath() const;

    /**
     * @brief Query whether a background build/planning task is currently running or awaiting completion polling.
     * @return `true` while the service is busy with an asynchronous task.
     */
    [[nodiscard]] bool IsBusy() const;

    /**
     * @brief Consume the "history should be refreshed" flag raised by background completions.
     * @return `true` when the caller should reload build history now.
     */
    [[nodiscard]] bool ConsumeHistoryRefreshRequested();

private:
    enum class EAsyncOperationKind : std::uint8_t
    {
        Plan = 0,
        Package,
        Retry,
    };

    struct AsyncCompletion
    {
        EAsyncOperationKind Kind = EAsyncOperationKind::Plan; /**< @brief Completed background operation kind. */
        std::optional<EditorBuildPlan> Plan{}; /**< @brief Planned graph produced by the operation, when any. */
        std::optional<BuildExecutionReport> Report{}; /**< @brief Final execution report for package/retry operations, when any. */
        Result SubmissionResult = Ok(); /**< @brief Final success or failure for the completed operation. */
        std::string StatusMessage{}; /**< @brief Final user-facing status text to apply on the main thread. */
        bool RequestHistoryRefresh = false; /**< @brief `true` when build history should be reloaded after completion. */
    };

    /**
     * @brief Report the current status message to stdout once per unique message.
     * @remarks This mirrors the lightweight status-reporting pattern used by other editor services.
     */
    void MaybeReportStatusMessageToStdout() const;

    /**
     * @brief Reset the rolling console log for one project-scoped build session.
     * @param ProjectFilePath Active project descriptor path that owns the new console session.
     */
    void ClearConsoleLog(const std::filesystem::path& ProjectFilePath = {});

    /**
     * @brief Append one UTF-8 text chunk to the rolling console log.
     * @param Text Console text chunk to append.
     * @remarks This helper is safe to call from the background stdout/stderr capture thread.
     */
    void AppendConsoleLog(std::string_view Text);

    /**
     * @brief Trim the rolling console log to the bounded in-memory retention window.
     * @remarks Caller must hold `m_consoleLogMutex`.
     */
    void TrimConsoleLogLocked();

    /**
     * @brief Start one background worker task when the service is idle.
     * @param StartStatus Immediate status line shown while the task is running.
     * @param Work Background task that performs the heavy build work.
     * @return `Ok()` when the task was queued or a structured submission error.
     */
    [[nodiscard]] Result StartAsyncOperation(std::string StartStatus, std::function<AsyncCompletion()> Work);

    /**
     * @brief Apply one completed background task to editor-visible state.
     * @param Completion Completed background task payload.
     */
    void ApplyAsyncCompletion(AsyncCompletion Completion);

    std::string m_statusMessage{}; /**< @brief Latest editor build status text. */
    mutable std::string m_lastReportedStatusMessage{}; /**< @brief Last status text printed to stdout. */
    std::optional<EditorBuildPlan> m_lastPlan{}; /**< @brief Most recently planned build. */
    std::optional<BuildExecutionReport> m_lastReport{}; /**< @brief Most recently executed build report. */
    mutable std::mutex m_consoleLogMutex{}; /**< @brief Guards the rolling console log shared with the stdout capture thread. */
    std::string m_consoleLogText{}; /**< @brief Bounded rolling console text shown in the packaging modal. */
    std::uint64_t m_consoleLogRevision = 0u; /**< @brief Monotonic revision token for lightweight UI invalidation. */
    std::filesystem::path m_consoleLogProjectFilePath{}; /**< @brief Project descriptor path associated with the current console session. */
    mutable std::mutex m_asyncMutex{}; /**< @brief Guards background-worker state exchanged with the UI thread. */
    std::thread m_asyncWorkerThread{}; /**< @brief Single background worker thread used for planning and package execution. */
    std::optional<AsyncCompletion> m_asyncCompletion{}; /**< @brief Completed background result waiting for `Tick()` to apply it. */
    std::atomic<bool> m_asyncWorkerCompleted = false; /**< @brief `true` once the worker produced `m_asyncCompletion`. */
    bool m_asyncBusy = false; /**< @brief `true` while one asynchronous build task is running or awaiting completion polling. */
    bool m_historyRefreshRequested = false; /**< @brief `true` when the editor shell should reload build history. */
};

} // namespace SnAPI::GameFramework::Editor
