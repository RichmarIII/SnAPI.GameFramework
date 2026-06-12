#pragma once

#include "AssetCookServiceAdapter.h"
#include "BuildCache.h"
#include "BuildPlanner.h"
#include "CodeBuildServiceAdapter.h"
#include "Expected.h"
#include "Export.h"
#include "PackageOutput.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Overall build execution status recorded in reports and summaries.
     */
    enum class EBuildExecutionStatus : std::uint8_t
    {
        Succeeded = 0, /**< @brief All planned nodes completed successfully. */
        Failed, /**< @brief At least one planned node failed. */
        Cancelled, /**< @brief Execution stopped before completion. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Per-node execution status recorded in build reports.
     */
    enum class EBuildNodeExecutionStatus : std::uint8_t
    {
        Succeeded = 0, /**< @brief Node completed successfully. */
        Failed, /**< @brief Node failed with a blocking error. */
        Cancelled, /**< @brief Node did not complete because execution was cancelled. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Structured cancellation reason recorded when one build ends in `Cancelled`.
     */
    enum class EBuildCancellationReason : std::uint8_t
    {
        None = 0, /**< @brief Build did not end in a cancelled state. */
        UserRequested, /**< @brief A user-initiated cancellation request stopped execution. */
        DependencyFailure, /**< @brief Execution stopped because required upstream work failed. */
        HostShutdown, /**< @brief Execution stopped because the host or process was shutting down. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Structured event kind emitted while executing one build graph.
     */
    enum class EBuildEventKind : std::uint8_t
    {
        BuildStarted = 0, /**< @brief Build execution began. */
        BuildPlanReady, /**< @brief The frozen request and build plan were written and are ready for inspection. */
        ValidationIssueRaised, /**< @brief Validation issue surfaced during execution startup. */
        NodeQueued, /**< @brief Node was queued for execution. */
        NodeStarted, /**< @brief Node execution started. */
        NodeProgress, /**< @brief Node reported intermediate progress. */
        NodeCacheHit, /**< @brief Node completed via cache reuse. */
        NodeFinished, /**< @brief Node completed successfully. */
        NodeFailed, /**< @brief Node failed. */
        BuildCancelled, /**< @brief Build execution was cancelled before completion. */
        BuildFinished, /**< @brief Build execution finished with a terminal status. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One structured build event emitted by `BuildExecutionService`.
     *
     * Events are the shared progress/reporting surface for editor UI, CLI, tests,
     * and per-stage log writing. `Payload` is intentionally optional and should
     * contain compact machine-readable context rather than large data dumps.
     */
    struct BuildEvent
    {
        EBuildEventKind Kind = EBuildEventKind::BuildStarted; /**< @brief Event category. */
        EBuildValidationSeverity Severity =
            EBuildValidationSeverity::Info; /**< @brief User-facing event severity. */
        std::string TimestampUtc{}; /**< @brief ISO-8601 UTC timestamp for the event. */
        EBuildStage Stage = EBuildStage::Preflight; /**< @brief Stage that produced the event. */
        std::uint32_t NodeId = 0u; /**< @brief Owning node id, or `0` for build-level events. */
        std::string Message{}; /**< @brief Human-readable event message. */
        nlohmann::ordered_json Payload =
            nlohmann::ordered_json::object(); /**< @brief Optional structured machine payload. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Execution result returned by one build-node executor.
     *
     * Executors report whether the node reused cache state, any output paths that
     * were materially produced, and a concise diagnostic message for reports/logs.
     */
    struct BuildNodeExecutionResult
    {
        bool CacheHit = false; /**< @brief `true` when the node reused cache and skipped real work. */
        std::string Message{}; /**< @brief Concise node-level execution detail. */
        std::vector<std::string> Outputs{}; /**< @brief Materialized output file paths for the node. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One node-execution record stored in the final build report.
     */
    struct BuildNodeExecutionRecord
    {
        std::uint32_t NodeId = 0u; /**< @brief Executed node id. */
        EBuildStage Stage = EBuildStage::Preflight; /**< @brief Executed node stage. */
        EBuildNodeType Type = EBuildNodeType::LoadProject; /**< @brief Executed node type. */
        std::string Name{}; /**< @brief Human-readable node name. */
        EBuildNodeExecutionStatus Status = EBuildNodeExecutionStatus::Succeeded; /**< @brief Terminal node status. */
        bool CacheHit = false; /**< @brief `true` when the node reused cache. */
        std::string StartedAtUtc{}; /**< @brief ISO-8601 UTC start timestamp. */
        std::string FinishedAtUtc{}; /**< @brief ISO-8601 UTC finish timestamp. */
        std::uint64_t DurationMilliseconds = 0u; /**< @brief Node wall-clock duration in milliseconds. */
        std::string Message{}; /**< @brief Node-level summary or failure message. */
        std::vector<std::string> Outputs{}; /**< @brief Materialized output file paths. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Final execution report written for one build invocation.
     *
     * This report is the machine-readable summary of one executed build plan. It
     * records the build identity, terminal status, validation issues, node-level
     * execution details, and the canonical history artifact locations.
     */
    struct BuildExecutionReport
    {
        std::string BuildId{}; /**< @brief Unique build invocation id. */
        std::string RequestHash{}; /**< @brief Frozen request hash. */
        EBuildExecutionStatus Status = EBuildExecutionStatus::Succeeded; /**< @brief Terminal build status. */
        EBuildCancellationReason CancellationReason =
            EBuildCancellationReason::None; /**< @brief Recorded cancellation reason when `Status` is `Cancelled`. */
        std::string StartedAtUtc{}; /**< @brief ISO-8601 UTC build start timestamp. */
        std::string FinishedAtUtc{}; /**< @brief ISO-8601 UTC build finish timestamp. */
        std::uint64_t DurationMilliseconds = 0u; /**< @brief Total execution duration in milliseconds. */
        std::uint64_t EventCount = 0u; /**< @brief Number of structured events emitted during execution. */
        std::filesystem::path HistoryDirectory{}; /**< @brief Build history directory for the invocation. */
        std::filesystem::path StageDirectory{}; /**< @brief Planned staging directory root. */
        std::filesystem::path BuildRequestFilePath{}; /**< @brief Frozen request artifact path. */
        std::filesystem::path BuildPlanFilePath{}; /**< @brief Planned build-graph artifact path. */
        std::filesystem::path BuildReportFilePath{}; /**< @brief Final build-report artifact path. */
        std::filesystem::path BuildSummaryFilePath{}; /**< @brief Human-readable summary artifact path. */
        std::filesystem::path PackageOutputRootDirectory{}; /**< @brief Final copied-package output root when package promotion ran. */
        std::filesystem::path PackageDirectoryPath{}; /**< @brief Final copied package-directory path when package promotion ran. */
        std::filesystem::path ArchiveFilePath{}; /**< @brief Final archive file path when archive emission ran. */
        std::vector<std::filesystem::path> StageLogFilePaths{}; /**< @brief Per-stage log artifact paths. */
        std::vector<BuildValidationIssue> ValidationIssues{}; /**< @brief Validation issues recorded for the build. */
        std::vector<BuildNodeExecutionRecord> NodeRecords{}; /**< @brief Ordered node execution records. */
        std::vector<std::string> OutputFiles{}; /**< @brief Materialized output files recorded for automation. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Execution options applied when running one planned build graph.
     */
    struct BuildExecutionOptions
    {
        std::function<void(const BuildEvent&)>
            EventSink{}; /**< @brief Optional callback invoked for every structured build event. */
        std::function<bool()>
            CancellationRequested{}; /**< @brief Optional cooperative cancellation callback checked between nodes. */
        class IBuildNodeExecutor*
            NodeExecutor = nullptr; /**< @brief Optional executor override used instead of the default no-op executor. */
        const BuildExecutionReport* ResumeBaselineReport =
            nullptr; /**< @brief Optional prior report whose successful node outputs may be reused for retry/resume flows. */
        CodeBuildServiceOptions
            CodeBuild{}; /**< @brief Optional CMake/code-build adapter settings for real code-node execution. */
        AssetCookServiceOptions
            AssetCook{}; /**< @brief Optional asset-cook adapter settings for real asset-node execution. */
        PackageOutputOptions
            PackageOutput{}; /**< @brief Optional final output/archive promotion settings for staged packages. */
        EBuildCancellationReason CancellationReasonOnRequest =
            EBuildCancellationReason::UserRequested; /**< @brief Cancellation reason recorded when `CancellationRequested`
                                                         returns `true`. */
        bool ResumeSucceededNodes =
            true; /**< @brief `true` to reuse prior successful node outputs from `ResumeBaselineReport` when possible. */
        bool EnablePersistentNodeCache =
            true; /**< @brief `true` to restore/store persistent node-cache entries under `Intermediate/BuildCache/`. */
        bool WriteHistoryArtifacts =
            true; /**< @brief `true` to write request/plan/report/summary/log artifacts under the history directory. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Extension point for concrete build-node execution backends.
     *
     * The first execution slice uses a default no-op executor so the graph can be
     * executed, logged, and reported before real toolchain adapters are integrated.
     * Later code-build, asset-cook, and packaging adapters should implement this
     * interface directly.
     */
    class SNAPI_GAMEFRAMEWORK_API IBuildNodeExecutor
    {
    public:
        virtual ~IBuildNodeExecutor() = default;

        /**
         * @brief Execute one planned build node.
         * @param Request Frozen request that owns the graph.
         * @param Graph Planned build graph.
         * @param Node Node to execute.
         * @return Node execution result or a structured failure.
         */
        [[nodiscard]] virtual TExpected<BuildNodeExecutionResult> Execute(const ResolvedBuildRequest& Request,
                                                                          const BuildGraph& Graph,
                                                                          const BuildGraphNode& Node) = 0;
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service that executes planned build graphs and writes history/report artifacts.
     *
     * The execution service is the first concrete runtime for the build graph. It
     * validates the request and graph, emits structured events, delegates per-node
     * work to a node executor, and writes the canonical history artifacts:
     * - `BuildRequest.json`
     * - `BuildPlan.json`
     * - `BuildReport.json`
     * - `BuildSummary.md`
     * - per-stage logs under `Logs/`
     * - staged package metadata such as `PackageManifest.json` and `StageFileHashes.json`
     */
    class SNAPI_GAMEFRAMEWORK_API BuildExecutionService final
    {
    public:
        /**
         * @brief Execute one planned build graph.
         * @param Request Frozen request that produced the graph.
         * @param Graph Planned graph to execute.
         * @param Options Optional event sink and executor overrides.
         * @return Final execution report or a structured internal/validation error.
         * @remarks
         * Build failures are reported through the returned report's `Status` field.
         * `std::unexpected` is reserved for invalid input or infrastructure failures
         * such as history artifact write errors.
         */
        [[nodiscard]] static TExpected<BuildExecutionReport> Execute(const ResolvedBuildRequest& Request,
                                                                     const BuildGraph& Graph,
                                                                     const BuildExecutionOptions& Options = {});

        /**
         * @brief Validate one build-execution report.
         * @param Report Execution report to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<BuildValidationIssue> Validate(const BuildExecutionReport& Report);

        /**
         * @brief Serialize one execution report into canonical JSON text.
         * @param Report Execution report to serialize.
         * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
         * @return Canonical JSON text or a structured serialization error.
         */
        [[nodiscard]] static TExpected<std::string> SerializeReport(const BuildExecutionReport& Report,
                                                                    int Indent = 2);

        /**
         * @brief Serialize one execution report into a concise Markdown summary.
         * @param Report Execution report to summarize.
         * @return Human-readable Markdown summary or a structured serialization error.
         */
        [[nodiscard]] static TExpected<std::string> SerializeSummary(const BuildExecutionReport& Report);
    };

} // namespace SnAPI::GameFramework
