#include "CodeBuildServiceAdapter.h"

#include "ProjectBuildGenerationShared.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <ranges>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace SnAPI::GameFramework
{
    namespace
    {

        constexpr std::string_view kDefaultExecutionEnvironment = "host-local";
        constexpr std::string_view kDockerPrefix = "docker://";
        constexpr std::string_view kDefaultRuntimeTarget = "SnAPI.GameFramework.Runtime";
        constexpr std::uint32_t kDefaultParallelJobFallback = 8u;
        constexpr std::uint32_t kDefaultParallelJobCap = 20u;

        /**
         * @brief Create one directory tree when it does not already exist.
         * @param Directory Directory path to create.
         * @return Success or a filesystem error.
         */
        [[nodiscard]] Result EnsureDirectory(const std::filesystem::path& Directory);

        /**
         * @brief Shell-backed default command runner used when callers do not inject one.
         */
        class DefaultCodeBuildCommandRunner final : public ICodeBuildCommandRunner
        {
        public:
            [[nodiscard]] TExpected<int> Execute(const CodeBuildCommand& Command) override
            {
                if (Command.Arguments.empty())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InvalidArgument, "Code build command must contain at least one argument"));
                }

                if (!Command.LogFilePath.empty())
                {
                    if (Result DirectoryResult = EnsureDirectory(Command.LogFilePath.parent_path()); !DirectoryResult)
                    {
                        return std::unexpected(DirectoryResult.error());
                    }
                }

                std::ofstream LogOutput{};
                if (!Command.LogFilePath.empty())
                {
                    LogOutput.open(Command.LogFilePath, std::ios::binary | std::ios::app);
                    if (!LogOutput.is_open())
                    {
                        return std::unexpected(
                            MakeError(EErrorCode::InternalError, "Failed to open code build log file for writing"));
                    }
                }

                std::string CommandLine = BuildShellCommandLine(Command);
                CommandLine += " 2>&1";

                FILE* Pipe = OpenPipe(CommandLine);
                if (Pipe == nullptr)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to launch code build command"));
                }

                std::array<char, 4096> Buffer{};
                while (true)
                {
                    const std::size_t BytesRead = std::fread(Buffer.data(), 1u, Buffer.size(), Pipe);
                    if (BytesRead == 0u)
                    {
                        break;
                    }

                    const std::string_view Chunk{Buffer.data(), BytesRead};
                    if (LogOutput.is_open())
                    {
                        LogOutput.write(Chunk.data(), static_cast<std::streamsize>(Chunk.size()));
                        if (!LogOutput.good())
                        {
                            (void)ClosePipe(Pipe);
                            return std::unexpected(
                                MakeError(EErrorCode::InternalError, "Failed to append code build log output"));
                        }
                    }

                    if (Command.OutputSink)
                    {
                        Command.OutputSink(Chunk);
                    }
                    else
                    {
                        std::fwrite(Chunk.data(), 1u, Chunk.size(), stdout);
                        std::fflush(stdout);
                    }
                }

                if (std::ferror(Pipe) != 0)
                {
                    (void)ClosePipe(Pipe);
                    return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to read code build command output"));
                }

                if (LogOutput.is_open())
                {
                    LogOutput.flush();
                    if (!LogOutput.good())
                    {
                        (void)ClosePipe(Pipe);
                        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to flush code build log output"));
                    }
                }

                const int RawExitCode = ClosePipe(Pipe);
                if (RawExitCode == -1)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to collect code build exit code"));
                }

                return NormalizeExitCode(RawExitCode);
            }

        private:
            /**
             * @brief Build one shell command line from tokenized arguments and working directory metadata.
             * @param Command Prepared command metadata.
             * @return Quoted shell command line.
             */
            [[nodiscard]] static std::string BuildShellCommandLine(const CodeBuildCommand& Command)
            {
                std::string CommandLine{};
                if (!Command.WorkingDirectory.empty())
                {
                    CommandLine += "cd " + QuoteForShell(Command.WorkingDirectory.string()) + " && ";
                }
                CommandLine += JoinArguments(Command.Arguments);
                return CommandLine;
            }

            /**
             * @brief Open one shell pipe for streaming process output.
             * @param CommandLine Fully quoted shell command line.
             * @return Open pipe or `nullptr` on launch failure.
             */
            [[nodiscard]] static FILE* OpenPipe(const std::string& CommandLine)
            {
#if defined(_WIN32)
                return _popen(CommandLine.c_str(), "r");
#else
                return popen(CommandLine.c_str(), "r");
#endif
            }

            /**
             * @brief Close one shell pipe and return the raw process exit code.
             * @param Pipe Open shell pipe returned by `OpenPipe`.
             * @return Raw shell exit code or `-1` on failure.
             */
            [[nodiscard]] static int ClosePipe(FILE* Pipe)
            {
#if defined(_WIN32)
                return _pclose(Pipe);
#else
                return pclose(Pipe);
#endif
            }

            /**
             * @brief Normalize one shell-specific raw exit code into a process exit code.
             * @param RawExitCode Raw exit code returned by the platform shell pipe close operation.
             * @return Normalized process exit code.
             */
            [[nodiscard]] static int NormalizeExitCode(const int RawExitCode)
            {
#if !defined(_WIN32)
                if (WIFEXITED(RawExitCode))
                {
                    return WEXITSTATUS(RawExitCode);
                }
#endif
                return RawExitCode;
            }

            /**
             * @brief Quote one shell token for the host platform.
             * @param Text Raw token text.
             * @return Platform-appropriate quoted token.
             */
            [[nodiscard]] static std::string QuoteForShell(const std::string_view Text)
            {
#if defined(_WIN32)
                std::string Result = "\"";
                for (const char Character : Text)
                {
                    if (Character == '"')
                    {
                        Result += "\\\"";
                    }
                    else
                    {
                        Result.push_back(Character);
                    }
                }
                Result.push_back('"');
                return Result;
#else
                std::string Result = "'";
                for (const char Character : Text)
                {
                    if (Character == '\'')
                    {
                        Result += "'\\''";
                    }
                    else
                    {
                        Result.push_back(Character);
                    }
                }
                Result.push_back('\'');
                return Result;
#endif
            }

            /**
             * @brief Join one tokenized command into a shell command line.
             * @param Arguments Tokenized command arguments.
             * @return Quoted shell command line.
             */
            [[nodiscard]] static std::string JoinArguments(const std::vector<std::string>& Arguments)
            {
                std::ostringstream Stream{};
                for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
                {
                    if (Index > 0u)
                    {
                        Stream << ' ';
                    }
                    Stream << QuoteForShell(Arguments[Index]);
                }
                return Stream.str();
            }
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
         * @brief Normalize one filesystem path for reports and command construction.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
        }

        /**
         * @brief Return `true` when one directory looks like the SnAPI.GameFramework source root.
         * @param Directory Candidate directory to inspect.
         * @return `true` when the directory contains the expected build-root files.
         */
        [[nodiscard]] bool IsGameFrameworkSourceRoot(const std::filesystem::path& Directory)
        {
            if (Directory.empty())
            {
                return false;
            }

            return std::filesystem::exists(Directory / "CMakeLists.txt") &&
                   std::filesystem::exists(Directory / "include" / "GameFramework.hpp") &&
                   std::filesystem::exists(Directory / "src" / "GameFramework.cpp");
        }

        /**
         * @brief Walk one path and its parents until one SnAPI.GameFramework source root is found.
         * @param StartDirectory Starting directory to inspect first.
         * @return Matching source root when found.
         */
        [[nodiscard]] std::optional<std::filesystem::path> FindSourceRootFromAncestors(
            std::filesystem::path StartDirectory)
        {
            if (StartDirectory.empty())
            {
                return std::nullopt;
            }

            StartDirectory = StartDirectory.lexically_normal();
            while (!StartDirectory.empty())
            {
                if (IsGameFrameworkSourceRoot(StartDirectory))
                {
                    return StartDirectory;
                }

                const std::filesystem::path Parent = StartDirectory.parent_path();
                if (Parent == StartDirectory)
                {
                    break;
                }
                StartDirectory = Parent;
            }

            return std::nullopt;
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

        [[nodiscard]] Result EnsureDirectory(const std::filesystem::path& Directory)
        {
            if (Directory.empty())
            {
                return Ok();
            }

            std::error_code Error{};
            std::filesystem::create_directories(Directory, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError,
                              "Failed to create directory '" + Directory.string() + "': " + Error.message()));
            }
            return Ok();
        }

        /**
         * @brief Return the effective engine source directory for code builds.
         * @param Options Adapter options.
         * @return Effective engine source directory or a structured error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveEngineSourceDirectory(
            const CodeBuildServiceOptions& Options)
        {
            if (!Options.EngineSourceDirectory.empty())
            {
                return Options.EngineSourceDirectory.lexically_normal();
            }

#if defined(SNAPI_GF_SOURCE_ROOT_DIR)
            {
                const std::filesystem::path CompiledSourceRoot = std::filesystem::path(SNAPI_GF_SOURCE_ROOT_DIR).lexically_normal();
                if (IsGameFrameworkSourceRoot(CompiledSourceRoot))
                {
                    return CompiledSourceRoot;
                }
            }
#endif

            std::error_code Error{};
            const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to resolve current working directory: " + Error.message()));
            }

            if (auto SourceRoot = FindSourceRootFromAncestors(CurrentPath); SourceRoot.has_value())
            {
                return SourceRoot->lexically_normal();
            }

            return std::unexpected(MakeError(
                EErrorCode::InvalidArgument,
                "Failed to resolve the SnAPI.GameFramework source root. Set CodeBuild.EngineSourceDirectory explicitly."));
        }

        /**
         * @brief Return the effective command runner for one execution.
         * @param Options Adapter options.
         * @param DefaultRunner Internal default command runner fallback.
         * @return Effective command runner.
         */
        [[nodiscard]] ICodeBuildCommandRunner& ResolveCommandRunner(const CodeBuildServiceOptions& Options,
                                                                    DefaultCodeBuildCommandRunner& DefaultRunner)
        {
            return Options.CommandRunner != nullptr ? *Options.CommandRunner : DefaultRunner;
        }

        /**
         * @brief Return the build directory for one configure/build node.
         * @param Node Planned node to inspect.
         * @return Build directory or a structured validation error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveBuildDirectory(const BuildGraphNode& Node)
        {
            if (Node.Type == EBuildNodeType::ConfigureCMake)
            {
                if (Node.Outputs.empty())
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                     "ConfigureCMake nodes require a CMakeCache.txt output path"));
                }
                return std::filesystem::path(Node.Outputs.front()).parent_path().lexically_normal();
            }

            if (Node.Type == EBuildNodeType::BuildCode)
            {
                if (Node.Inputs.empty())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InvalidArgument, "BuildCode nodes require a configured build input path"));
                }
                return std::filesystem::path(Node.Inputs.front()).parent_path().lexically_normal();
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node does not map to a CMake build directory"));
        }

        /**
         * @brief Return the effective build-target list for one code-build invocation.
         * @param Options Adapter options.
         * @return Ordered non-empty build targets.
         */
        [[nodiscard]] std::vector<std::string> ResolveBuildTargets(const CodeBuildServiceOptions& Options)
        {
            if (!Options.BuildTargets.empty())
            {
                return Options.BuildTargets;
            }
            return {std::string(kDefaultRuntimeTarget)};
        }

        /**
         * @brief Resolve one explicit build parallelism value for `cmake --build`.
         * @param Options Adapter options.
         * @return Positive bounded parallel job count.
         *
         * Leaving the argument as bare `--parallel` lets the active generator decide
         * job fan-out, which can become surprisingly aggressive and destabilize the
         * desktop during editor-triggered builds. The adapter therefore always emits
         * an explicit bounded job count unless the caller already supplied one.
         */
        [[nodiscard]] std::uint32_t ResolveParallelJobCount(const CodeBuildServiceOptions& Options)
        {
            if (Options.ParallelJobs > 0u)
            {
                return Options.ParallelJobs;
            }

            const std::uint32_t HardwareConcurrency = std::thread::hardware_concurrency();
            if (HardwareConcurrency == 0u)
            {
                return kDefaultParallelJobFallback;
            }

            return std::clamp(HardwareConcurrency, 1u, kDefaultParallelJobCap);
        }

        /**
         * @brief Wrap one host command line in a Docker invocation.
         * @param BaseCommand Host command to execute inside the container.
         * @param Environment Parsed execution environment.
         * @param Options Adapter options.
         * @param EngineSourceDirectory Mounted engine source directory.
         * @param ProjectRootDirectory Mounted project root directory.
         * @return Docker-wrapped command arguments.
         */
        [[nodiscard]] std::vector<std::string> BuildDockerWrappedArguments(
            const std::vector<std::string>& BaseCommand, const CodeBuildExecutionEnvironment& Environment,
            const CodeBuildServiceOptions& Options, const std::filesystem::path& EngineSourceDirectory,
            const std::filesystem::path& ProjectRootDirectory)
        {
            std::vector<std::string> Arguments{
                Options.DockerExecutable,
                "run",
                "--rm",
            };

            std::set<std::string> UniqueMounts{};
            const auto AddMount = [&](const std::filesystem::path& Directory)
            {
                const std::filesystem::path Normalized = Directory.lexically_normal();
                if (Normalized.empty())
                {
                    return;
                }
                const std::string HostPath = NormalizePathString(Normalized);
                UniqueMounts.insert(HostPath + ":" + HostPath);
            };

            AddMount(EngineSourceDirectory);
            AddMount(ProjectRootDirectory);

            for (const std::string& Mount : UniqueMounts)
            {
                Arguments.push_back("-v");
                Arguments.push_back(Mount);
            }

            Arguments.push_back("-w");
            Arguments.push_back(NormalizePathString(EngineSourceDirectory));
            Arguments.push_back(Environment.DockerImage);
            Arguments.insert(Arguments.end(), BaseCommand.begin(), BaseCommand.end());
            return Arguments;
        }

        /**
         * @brief Copy one regular file into the destination artifact directory.
         * @param SourceFile Source file to copy.
         * @param DestinationDirectory Artifact directory to populate.
         * @return Destination file path or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> CopyArtifact(const std::filesystem::path& SourceFile,
                                                                    const std::filesystem::path& DestinationDirectory)
        {
            if (Result DirectoryResult = EnsureDirectory(DestinationDirectory); !DirectoryResult)
            {
                return std::unexpected(DirectoryResult.error());
            }

            const std::filesystem::path DestinationFile = DestinationDirectory / SourceFile.filename();
            std::error_code Error{};
            std::filesystem::copy_file(SourceFile, DestinationFile, std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError,
                              "Failed to copy built artifact '" + SourceFile.string() + "': " + Error.message()));
            }

            return DestinationFile.lexically_normal();
        }

        /**
         * @brief Copy one regular file into the destination artifact directory under an explicit file name.
         * @param SourceFile Source file to copy.
         * @param DestinationDirectory Artifact directory to populate.
         * @param DestinationFileName Destination file name to author inside the artifact directory.
         * @return Destination file path or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::filesystem::path> CopyArtifactAs(const std::filesystem::path& SourceFile,
                                                                      const std::filesystem::path& DestinationDirectory,
                                                                      const std::string_view DestinationFileName)
        {
            if (TrimCopy(DestinationFileName).empty())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Destination artifact file name must not be empty"));
            }

            if (Result DirectoryResult = EnsureDirectory(DestinationDirectory); !DirectoryResult)
            {
                return std::unexpected(DirectoryResult.error());
            }

            const std::filesystem::path DestinationFile =
                (DestinationDirectory / std::string(DestinationFileName)).lexically_normal();
            std::error_code Error{};
            std::filesystem::copy_file(SourceFile, DestinationFile, std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError,
                              "Failed to copy built artifact '" + SourceFile.string() + "': " + Error.message()));
            }

            return DestinationFile;
        }

        /**
         * @brief Return `true` when one path points at a host-provided system library that should not be bundled.
         * @param LibraryPath Candidate resolved library path from the dynamic loader.
         * @return `true` when the library is under a standard system library root.
         */
        [[nodiscard]] bool IsSystemLibraryPath(const std::filesystem::path& LibraryPath)
        {
#if defined(_WIN32)
            (void)LibraryPath;
            return false;
#else
            const std::string Normalized = LibraryPath.lexically_normal().generic_string();
            return Normalized == "/lib" || Normalized == "/lib64" || Normalized == "/usr/lib" ||
                   Normalized == "/usr/lib64" || Normalized.rfind("/lib/", 0u) == 0u ||
                   Normalized.rfind("/lib64/", 0u) == 0u || Normalized.rfind("/usr/lib/", 0u) == 0u ||
                   Normalized.rfind("/usr/lib64/", 0u) == 0u;
#endif
        }

        /**
         * @brief Return `true` when one file looks like an ELF binary/shared object.
         * @param FilePath Candidate file to inspect.
         * @return `true` when the file begins with the ELF magic bytes.
         */
        [[nodiscard]] bool IsElfBinary(const std::filesystem::path& FilePath)
        {
#if defined(_WIN32)
            (void)FilePath;
            return false;
#else
            std::ifstream Input(FilePath, std::ios::binary);
            if (!Input.is_open())
            {
                return false;
            }

            std::array<unsigned char, 4> Magic{};
            Input.read(reinterpret_cast<char*>(Magic.data()), static_cast<std::streamsize>(Magic.size()));
            return Input.gcount() == static_cast<std::streamsize>(Magic.size()) && Magic[0] == 0x7Fu &&
                   Magic[1] == 'E' && Magic[2] == 'L' && Magic[3] == 'F';
#endif
        }

        /**
         * @brief Quote one shell token for the current host platform.
         * @param Text Raw token text.
         * @return Shell-safe quoted token.
         */
        [[nodiscard]] std::string QuoteForHostShell(const std::string_view Text)
        {
#if defined(_WIN32)
            std::string Result = "\"";
            for (const char Character : Text)
            {
                if (Character == '"')
                {
                    Result += "\\\"";
                }
                else
                {
                    Result.push_back(Character);
                }
            }
            Result.push_back('"');
            return Result;
#else
            std::string Result = "'";
            for (const char Character : Text)
            {
                if (Character == '\'')
                {
                    Result += "'\\''";
                }
                else
                {
                    Result.push_back(Character);
                }
            }
            Result.push_back('\'');
            return Result;
#endif
        }

        /**
         * @brief Join one tokenized command line into a shell string for `popen`.
         * @param Arguments Tokenized command arguments.
         * @return Shell-quoted command line string.
         */
        [[nodiscard]] std::string JoinHostShellArguments(const std::vector<std::string>& Arguments)
        {
            std::ostringstream Stream{};
            for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
            {
                if (Index > 0u)
                {
                    Stream << ' ';
                }
                Stream << QuoteForHostShell(Arguments[Index]);
            }
            return Stream.str();
        }

        /**
         * @brief Execute one small host-local command and capture merged stdout/stderr.
         * @param Arguments Tokenized command arguments.
         * @return Captured output on success or a structured execution error.
         */
        [[nodiscard]] TExpected<std::string> ExecuteHostCommandAndCaptureOutput(const std::vector<std::string>& Arguments)
        {
#if defined(_WIN32)
            (void)Arguments;
            return std::string{};
#else
            if (Arguments.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Captured host command must not be empty"));
            }

            std::string CommandLine = JoinHostShellArguments(Arguments);
            CommandLine += " 2>&1";

            FILE* Pipe = popen(CommandLine.c_str(), "r");
            if (Pipe == nullptr)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to launch captured host command"));
            }

            std::string Output{};
            std::array<char, 4096> Buffer{};
            while (true)
            {
                const std::size_t BytesRead = std::fread(Buffer.data(), 1u, Buffer.size(), Pipe);
                if (BytesRead == 0u)
                {
                    break;
                }
                Output.append(Buffer.data(), BytesRead);
            }

            if (std::ferror(Pipe) != 0)
            {
                (void)pclose(Pipe);
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to read captured host command output"));
            }

            const int RawExitCode = pclose(Pipe);
            if (RawExitCode == -1)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to collect captured host command exit code"));
            }

            int ExitCode = RawExitCode;
            if (WIFEXITED(RawExitCode))
            {
                ExitCode = WEXITSTATUS(RawExitCode);
            }

            if (ExitCode != 0)
            {
                const std::string Message = TrimCopy(Output).empty()
                                                ? ("Host command failed with exit code " + std::to_string(ExitCode))
                                                : TrimCopy(Output);
                return std::unexpected(MakeError(EErrorCode::InternalError, Message));
            }

            return Output;
