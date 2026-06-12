#pragma once

#include "BuildExecution.h"
#include "BuildPlanner.h"
#include "BuildRequest.h"
#include "Expected.h"
#include "Export.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Process-style exit codes returned by the shared build CLI service.
     *
     * The CLI keeps a stable exit-code contract so the same service can back the
     * standalone executable, local automation, and future editor-triggered command
     * surfaces without each caller inventing different success/failure semantics.
     */
    enum class EBuildCliExitCode : int
    {
        Success = 0, /**< @brief Command completed successfully. */
        InvalidArguments = 2, /**< @brief Command-line parsing failed before work could begin. */
        ValidationFailed = 3, /**< @brief Descriptor, profile, request, or plan validation failed. */
        BuildFailed = 4, /**< @brief Build execution completed but the build itself failed. */
        InternalError = 5, /**< @brief Unexpected infrastructure or internal-service failure. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Aggregate result returned by one parsed build CLI invocation.
     *
     * The service returns captured stdout/stderr text instead of writing directly
     * to process streams so tests, the standalone executable, and future editor
     * integrations can all decide how to surface the command result.
     */
    struct BuildCliResult
    {
        EBuildCliExitCode ExitCode = EBuildCliExitCode::Success; /**< @brief Process-style exit code for the command. */
        std::string StandardOutput{}; /**< @brief Human-readable stdout text for the command. */
        std::string StandardError{}; /**< @brief Human-readable stderr text for the command. */
        std::optional<BuildGraph> PlannedGraph{}; /**< @brief Planned graph when the command produced one. */
        std::optional<BuildExecutionReport> ExecutionReport{}; /**< @brief Final build report when package execution ran. */
        std::vector<std::filesystem::path> ArtifactPaths{}; /**< @brief Created or resolved artifact paths produced by the command. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Execution defaults applied when the shared build CLI resolves one command.
     *
     * Callers can use these options to inject deterministic planner ids for tests,
     * override execution adapters, or control the working directory without needing
     * to fork the CLI parsing logic itself.
     */
    struct BuildCliOptions
    {
        std::filesystem::path CurrentWorkingDirectory{}; /**< @brief Working directory used to resolve relative CLI paths. */
        std::size_t MaxProfileInheritanceDepth = 4u; /**< @brief Maximum allowed build-profile inheritance depth. */
        BuildPlannerOptions Planner{}; /**< @brief Base planner overrides applied to package/validate commands. */
        BuildExecutionOptions Execution{}; /**< @brief Base execution options applied to package commands. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared CLI parser and dispatcher for project creation, module creation, validation, and packaging.
     *
     * The CLI service is intentionally thin over the existing backend services. It
     * parses commands, translates them into typed request models, and then delegates
     * to the same project/build services used by tests and future editor surfaces.
     */
    class SNAPI_GAMEFRAMEWORK_API BuildCliService final
    {
    public:
        /**
         * @brief Return help text for the shared build CLI surface.
         * @param ExecutableName User-facing executable name shown in usage examples.
         * @return Multi-line usage text.
         */
        [[nodiscard]] static std::string Usage(std::string_view ExecutableName = "snapi");

        /**
         * @brief Parse and execute one build CLI invocation.
         * @param Arguments Command-line arguments excluding the process name.
         * @param Options Base working-directory, planner, and execution settings.
         * @return Captured command result including exit code and text output.
         */
        [[nodiscard]] static BuildCliResult Run(const std::vector<std::string>& Arguments,
                                                const BuildCliOptions& Options = {});
    };

} // namespace SnAPI::GameFramework
