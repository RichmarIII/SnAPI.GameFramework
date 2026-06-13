#pragma once

#include "BuildExecution.h"
#include "Expected.h"
#include "Export.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief High-level state of one discovered build-history entry.
     */
    enum class EBuildHistoryEntryState : std::uint8_t
    {
        Complete = 0, /**< @brief History directory contains a readable build report. */
        Incomplete, /**< @brief History directory exists but is missing or cannot load the final report. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One discovered build-history entry under a project's `Saved/BuildHistory/` tree.
     *
     * The entry is intentionally lightweight so editor UI and CLI surfaces can list
     * prior runs without loading every artifact eagerly. When `State` is `Complete`,
     * the timing, status, and stage fields are sourced from the parsed build report.
     * When `State` is `Incomplete`, the entry still records the known artifact paths
     * so failed or interrupted runs remain inspectable.
     */
    struct BuildHistoryEntry
    {
        std::string BuildId{}; /**< @brief Build invocation id, usually the history-directory name. */
        EBuildHistoryEntryState State =
            EBuildHistoryEntryState::Incomplete; /**< @brief Whether the final report was successfully loaded. */
        std::string RequestHash{}; /**< @brief Frozen request hash when the final report was available. */
        EBuildExecutionStatus Status =
            EBuildExecutionStatus::Cancelled; /**< @brief Terminal build status when the final report was available. */
        std::string StartedAtUtc{}; /**< @brief ISO-8601 UTC build start timestamp when available. */
        std::string FinishedAtUtc{}; /**< @brief ISO-8601 UTC build finish timestamp when available. */
        std::uint64_t NodeCount = 0u; /**< @brief Number of recorded node executions when available. */
        std::uint64_t OutputFileCount = 0u; /**< @brief Number of recorded output files when available. */
        std::filesystem::path HistoryDirectory{}; /**< @brief Root history directory for the build id. */
        std::filesystem::path StageDirectory{}; /**< @brief Planned stage directory when the final report was available. */
        std::filesystem::path BuildRequestFilePath{}; /**< @brief Frozen request artifact path. */
        std::filesystem::path BuildPlanFilePath{}; /**< @brief Build-plan artifact path. */
        std::filesystem::path BuildReportFilePath{}; /**< @brief Build-report artifact path. */
        std::filesystem::path BuildSummaryFilePath{}; /**< @brief Human-readable build-summary artifact path. */
        std::string DiagnosticMessage{}; /**< @brief Optional diagnostic when the entry is incomplete or partially unreadable. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Options that control one build-history listing query.
     */
    struct BuildHistoryListOptions
    {
        std::size_t MaxEntries = 0u; /**< @brief Maximum number of entries to return, or `0` for all entries. */
        bool IncludeIncomplete =
            true; /**< @brief `true` to include history directories missing a final readable build report. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One node-level difference discovered while comparing two build reports.
     */
    struct BuildHistoryNodeDelta
    {
        std::uint32_t NodeId = 0u; /**< @brief Node id shared by the compared records when present. */
        std::string Name{}; /**< @brief Human-readable node name from either side. */
        EBuildNodeType Type = EBuildNodeType::LoadProject; /**< @brief Node type from either side. */
        bool LeftPresent = false; /**< @brief `true` when the node existed in the left report. */
        bool RightPresent = false; /**< @brief `true` when the node existed in the right report. */
        EBuildNodeExecutionStatus LeftStatus =
            EBuildNodeExecutionStatus::Succeeded; /**< @brief Left-side node status when present. */
        EBuildNodeExecutionStatus RightStatus =
            EBuildNodeExecutionStatus::Succeeded; /**< @brief Right-side node status when present. */
        bool LeftCacheHit = false; /**< @brief Left-side cache-hit flag when present. */
        bool RightCacheHit = false; /**< @brief Right-side cache-hit flag when present. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief High-level difference summary between two executed build reports.
     *
     * The comparison currently focuses on the signal most useful for build-history
     * surfaces: request-hash drift, terminal status drift, output-file set changes,
     * and per-node status/cache-hit changes.
     */
    struct BuildHistoryComparison
    {
        std::string LeftBuildId{}; /**< @brief Left-side build id. */
        std::string RightBuildId{}; /**< @brief Right-side build id. */
        bool SameRequestHash = false; /**< @brief `true` when both reports were produced from the same frozen request. */
        bool SameStatus = false; /**< @brief `true` when both reports finished with the same terminal status. */
        std::vector<std::string> AddedOutputFiles{}; /**< @brief Output files present only in the right report. */
        std::vector<std::string> RemovedOutputFiles{}; /**< @brief Output files present only in the left report. */
        std::vector<BuildHistoryNodeDelta> NodeDeltas{}; /**< @brief Node-level status/cache-hit deltas. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service that lists, loads, and compares build-history artifacts.
     *
     * `BuildHistoryService` is the first read-side API over the history artifacts
     * emitted by `BuildExecutionService`. It allows editor UI, CLI automation, and
     * tests to:
     * - enumerate prior runs under `Saved/BuildHistory/`
     * - load canonical `BuildReport.json` artifacts
     * - compare two reports by outputs and node outcomes
     */
    class SNAPI_GAMEFRAMEWORK_API BuildHistoryService final
    {
    public:
        /**
         * @brief List prior build-history entries for one project saved root.
         * @param SavedRootDirectory Project `Saved/` directory that owns `BuildHistory/`.
         * @param Options Query controls for incomplete entries and result limits.
         * @return Ordered build-history entries or a structured filesystem error.
         */
        [[nodiscard]] static TExpected<std::vector<BuildHistoryEntry>> List(
            const std::filesystem::path& SavedRootDirectory, const BuildHistoryListOptions& Options = {});

        /**
         * @brief Load one canonical build report from disk.
         * @param BuildReportFilePath Path to `BuildReport.json`.
         * @return Parsed execution report or a structured parse/validation error.
         */
        [[nodiscard]] static TExpected<BuildExecutionReport> LoadReport(
            const std::filesystem::path& BuildReportFilePath);

        /**
         * @brief Validate one lightweight build-history entry.
         * @param Entry Build-history entry to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<BuildValidationIssue> Validate(const BuildHistoryEntry& Entry);

        /**
         * @brief Compare two executed build reports.
         * @param Left Baseline report.
         * @param Right Candidate report.
         * @return High-level comparison summary.
         */
        [[nodiscard]] static BuildHistoryComparison Compare(const BuildExecutionReport& Left,
                                                            const BuildExecutionReport& Right);
    };

} // namespace SnAPI::GameFramework