#endif
        }

        /**
         * @brief One resolved runtime dependency discovered from an ELF binary.
         */
        struct RuntimeDependency
        {
            std::string RequestedName{}; /**< @brief Library name originally requested by the loader. */
            std::filesystem::path ResolvedPath{}; /**< @brief Concrete resolved library file path on disk. */
        };

        /**
         * @brief Discover non-system shared-library dependencies for one ELF binary.
         * @param BinaryPath ELF executable or shared object to inspect.
         * @param Options Code-build adapter options that provide tool names.
         * @return Ordered resolved non-system dependencies.
         */
        [[nodiscard]] TExpected<std::vector<RuntimeDependency>> DiscoverRuntimeDependencies(
            const std::filesystem::path& BinaryPath, const CodeBuildServiceOptions& Options)
        {
#if defined(_WIN32)
            (void)BinaryPath;
            (void)Options;
            return std::vector<RuntimeDependency>{};
#else
            if (!IsElfBinary(BinaryPath))
            {
                return std::vector<RuntimeDependency>{};
            }

            auto Output = ExecuteHostCommandAndCaptureOutput(
                {Options.LddExecutable.empty() ? std::string("ldd") : Options.LddExecutable, BinaryPath.string()});
            if (!Output)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to inspect runtime dependencies for `" +
                                                     BinaryPath.string() + "`: " + Output.error().Message));
            }

            std::vector<RuntimeDependency> Dependencies{};
            std::set<std::string> SeenKeys{};
            std::istringstream Stream(*Output);
            std::string Line{};
            while (std::getline(Stream, Line))
            {
                const std::string Trimmed = TrimCopy(Line);
                if (Trimmed.empty() || Trimmed.rfind("linux-vdso", 0u) == 0u)
                {
                    continue;
                }

                if (Trimmed.find("=> not found") != std::string::npos)
                {
                    return std::unexpected(MakeError(EErrorCode::NotFound,
                                                     "Runtime dependency was not found for `" + BinaryPath.string() +
                                                         "`: " + Trimmed));
                }

                std::string RequestedName{};
                std::string ResolvedText = TrimCopy(Trimmed);
                const std::size_t Arrow = Trimmed.find("=>");
                if (Arrow != std::string::npos)
                {
                    RequestedName = TrimCopy(Trimmed.substr(0u, Arrow));
                    ResolvedText = TrimCopy(Trimmed.substr(Arrow + 2u));
                }

                const std::size_t AddressSuffix = ResolvedText.find(" (");
                if (AddressSuffix != std::string::npos)
                {
                    ResolvedText.erase(AddressSuffix);
                }
                ResolvedText = TrimCopy(ResolvedText);
                if (ResolvedText.empty())
                {
                    continue;
                }

                const std::filesystem::path ResolvedPath = std::filesystem::path(ResolvedText).lexically_normal();
                if (!ResolvedPath.is_absolute() || IsSystemLibraryPath(ResolvedPath))
                {
                    continue;
                }

                if (RequestedName.empty())
                {
                    RequestedName = ResolvedPath.filename().string();
                }

                const std::string Key = RequestedName + "|" + ResolvedPath.generic_string();
                if (!SeenKeys.insert(Key).second)
                {
                    continue;
                }

                Dependencies.push_back(RuntimeDependency{
                    .RequestedName = RequestedName,
                    .ResolvedPath = ResolvedPath,
                });
            }

            return Dependencies;
