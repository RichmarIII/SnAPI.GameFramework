#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace
{

    /**
     * @brief Temporary per-test directory that is deleted on scope exit.
     */
    struct TempDir
    {
        std::filesystem::path Path{};

        TempDir()
        {
            const auto Stamp = std::to_string(
                static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_code_build_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Ec{};
            std::filesystem::remove_all(Path, Ec);
        }
    };

    /**
     * @brief Scoped current-working-directory override used by code-build adapter tests.
     */
    struct ScopedCurrentPath
    {
        std::filesystem::path PreviousPath{};

        explicit ScopedCurrentPath(const std::filesystem::path& NewPath)
        {
            PreviousPath = std::filesystem::current_path();
            std::filesystem::current_path(NewPath);
        }

        ~ScopedCurrentPath()
        {
            std::error_code Ec{};
            std::filesystem::current_path(PreviousPath, Ec);
        }
    };

    /**
     * @brief Build one authored scalar profile patch with a concrete value.
     * @tparam TValue Value type.
     * @param Value Concrete authored value.
     * @return Authored patch.
     */
    template <typename TValue>
    [[nodiscard]] BuildProfileValue<TValue> SetValue(TValue Value)
    {
        return BuildProfileValue<TValue>{
            .IsSet = true,
            .Value = std::move(Value),
        };
    }

    /**
     * @brief Create one project descriptor on disk for code-build adapter tests.
     * @param Root Temporary parent directory.
     * @param ProjectName Stable project name.
     * @param Profiles Authored build profiles to store in the descriptor.
     * @return Path to the written project descriptor.
     */
    [[nodiscard]] std::filesystem::path CreateProject(const std::filesystem::path& Root,
                                                      const std::string_view ProjectName,
                                                      std::vector<BuildProfile> Profiles = {})
    {
        const std::filesystem::path ProjectRoot = Root / std::string(ProjectName);
        const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

        ProjectDescriptor Descriptor{};
        Descriptor.Project.Name = std::string(ProjectName);
        Descriptor.Project.DisplayName = std::string(ProjectName) + " Display";
        Descriptor.Project.ProjectId = std::string(ProjectName) + "-id";
        Descriptor.Startup.StartupLevelAsset = "Levels/Main.level";
        Descriptor.Startup.DefaultGameClass = std::string(ProjectName) + "::Game";
        Descriptor.Startup.DefaultGameModeClass = std::string(ProjectName) + "::GameMode";
        Descriptor.Profiles = std::move(Profiles);

        const Result SaveResult = ProjectDescriptorService::Save(Descriptor, ProjectFilePath.string());
        if (!SaveResult)
        {
            throw std::runtime_error(SaveResult.error().Message);
        }
        return ProjectFilePath;
    }

    /**
     * @brief Resolve one build request plus build plan for adapter tests.
     * @param ProjectFile Project descriptor file path.
     * @param ProfileName Authored build-profile name.
     * @param BuildId Stable planned build id.
     * @return Pair of resolved request and build plan.
     */
    [[nodiscard]] std::pair<ResolvedBuildRequest, BuildGraph> ResolveRequestAndPlan(
        const std::filesystem::path& ProjectFile, const std::string_view ProfileName, const std::string_view BuildId)
    {
        BuildRequest Request{};
        Request.ProjectFilePath = ProjectFile;
        Request.ProfileName = std::string(ProfileName);

        auto Resolved = BuildRequestService::Resolve(Request);
        if (!Resolved)
        {
            throw std::runtime_error(Resolved.error().Message);
        }

        BuildPlannerOptions PlannerOptions{};
        PlannerOptions.BuildId = std::string(BuildId);
        auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
        if (!Plan)
        {
            throw std::runtime_error(Plan.error().Message);
        }

        return {*Resolved, *Plan};
    }

    /**
     * @brief Write one small executable shell script to disk for host-tool fakes.
     * @param FilePath Script file path to author.
     * @param Body Shell-script body without the shebang.
     */
    void WriteExecutableScript(const std::filesystem::path& FilePath, const std::string_view Body)
    {
        std::filesystem::create_directories(FilePath.parent_path());
        std::ofstream Output(FilePath, std::ios::binary | std::ios::trunc);
        if (!Output.is_open())
        {
            throw std::runtime_error("Failed to write test script");
        }

        Output << "#!/usr/bin/env bash\n";
        Output << Body;
        Output.flush();
        Output.close();

        std::filesystem::permissions(
            FilePath,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
                std::filesystem::perms::owner_exec | std::filesystem::perms::group_read |
                std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                std::filesystem::perms::others_exec,
            std::filesystem::perm_options::replace);
    }

    /**
     * @brief Command-runner fake used to inspect generated CMake commands and synthesize outputs.
     */
    class RecordingCommandRunner final : public ICodeBuildCommandRunner
    {
    public:
        std::vector<CodeBuildCommand> Commands{};

        [[nodiscard]] TExpected<int> Execute(const CodeBuildCommand& Command) override
        {
            Commands.push_back(Command);

            const auto BuildDirectory = FindArgumentValue(Command.Arguments, "-B");
            if (BuildDirectory)
            {
                std::filesystem::create_directories(*BuildDirectory);
                std::ofstream(*BuildDirectory / "CMakeCache.txt") << "SNAPI_PROJECT_ROOT_DIR:PATH=stub\n";
                return 0;
            }

            const auto BuildIndex = std::ranges::find(Command.Arguments, std::string("--build"));
            if (BuildIndex != Command.Arguments.end() && std::next(BuildIndex) != Command.Arguments.end())
            {
                const std::filesystem::path BuildDir = std::filesystem::path(*std::next(BuildIndex));
                std::filesystem::create_directories(BuildDir);
                std::ofstream(BuildDir / "SnAPI.GameFramework.Runtime") << "runtime-binary";
                return 0;
            }

            return 0;
        }

    private:
        /**
         * @brief Find one flag's following argument in a tokenized command line.
         * @param Arguments Tokenized command arguments.
         * @param Flag Flag to locate.
         * @return Path value when present.
         */
        [[nodiscard]] static std::optional<std::filesystem::path> FindArgumentValue(
            const std::vector<std::string>& Arguments, const std::string_view Flag)
        {
            const auto It = std::ranges::find(Arguments, std::string(Flag));
            if (It == Arguments.end() || std::next(It) == Arguments.end())
            {
                return std::nullopt;
            }
            return std::filesystem::path(*std::next(It));
        }
    };

} // namespace

TEST_CASE("CodeBuildServiceAdapter builds host-local and docker CMake commands", "[Build][Code]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("host-local"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "CodeBuildHost", {WindowsDevelopment});
    const auto [Resolved, Plan] = ResolveRequestAndPlan(ProjectFile, "WindowsDevelopment", "20260322-040101-codehost");

    const auto ConfigureNode =
        std::ranges::find_if(Plan.Nodes, [](const BuildGraphNode& Node) { return Node.Type == EBuildNodeType::ConfigureCMake; });
    const auto BuildNode =
        std::ranges::find_if(Plan.Nodes, [](const BuildGraphNode& Node) { return Node.Type == EBuildNodeType::BuildCode; });
    REQUIRE(ConfigureNode != Plan.Nodes.end());
    REQUIRE(BuildNode != Plan.Nodes.end());

    CodeBuildServiceOptions Options{};
    Options.Enabled = true;
    Options.EngineSourceDirectory = Root.Path / "EngineSource";
    Options.Generator = "Ninja";
    Options.ParallelJobs = 7u;

    auto ConfigureCommand = CodeBuildServiceAdapter::CreateConfigureCommand(
        Resolved, *ConfigureNode, Options, Root.Path / "configure.log");
    REQUIRE(ConfigureCommand);
    CHECK(ConfigureCommand->Arguments[0] == "cmake");
    CHECK(std::ranges::find(ConfigureCommand->Arguments,
                            (Resolved.Project.IntermediateRootDirectory / "Build" / "windows" / "development" /
                             "host-local")
                                .lexically_normal()
                                .generic_string()) != ConfigureCommand->Arguments.end());
    CHECK(std::ranges::find(ConfigureCommand->Arguments, std::string("-G")) != ConfigureCommand->Arguments.end());
    CHECK(std::ranges::find(ConfigureCommand->Arguments, std::string("Ninja")) != ConfigureCommand->Arguments.end());
    CHECK(std::ranges::find_if(ConfigureCommand->Arguments, [&](const std::string& Argument)
                               { return Argument == "-DSNAPI_PROJECT_ROOT_DIR=" +
                                                        Resolved.Project.ProjectRootDirectory.lexically_normal().generic_string(); }) !=
          ConfigureCommand->Arguments.end());

    auto BuildCommand =
        CodeBuildServiceAdapter::CreateBuildCommand(Resolved, *BuildNode, Options, Root.Path / "build.log");
    REQUIRE(BuildCommand);
    CHECK(BuildCommand->Arguments[0] == "cmake");
    CHECK(std::ranges::find(BuildCommand->Arguments, std::string("--build")) != BuildCommand->Arguments.end());
    CHECK(std::ranges::find(BuildCommand->Arguments, std::string("--parallel")) != BuildCommand->Arguments.end());
    CHECK(std::ranges::find(BuildCommand->Arguments, std::string("7")) != BuildCommand->Arguments.end());
    CHECK(std::ranges::find(BuildCommand->Arguments, std::string("--target")) != BuildCommand->Arguments.end());
    CHECK(std::ranges::find(BuildCommand->Arguments, std::string("SnAPI.GameFramework.Runtime")) !=
          BuildCommand->Arguments.end());

    ResolvedBuildRequest DockerResolved = Resolved;
    DockerResolved.Profile.ExecutionEnvironment = "docker://snapi/windows-msvc:2026.03";
    auto DockerConfigureCommand = CodeBuildServiceAdapter::CreateConfigureCommand(
        DockerResolved, *ConfigureNode, Options, Root.Path / "docker-configure.log");
    REQUIRE(DockerConfigureCommand);
    CHECK(DockerConfigureCommand->Arguments[0] == "docker");
    CHECK(std::ranges::find(DockerConfigureCommand->Arguments, std::string("run")) !=
          DockerConfigureCommand->Arguments.end());
    CHECK(std::ranges::find(DockerConfigureCommand->Arguments, std::string("snapi/windows-msvc:2026.03")) !=
          DockerConfigureCommand->Arguments.end());
}

TEST_CASE("CodeBuildServiceAdapter resolves the engine source root from the compiled GameFramework tree", "[Build][Code]")
{
    TempDir Root{};

    BuildProfile LinuxDevelopment{};
    LinuxDevelopment.Name = "LinuxDevelopment";
    LinuxDevelopment.Platform = SetValue(std::string("Linux"));
    LinuxDevelopment.ExecutionEnvironment = SetValue(std::string("host-local"));
    LinuxDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    LinuxDevelopment.SelectedLevels.IsSet = true;
    LinuxDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "CodeBuildDefaultSourceRoot", {LinuxDevelopment});
    const auto [Resolved, Plan] =
        ResolveRequestAndPlan(ProjectFile, "LinuxDevelopment", "20260323-071501-default-source-root");

    const auto ConfigureNode =
        std::ranges::find_if(Plan.Nodes, [](const BuildGraphNode& Node) { return Node.Type == EBuildNodeType::ConfigureCMake; });
    REQUIRE(ConfigureNode != Plan.Nodes.end());

    const std::filesystem::path ExpectedSourceRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path().lexically_normal();
    const std::filesystem::path NestedWorkingDirectory = Root.Path / "nested" / "build" / "relwithdebinfo";
    std::filesystem::create_directories(NestedWorkingDirectory);
    ScopedCurrentPath CurrentPathScope(NestedWorkingDirectory);

    CodeBuildServiceOptions Options{};
    Options.Enabled = true;

    auto ConfigureCommand = CodeBuildServiceAdapter::CreateConfigureCommand(
        Resolved, *ConfigureNode, Options, Root.Path / "default-source-root.log");
    REQUIRE(ConfigureCommand);
    REQUIRE(ConfigureCommand->Arguments.size() >= 4u);
    CHECK(ConfigureCommand->Arguments[0] == "cmake");
    CHECK(ConfigureCommand->Arguments[1] == "-S");
    CHECK(std::filesystem::path(ConfigureCommand->Arguments[2]).lexically_normal() == ExpectedSourceRoot);
}

