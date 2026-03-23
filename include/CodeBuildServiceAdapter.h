#pragma once

#include "BuildPlanner.h"
#include "BuildRequest.h"
#include "Expected.h"
#include "Export.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Execution-environment kind used by the code-build adapter.
     */
    enum class ECodeBuildExecutionEnvironmentKind : std::uint8_t
    {
        HostLocal = 0, /**< @brief Run CMake directly on the current host. */
        DockerContainer, /**< @brief Run CMake inside one Docker/OCI container image. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Parsed execution-environment description used by code-build command generation.
     */
    struct CodeBuildExecutionEnvironment
    {
        ECodeBuildExecutionEnvironmentKind Kind =
            ECodeBuildExecutionEnvironmentKind::HostLocal; /**< @brief Parsed environment kind. */
        std::string RawValue{}; /**< @brief Original authored execution-environment string. */
        std::string DockerImage{}; /**< @brief Docker image name when `Kind` is `DockerContainer`. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One external code-build command prepared by `CodeBuildServiceAdapter`.
     *
     * Commands stay tokenized so callers can log or inspect them without needing
     * to reverse-shell-parse a flattened string. The concrete command runner owns
     * how those tokens are executed.
     */
    struct CodeBuildCommand
    {
        std::filesystem::path WorkingDirectory{}; /**< @brief Host working directory used to invoke the command. */
        std::vector<std::string> Arguments{}; /**< @brief Tokenized command-line arguments, including the executable. */
        std::filesystem::path LogFilePath{}; /**< @brief Optional log-file redirection target. */
        std::function<void(std::string_view)> OutputSink{}; /**< @brief Optional sink that receives streamed command output chunks. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Lightweight result returned by code-build node execution.
     */
    struct CodeBuildNodeResult
    {
        std::string Message{}; /**< @brief Concise node-level summary. */
        std::vector<std::string> Outputs{}; /**< @brief Materialized output file paths. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Command-runner interface used by the code-build adapter.
     *
     * Production code can use the default shell-backed runner, while tests can
     * inject deterministic fakes that record commands and synthesize outputs.
     */
    class SNAPI_GAMEFRAMEWORK_API ICodeBuildCommandRunner
    {
    public:
        virtual ~ICodeBuildCommandRunner() = default;

        /**
         * @brief Execute one prepared external code-build command.
         * @param Command Tokenized command plus working-directory/log-path metadata.
         * @return Process exit code on success or a structured infrastructure error.
         */
        [[nodiscard]] virtual TExpected<int> Execute(const CodeBuildCommand& Command) = 0;
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Options that control how the code-build adapter drives CMake.
     */
    struct CodeBuildServiceOptions
    {
        bool Enabled = false; /**< @brief `true` to replace placeholder code-node execution with real CMake orchestration. */
        std::filesystem::path
            EngineSourceDirectory{}; /**< @brief Engine/GameFramework CMake source root. Defaults to the compiled GameFramework source root. */
        std::string CMakeExecutable = "cmake"; /**< @brief CMake executable used for configure/build commands. */
        std::string DockerExecutable = "docker"; /**< @brief Docker executable used for containerized builds. */
        std::string LddExecutable = "ldd"; /**< @brief Linux shared-library dependency discovery executable used when collecting packaged runtime dependencies. */
        std::string PatchelfExecutable = "patchelf"; /**< @brief Linux runtime-search-path patching executable used when normalizing staged ELF binaries. */
        std::string Generator{}; /**< @brief Optional CMake generator name such as `Ninja`. */
        std::uint32_t ParallelJobs = 0u; /**< @brief Explicit `cmake --build --parallel <N>` job count. `0` resolves to a bounded host default. */
        bool CopyRuntimeDependencies = true; /**< @brief `true` to copy non-system shared-library dependencies into the artifact directory for packaging. */
        bool NormalizeRuntimeSearchPaths = true; /**< @brief `true` to rewrite copied ELF runtime search paths so packaged binaries resolve dependencies from their own directory. */
        std::vector<std::string> ConfigureArguments{}; /**< @brief Extra arguments appended to `cmake -S -B ...`. */
        std::vector<std::string> BuildArguments{}; /**< @brief Extra arguments appended to `cmake --build ...`. */
        std::vector<std::string>
            BuildTargets{}; /**< @brief Explicit CMake targets to build. Defaults to `SnAPI.GameFramework.Runtime`. */
        std::function<void(std::string_view)>
            OutputSink{}; /**< @brief Optional sink that receives streamed command output during configure/build execution. */
        ICodeBuildCommandRunner*
            CommandRunner = nullptr; /**< @brief Optional command-runner override used instead of the default shell runner. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Adapter that turns frozen build requests into CMake configure/build work.
     *
     * This adapter is the first real bridge from the build-system model layer into
     * the native toolchain. It owns:
     * - project build-file regeneration from the descriptor model
     * - host-local and Docker-backed CMake command generation
     * - external command execution through an injectable runner
     * - runtime-artifact discovery and copying into build-history artifact roots
     */
    class SNAPI_GAMEFRAMEWORK_API CodeBuildServiceAdapter final
    {
    public:
        /**
         * @brief Parse one authored execution-environment string.
         * @param ExecutionEnvironment Authored build-profile execution-environment value.
         * @return Parsed execution environment or a structured validation error.
         */
        [[nodiscard]] static TExpected<CodeBuildExecutionEnvironment> ParseExecutionEnvironment(
            std::string_view ExecutionEnvironment);

        /**
         * @brief Create the `cmake -S -B ...` command for one configure node.
         * @param Request Frozen build request.
         * @param Node Planned configure node.
         * @param Options Code-build adapter options.
         * @param LogFilePath Optional command log file.
         * @return Tokenized configure command or a structured validation error.
         */
        [[nodiscard]] static TExpected<CodeBuildCommand> CreateConfigureCommand(
            const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
            const std::filesystem::path& LogFilePath = {});

        /**
         * @brief Create the `cmake --build ...` command for one build node.
         * @param Request Frozen build request.
         * @param Node Planned build node.
         * @param Options Code-build adapter options.
         * @param LogFilePath Optional command log file.
         * @return Tokenized build command or a structured validation error.
         */
        [[nodiscard]] static TExpected<CodeBuildCommand> CreateBuildCommand(
            const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
            const std::filesystem::path& LogFilePath = {});

        /**
         * @brief Regenerate project-level build bridge files from the descriptor model.
         * @param Project Resolved project descriptor snapshot.
         * @return Written build-integration artifacts or a structured filesystem error.
         */
        [[nodiscard]] static TExpected<CodeBuildNodeResult> ExecuteGenerateProjectBuildFiles(
            const ResolvedProjectDescriptor& Project);

        /**
         * @brief Execute one planned CMake configure node.
         * @param Request Frozen build request.
         * @param Node Planned configure node.
         * @param Options Code-build adapter options.
         * @param LogFilePath Optional command log file.
         * @return Configure-node result or a structured infrastructure/build error.
         */
        [[nodiscard]] static TExpected<CodeBuildNodeResult> ExecuteConfigureCMake(
            const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
            const std::filesystem::path& LogFilePath = {});

        /**
         * @brief Execute one planned CMake build node and collect runtime artifacts.
         * @param Request Frozen build request.
         * @param Node Planned build node.
         * @param Options Code-build adapter options.
         * @param LogFilePath Optional command log file.
         * @return Build-node result or a structured infrastructure/build error.
         */
        [[nodiscard]] static TExpected<CodeBuildNodeResult> ExecuteBuildCode(
            const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
            const std::filesystem::path& LogFilePath = {});
    };

} // namespace SnAPI::GameFramework