#endif
        }

        /**
         * @brief Rewrite one copied ELF binary to resolve sibling libraries from its own directory.
         * @param FilePath Copied ELF binary/shared object inside the artifact directory.
         * @param Options Code-build adapter options that provide tool names.
         * @return Success or a structured patching error.
         */
        [[nodiscard]] Result NormalizeRuntimeSearchPath(const std::filesystem::path& FilePath,
                                                        const CodeBuildServiceOptions& Options)
        {
#if defined(_WIN32)
            (void)FilePath;
            (void)Options;
            return Ok();
#else
            if (!IsElfBinary(FilePath) || !Options.NormalizeRuntimeSearchPaths)
            {
                return Ok();
            }

            auto Output = ExecuteHostCommandAndCaptureOutput({Options.PatchelfExecutable.empty() ? std::string("patchelf")
                                                                                                : Options.PatchelfExecutable,
                                                              "--set-rpath",
                                                              "$ORIGIN",
                                                              FilePath.string()});
            if (!Output)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to normalize packaged runtime search path for `" +
                                                     FilePath.string() + "`: " + Output.error().Message));
            }

            return Ok();
#endif
        }

        /**
         * @brief Discover built runtime artifacts for the requested CMake targets.
         * @param BuildDirectory Configured CMake build directory.
         * @param TargetNames Requested CMake target names.
         * @return Ordered unique built artifact file paths.
         */
        [[nodiscard]] TExpected<std::vector<std::filesystem::path>> DiscoverBuiltArtifacts(
            const std::filesystem::path& BuildDirectory, const std::vector<std::string>& TargetNames)
        {
            std::error_code Error{};
            if (!std::filesystem::exists(BuildDirectory, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Configured build directory does not exist for artifact discovery"));
            }

            std::set<std::filesystem::path> Matches{};
            for (const auto& Entry : std::filesystem::recursive_directory_iterator(BuildDirectory, Error))
            {
                if (Error)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError,
                                                     "Failed to enumerate built artifacts: " + Error.message()));
                }
                if (!Entry.is_regular_file())
                {
                    continue;
                }

                const std::string FileName = Entry.path().filename().string();
                const bool MatchesTarget = std::ranges::any_of(TargetNames, [&](const std::string& TargetName)
                                                               { return FileName == TargetName || FileName == (TargetName + ".exe"); });
                if (MatchesTarget)
                {
                    Matches.insert(Entry.path().lexically_normal());
                }
            }

            if (Matches.empty())
            {
                return std::unexpected(MakeError(EErrorCode::NotFound,
                                                 "CMake build completed but no runtime artifacts were discovered"));
            }

            return std::vector<std::filesystem::path>(Matches.begin(), Matches.end());
        }

    } // namespace

    TExpected<CodeBuildExecutionEnvironment> CodeBuildServiceAdapter::ParseExecutionEnvironment(
        const std::string_view ExecutionEnvironment)
    {
        const std::string Value = TrimCopy(ExecutionEnvironment);
        if (Value.empty() || Value == kDefaultExecutionEnvironment)
        {
            return CodeBuildExecutionEnvironment{
                .Kind = ECodeBuildExecutionEnvironmentKind::HostLocal,
                .RawValue = Value.empty() ? std::string(kDefaultExecutionEnvironment) : Value,
            };
        }

        if (Value.rfind(kDockerPrefix, 0u) == 0u)
        {
            const std::string Image = TrimCopy(Value.substr(kDockerPrefix.size()));
            if (Image.empty())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Docker execution environments require a non-empty image name"));
            }

            return CodeBuildExecutionEnvironment{
                .Kind = ECodeBuildExecutionEnvironmentKind::DockerContainer,
                .RawValue = Value,
                .DockerImage = Image,
            };
        }

        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Unsupported code-build execution environment: " + Value));
    }

    TExpected<CodeBuildCommand> CodeBuildServiceAdapter::CreateConfigureCommand(
        const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
        const std::filesystem::path& LogFilePath)
    {
        if (Node.Type != EBuildNodeType::ConfigureCMake)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Configure command creation requires a ConfigureCMake node"));
        }

        auto EngineSourceDirectory = ResolveEngineSourceDirectory(Options);
        auto BuildDirectory = ResolveBuildDirectory(Node);
        auto Environment = ParseExecutionEnvironment(Request.Profile.ExecutionEnvironment);
        if (!EngineSourceDirectory)
        {
            return std::unexpected(EngineSourceDirectory.error());
        }
        if (!BuildDirectory)
        {
            return std::unexpected(BuildDirectory.error());
        }
        if (!Environment)
        {
            return std::unexpected(Environment.error());
        }

        std::vector<std::string> Arguments{
            Options.CMakeExecutable,
            "-S",
            NormalizePathString(*EngineSourceDirectory),
            "-B",
            NormalizePathString(*BuildDirectory),
            "-DSNAPI_PROJECT_ROOT_DIR=" + NormalizePathString(Request.Project.ProjectRootDirectory),
            "-DSNAPI_GF_BUILD_TESTS=OFF",
            "-DSNAPI_GF_BUILD_EXAMPLES=OFF",
            "-DSNAPI_GF_BUILD_DOCS=OFF",
            "-DCMAKE_BUILD_TYPE=" + ToString(Request.Profile.Configuration),
        };
        if (!Options.Generator.empty())
        {
            Arguments.push_back("-G");
            Arguments.push_back(Options.Generator);
        }
        Arguments.insert(Arguments.end(), Options.ConfigureArguments.begin(), Options.ConfigureArguments.end());

        if (Environment->Kind == ECodeBuildExecutionEnvironmentKind::DockerContainer)
        {
            Arguments = BuildDockerWrappedArguments(Arguments, *Environment, Options, *EngineSourceDirectory,
                                                   Request.Project.ProjectRootDirectory);
        }

        return CodeBuildCommand{
            .WorkingDirectory = *EngineSourceDirectory,
            .Arguments = std::move(Arguments),
            .LogFilePath = LogFilePath,
            .OutputSink = Options.OutputSink,
        };
    }

    TExpected<CodeBuildCommand> CodeBuildServiceAdapter::CreateBuildCommand(
        const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
        const std::filesystem::path& LogFilePath)
    {
        if (Node.Type != EBuildNodeType::BuildCode)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Build command creation requires a BuildCode node"));
        }

        auto EngineSourceDirectory = ResolveEngineSourceDirectory(Options);
        auto BuildDirectory = ResolveBuildDirectory(Node);
        auto Environment = ParseExecutionEnvironment(Request.Profile.ExecutionEnvironment);
        if (!EngineSourceDirectory)
        {
            return std::unexpected(EngineSourceDirectory.error());
        }
        if (!BuildDirectory)
        {
            return std::unexpected(BuildDirectory.error());
        }
        if (!Environment)
        {
            return std::unexpected(Environment.error());
        }

        std::vector<std::string> Arguments{
            Options.CMakeExecutable,
            "--build",
            NormalizePathString(*BuildDirectory),
            "--config",
            ToString(Request.Profile.Configuration),
            "--parallel",
            std::to_string(ResolveParallelJobCount(Options)),
        };

        const std::vector<std::string> Targets = ResolveBuildTargets(Options);
        if (!Targets.empty())
        {
            Arguments.push_back("--target");
            Arguments.insert(Arguments.end(), Targets.begin(), Targets.end());
        }
        Arguments.insert(Arguments.end(), Options.BuildArguments.begin(), Options.BuildArguments.end());

        if (Environment->Kind == ECodeBuildExecutionEnvironmentKind::DockerContainer)
        {
            Arguments = BuildDockerWrappedArguments(Arguments, *Environment, Options, *EngineSourceDirectory,
                                                   Request.Project.ProjectRootDirectory);
        }

        return CodeBuildCommand{
            .WorkingDirectory = *EngineSourceDirectory,
            .Arguments = std::move(Arguments),
            .LogFilePath = LogFilePath,
            .OutputSink = Options.OutputSink,
        };
    }

    TExpected<CodeBuildNodeResult> CodeBuildServiceAdapter::ExecuteGenerateProjectBuildFiles(
        const ResolvedProjectDescriptor& Project)
    {
        const Detail::ProjectBuildIntegrationLayout Layout =
            Detail::BuildProjectBuildIntegrationLayout(Project.Descriptor, Project.ProjectRootDirectory);
        std::vector<std::filesystem::path> GeneratedFiles{};
        if (Result WriteResult =
                Detail::WriteProjectBuildIntegrationFiles(Project.Descriptor, Layout, &GeneratedFiles);
            !WriteResult)
        {
            return std::unexpected(WriteResult.error());
        }

        std::vector<std::string> Outputs{};
        Outputs.reserve(GeneratedFiles.size());
        for (const std::filesystem::path& GeneratedFile : GeneratedFiles)
        {
            Outputs.push_back(NormalizePathString(GeneratedFile));
        }

        return CodeBuildNodeResult{
            .Message = "Regenerated project build integration files.",
            .Outputs = std::move(Outputs),
        };
    }

    TExpected<CodeBuildNodeResult> CodeBuildServiceAdapter::ExecuteConfigureCMake(
        const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
        const std::filesystem::path& LogFilePath)
    {
        auto Command = CreateConfigureCommand(Request, Node, Options, LogFilePath);
        if (!Command)
        {
            return std::unexpected(Command.error());
        }

        DefaultCodeBuildCommandRunner DefaultRunner{};
        ICodeBuildCommandRunner& Runner = ResolveCommandRunner(Options, DefaultRunner);
        auto ExitCode = Runner.Execute(*Command);
        if (!ExitCode)
        {
            return std::unexpected(ExitCode.error());
        }
        if (*ExitCode != 0)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "CMake configure failed with exit code " + std::to_string(*ExitCode)));
        }

        return CodeBuildNodeResult{
            .Message = "Configured CMake build tree.",
            .Outputs = Node.Outputs,
        };
    }

    TExpected<CodeBuildNodeResult> CodeBuildServiceAdapter::ExecuteBuildCode(
        const ResolvedBuildRequest& Request, const BuildGraphNode& Node, const CodeBuildServiceOptions& Options,
        const std::filesystem::path& LogFilePath)
    {
        auto BuildDirectory = ResolveBuildDirectory(Node);
        if (!BuildDirectory)
        {
            return std::unexpected(BuildDirectory.error());
        }

        auto Command = CreateBuildCommand(Request, Node, Options, LogFilePath);
        if (!Command)
        {
            return std::unexpected(Command.error());
        }

        DefaultCodeBuildCommandRunner DefaultRunner{};
        ICodeBuildCommandRunner& Runner = ResolveCommandRunner(Options, DefaultRunner);
        auto ExitCode = Runner.Execute(*Command);
        if (!ExitCode)
        {
            return std::unexpected(ExitCode.error());
        }
        if (*ExitCode != 0)
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError, "CMake build failed with exit code " + std::to_string(*ExitCode)));
        }

        if (Node.Outputs.empty())
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "BuildCode nodes require an artifact output directory"));
        }

        const std::filesystem::path ArtifactDirectory = std::filesystem::path(Node.Outputs.front()).lexically_normal();
        const std::vector<std::string> Targets = ResolveBuildTargets(Options);
        auto BuiltArtifacts = DiscoverBuiltArtifacts(*BuildDirectory, Targets);
        if (!BuiltArtifacts)
        {
            return std::unexpected(BuiltArtifacts.error());
        }

        std::vector<std::string> Outputs{};
        std::vector<std::filesystem::path> FilesToPatch{};
        std::set<std::filesystem::path> SeenSourceArtifacts{};
        std::set<std::filesystem::path> SeenDestinationArtifacts{};
        std::vector<std::filesystem::path> DependencyScanQueue = *BuiltArtifacts;
        Outputs.reserve(BuiltArtifacts->size());
        for (const std::filesystem::path& Artifact : *BuiltArtifacts)
        {
            SeenSourceArtifacts.insert(Artifact.lexically_normal());
            auto CopiedArtifact = CopyArtifact(Artifact, ArtifactDirectory);
            if (!CopiedArtifact)
            {
                return std::unexpected(CopiedArtifact.error());
            }
            if (SeenDestinationArtifacts.insert(*CopiedArtifact).second)
            {
                Outputs.push_back(NormalizePathString(*CopiedArtifact));
                FilesToPatch.push_back(*CopiedArtifact);
            }
        }

        if (Options.CopyRuntimeDependencies)
        {
            for (std::size_t Index = 0; Index < DependencyScanQueue.size(); ++Index)
            {
                const std::filesystem::path SourceArtifact = DependencyScanQueue[Index].lexically_normal();
                auto Dependencies = DiscoverRuntimeDependencies(SourceArtifact, Options);
                if (!Dependencies)
                {
                    return std::unexpected(Dependencies.error());
                }

                for (const RuntimeDependency& Dependency : *Dependencies)
                {
                    if (SeenSourceArtifacts.insert(Dependency.ResolvedPath).second)
                    {
                        DependencyScanQueue.push_back(Dependency.ResolvedPath);
                    }

                    auto CopiedDependency = CopyArtifact(Dependency.ResolvedPath, ArtifactDirectory);
                    if (!CopiedDependency)
                    {
                        return std::unexpected(CopiedDependency.error());
                    }

                    if (SeenDestinationArtifacts.insert(*CopiedDependency).second)
                    {
                        Outputs.push_back(NormalizePathString(*CopiedDependency));
                        FilesToPatch.push_back(*CopiedDependency);
                    }

                    const std::string RequestedName = TrimCopy(Dependency.RequestedName);
                    if (!RequestedName.empty() && RequestedName != Dependency.ResolvedPath.filename().string())
                    {
                        auto AliasArtifact = CopyArtifactAs(Dependency.ResolvedPath, ArtifactDirectory, RequestedName);
                        if (!AliasArtifact)
                        {
                            return std::unexpected(AliasArtifact.error());
                        }

                        if (SeenDestinationArtifacts.insert(*AliasArtifact).second)
                        {
                            Outputs.push_back(NormalizePathString(*AliasArtifact));
                            FilesToPatch.push_back(*AliasArtifact);
                        }
                    }
                }
            }
        }

        for (const std::filesystem::path& CopiedArtifact : FilesToPatch)
        {
            if (Result NormalizeResult = NormalizeRuntimeSearchPath(CopiedArtifact, Options); !NormalizeResult)
            {
                return std::unexpected(NormalizeResult.error());
            }
        }

        return CodeBuildNodeResult{
            .Message = "Built runtime code and collected runtime artifacts.",
            .Outputs = std::move(Outputs),
        };
    }

} // namespace SnAPI::GameFramework