TEST_CASE("BuildExecutionService can drive real code nodes through CodeBuildServiceAdapter", "[Build][Code]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("host-local"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "CodeBuildExecution", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-040102-codeexec";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    RecordingCommandRunner Runner{};

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.CodeBuild.Enabled = true;
    ExecutionOptions.CodeBuild.EngineSourceDirectory = Root.Path / "EngineSource";
    ExecutionOptions.CodeBuild.CommandRunner = &Runner;
    ExecutionOptions.CodeBuild.Generator = "Ninja";
    ExecutionOptions.CodeBuild.ParallelJobs = 5u;

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);
    CHECK(Runner.Commands.size() == 2u);
    CHECK(std::ranges::find(Runner.Commands.back().Arguments, std::string("--parallel")) !=
          Runner.Commands.back().Arguments.end());
    CHECK(std::ranges::find(Runner.Commands.back().Arguments, std::string("5")) !=
          Runner.Commands.back().Arguments.end());

    const std::filesystem::path GeneratedModules =
        Resolved->Project.IntermediateRootDirectory / "Build" / "Generated" / "ProjectModules.cmake";
    CHECK(std::filesystem::exists(GeneratedModules));
    CHECK(std::filesystem::exists(
        Resolved->Project.IntermediateRootDirectory / "Build" / "windows" / "development" / "host-local" /
        "CMakeCache.txt"));

    const std::filesystem::path StagedRuntime = Report->StageDirectory / "Bin" / "SnAPI.GameFramework.Runtime";
    CHECK(std::filesystem::exists(StagedRuntime));
    CHECK(std::ranges::any_of(Report->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.Type == EBuildNodeType::BuildCode &&
                                       std::ranges::any_of(Record.Outputs, [](const std::string& Output)
                                                           { return Output.find("SnAPI.GameFramework.Runtime") !=
                                                                    std::string::npos; }); }));
}

#if !defined(_WIN32)
TEST_CASE("CodeBuildServiceAdapter bundles non-system ELF runtime dependencies into the artifact directory",
          "[Build][Code]")
{
    TempDir Root{};

    BuildProfile LinuxDevelopment{};
    LinuxDevelopment.Name = "LinuxDevelopment";
    LinuxDevelopment.Platform = SetValue(std::string("Linux"));
    LinuxDevelopment.ExecutionEnvironment = SetValue(std::string("host-local"));
    LinuxDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    LinuxDevelopment.SelectedLevels.IsSet = true;
    LinuxDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "CodeBuildRuntimeDeps", {LinuxDevelopment});
    const auto [Resolved, Plan] =
        ResolveRequestAndPlan(ProjectFile, "LinuxDevelopment", "20260323-120101-runtime-deps");

    const auto BuildNode =
        std::ranges::find_if(Plan.Nodes, [](const BuildGraphNode& Node) { return Node.Type == EBuildNodeType::BuildCode; });
    REQUIRE(BuildNode != Plan.Nodes.end());

    const std::filesystem::path NonSystemDependencyDirectory = Root.Path / "thirdparty";
    std::filesystem::create_directories(NonSystemDependencyDirectory);
    const std::filesystem::path NonSystemDependencySource = NonSystemDependencyDirectory / "libCustomRuntimeDependency.so";
    std::filesystem::copy_file("/bin/ls", NonSystemDependencySource, std::filesystem::copy_options::overwrite_existing);

    const std::filesystem::path LddScript = Root.Path / "tools" / "fake-ldd.sh";
    WriteExecutableScript(
        LddScript,
        "printf 'linux-vdso.so.1 (0x00000000)\\n'\n"
        "printf 'libCustomRuntimeDependency.so => " +
            NonSystemDependencySource.lexically_normal().generic_string() + " (0x00000000)\\n'\n"
        "printf 'libc.so.6 => /usr/lib/libc.so.6 (0x00000000)\\n'\n");

    const std::filesystem::path PatchelfLog = Root.Path / "tools" / "patchelf.log";
    const std::filesystem::path PatchelfScript = Root.Path / "tools" / "fake-patchelf.sh";
    WriteExecutableScript(
        PatchelfScript,
        "printf '%s\\n' \"$*\" >> '" + PatchelfLog.lexically_normal().generic_string() + "'\n");

    RecordingCommandRunner Runner{};
    CodeBuildServiceOptions Options{};
    Options.Enabled = true;
    Options.EngineSourceDirectory = Root.Path / "EngineSource";
    Options.CommandRunner = &Runner;
    Options.LddExecutable = LddScript.string();
    Options.PatchelfExecutable = PatchelfScript.string();

    auto Result = CodeBuildServiceAdapter::ExecuteBuildCode(Resolved, *BuildNode, Options);
    REQUIRE(Result);

    const std::filesystem::path ArtifactDirectory = std::filesystem::path(BuildNode->Outputs.front()).lexically_normal();
    const std::filesystem::path PackagedRuntime = ArtifactDirectory / "SnAPI.GameFramework.Runtime";
    const std::filesystem::path PackagedDependency = ArtifactDirectory / "libCustomRuntimeDependency.so";

    CHECK(std::filesystem::exists(PackagedRuntime));
    CHECK(std::filesystem::exists(PackagedDependency));
    CHECK(std::ranges::find(Result->Outputs, PackagedRuntime.generic_string()) != Result->Outputs.end());
    CHECK(std::ranges::find(Result->Outputs, PackagedDependency.generic_string()) != Result->Outputs.end());

    std::ifstream PatchelfLogInput(PatchelfLog, std::ios::binary);
    REQUIRE(PatchelfLogInput.is_open());
    const std::string PatchelfLogContents((std::istreambuf_iterator<char>(PatchelfLogInput)),
                                          std::istreambuf_iterator<char>());
    CHECK(PatchelfLogContents.find("--set-rpath $ORIGIN") != std::string::npos);
    CHECK(PatchelfLogContents.find("SnAPI.GameFramework.Runtime") != std::string::npos);
    CHECK(PatchelfLogContents.find("libCustomRuntimeDependency.so") != std::string::npos);
}
#endif
