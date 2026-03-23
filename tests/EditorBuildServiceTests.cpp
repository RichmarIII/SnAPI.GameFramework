#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <typeindex>

#include <catch2/catch_test_macros.hpp>

#include "Editor/EditorAssetService.h"
#include "Editor/EditorBuildService.h"
#include "Editor/IEditorService.h"
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Editor;

namespace
{

struct TempDir
{
    std::filesystem::path Path{};

    TempDir()
    {
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_editor_build_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

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

[[nodiscard]] std::string HostDevelopmentProfileName()
{
    return HostPlatformName() + "Development";
}

void AddHostDevelopmentProfile(const std::filesystem::path& ProjectFilePath)
{
    auto Descriptor = ProjectDescriptorService::Load(ProjectFilePath.string());
    REQUIRE(Descriptor);

    BuildProfile Profile{};
    Profile.Name = HostDevelopmentProfileName();
    Profile.Platform = BuildProfileValue<std::string>{
        .IsSet = true,
        .Value = HostPlatformName(),
    };
    Profile.ExecutionEnvironment = BuildProfileValue<std::string>{
        .IsSet = true,
        .Value = std::string("host-local"),
    };
    Profile.Configuration = BuildProfileValue<EBuildConfiguration>{
        .IsSet = true,
        .Value = EBuildConfiguration::Development,
    };
    Profile.SelectedLevels.IsSet = true;
    Profile.SelectedLevels.Values = {Descriptor->Startup.StartupLevelAsset};

    Descriptor->Profiles = {std::move(Profile)};
    REQUIRE(ProjectDescriptorService::Save(*Descriptor, ProjectFilePath.string()));
}

struct TestEditorBuildHost final : IEditorServiceHost
{
    GameRuntime Runtime{};
    EditorAssetService AssetService{};
    EditorBuildService BuildService{};

    TestEditorBuildHost()
    {
        REQUIRE(Runtime.Init({}));
        EditorServiceContext Context(*this);
        REQUIRE(AssetService.Initialize(Context));
        REQUIRE(BuildService.Initialize(Context));
    }

    ~TestEditorBuildHost() override
    {
        EditorServiceContext Context(*this);
        BuildService.Shutdown(Context);
        AssetService.Shutdown(Context);
        Runtime.Shutdown();
    }

    [[nodiscard]] GameRuntime& RuntimeForServices() override
    {
        return Runtime;
    }

    [[nodiscard]] const GameRuntime& RuntimeForServices() const override
    {
        return Runtime;
    }

    [[nodiscard]] IEditorService* ResolveServiceForContext(const std::type_index& Type) override
    {
        if (Type == std::type_index(typeid(EditorAssetService)))
        {
            return &AssetService;
        }
        if (Type == std::type_index(typeid(EditorBuildService)))
        {
            return &BuildService;
        }
        return nullptr;
    }

    [[nodiscard]] const IEditorService* ResolveServiceForContext(const std::type_index& Type) const override
    {
        if (Type == std::type_index(typeid(EditorAssetService)))
        {
            return &AssetService;
        }
        if (Type == std::type_index(typeid(EditorBuildService)))
        {
            return &BuildService;
        }
        return nullptr;
    }
};

class TestBuildNodeExecutor final : public IBuildNodeExecutor
{
public:
    EBuildNodeType FailureType = EBuildNodeType::WriteBuildReport;
    bool EnableFailure = false;
    bool EmitConsoleNoise = false;
    std::size_t LargeConsoleNoiseLineCount = 0u;

    [[nodiscard]] TExpected<BuildNodeExecutionResult> Execute(const ResolvedBuildRequest&,
                                                              const BuildGraph&,
                                                              const BuildGraphNode& Node) override
    {
        if (EmitConsoleNoise)
        {
            std::cout << "\x1b[2K\rSynthetic progress 10%";
            std::cout << "\x1b[2K\rSynthetic progress 20%";
            std::cout << "\nSynthetic stdout line\n";
            std::cerr << "\x1b[31mSynthetic warning line\x1b[0m\n";
        }

        if (LargeConsoleNoiseLineCount > 0u)
        {
            for (std::size_t Index = 0u; Index < LargeConsoleNoiseLineCount; ++Index)
            {
                std::cout << "Synthetic large console line " << Index << " ........................................\n";
            }
        }

        if (EnableFailure && Node.Type == FailureType)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Synthetic executor failure"));
        }

        const auto IsDirectoryOutput = [&Node]()
        {
            switch (Node.Type)
            {
            case EBuildNodeType::BuildCode:
            case EBuildNodeType::CookAssets:
            case EBuildNodeType::WriteSnpak:
            case EBuildNodeType::CreateStageTree:
            case EBuildNodeType::StageBinaries:
            case EBuildNodeType::StageAssets:
            case EBuildNodeType::StageConfigs:
                return true;
            default:
                return false;
            }
        }();

        for (const std::string& Output : Node.Outputs)
        {
            const std::filesystem::path OutputPath = std::filesystem::path(Output);
            if (IsDirectoryOutput)
            {
                std::filesystem::create_directories(OutputPath);
                continue;
            }

            std::filesystem::create_directories(OutputPath.parent_path());
            std::ofstream Stream(OutputPath, std::ios::binary | std::ios::trunc);
            Stream << "Synthetic editor test output for " << Node.Name << "\n";
        }

        return BuildNodeExecutionResult{
            .CacheHit = false,
            .Message = "Executed by synthetic editor test executor.",
            .Outputs = Node.Outputs,
        };
    }
};

void DrainAsyncBuild(TestEditorBuildHost& Host,
                     EditorServiceContext& Context,
                     const std::chrono::milliseconds Timeout = std::chrono::milliseconds(5000))
{
    const auto Deadline = std::chrono::steady_clock::now() + Timeout;
    while (Host.BuildService.IsBusy() && std::chrono::steady_clock::now() < Deadline)
    {
        Host.BuildService.Tick(Context, 0.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Host.BuildService.Tick(Context, 0.0f);
    REQUIRE_FALSE(Host.BuildService.IsBusy());
}

} // namespace

TEST_CASE("Editor build service plans active projects through the shared build backend", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildPlanGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildPlanGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    auto DefaultRequest = Host.BuildService.MakeDefaultRequest(Context);
    REQUIRE(DefaultRequest);
    CHECK(DefaultRequest->ProfileName == HostDevelopmentProfileName());
    CHECK(DefaultRequest->ProjectFilePath.lexically_normal() == ProjectFilePath.lexically_normal());

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-120001-editor-plan";

    auto EditorPlan = Host.BuildService.PlanActiveProject(Context, {}, PlannerOptions);
    REQUIRE(EditorPlan);
    REQUIRE(Host.BuildService.LastPlan().has_value());

    auto Resolved = BuildRequestService::Resolve(*DefaultRequest);
    REQUIRE(Resolved);
    auto BackendPlan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(BackendPlan);

    CHECK(EditorPlan->Request.RequestHash == Resolved->RequestHash);

    auto EditorGraphJson = BuildPlannerService::Serialize(EditorPlan->Graph, 2);
    REQUIRE(EditorGraphJson);
    auto BackendGraphJson = BuildPlannerService::Serialize(*BackendPlan, 2);
    REQUIRE(BackendGraphJson);
    CHECK(*EditorGraphJson == *BackendGraphJson);
}

TEST_CASE("Editor build service resolves profile summaries for the active project", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildProfilesGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildProfilesGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    auto Profiles = Host.BuildService.ListProfiles(Context);
    REQUIRE(Profiles);
    REQUIRE_FALSE(Profiles->empty());

    const auto AdHocIt = std::find_if(Profiles->begin(), Profiles->end(), [](const EditorBuildProfileSummary& Profile) {
        return Profile.IsAdHoc;
    });
    REQUIRE(AdHocIt != Profiles->end());
    CHECK(AdHocIt->Label == "Ad Hoc Host Development");

    const auto AuthoredIt = std::find_if(
        Profiles->begin(),
        Profiles->end(),
        [](const EditorBuildProfileSummary& Profile) { return Profile.Name == HostDevelopmentProfileName(); });
    REQUIRE(AuthoredIt != Profiles->end());
    CHECK(AuthoredIt->Platform == HostPlatformName());
    CHECK(AuthoredIt->Configuration == "Development");
    CHECK(AuthoredIt->ExecutionEnvironment == "host-local");
    CHECK(AuthoredIt->IsDefault);
}

TEST_CASE("Editor and CLI build planning stay in parity for the same project profile", "[Build][Editor][CLI]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorCliParityGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorCliParityGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-120051-editor-cli-parity";

    auto EditorPlan = Host.BuildService.PlanActiveProject(Context, {}, PlannerOptions);
    REQUIRE(EditorPlan);

    BuildCliOptions CliOptions{};
    CliOptions.CurrentWorkingDirectory = Root.Path;
    const BuildCliResult CliResult =
        BuildCliService::Run({"validate", "--project", ProjectFilePath.string(), "--profile", HostDevelopmentProfileName(),
                              "--build-id", PlannerOptions.BuildId},
                             CliOptions);
    REQUIRE(CliResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(CliResult.PlannedGraph.has_value());

    auto EditorGraphJson = BuildPlannerService::Serialize(EditorPlan->Graph, 2);
    REQUIRE(EditorGraphJson);
    auto CliGraphJson = BuildPlannerService::Serialize(*CliResult.PlannedGraph, 2);
    REQUIRE(CliGraphJson);
    CHECK(*EditorGraphJson == *CliGraphJson);
}

TEST_CASE("Editor build service packages active projects and exposes build history", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildPackageGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildPackageGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-120101-editor-package";

    auto Report = Host.BuildService.PackageActiveProject(Context, {}, PlannerOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);
    REQUIRE(Host.BuildService.LastReport().has_value());
    CHECK(Host.BuildService.ConsoleLogProjectFilePath().lexically_normal() == ProjectFilePath.lexically_normal());
    CHECK_FALSE(Host.BuildService.ConsoleLogText().empty());
    CHECK(Host.BuildService.ConsoleLogText().find("BuildStarted") != std::string::npos);
    CHECK(std::filesystem::exists(Report->BuildRequestFilePath));
    CHECK(std::filesystem::exists(Report->BuildPlanFilePath));
    CHECK(std::filesystem::exists(Report->BuildReportFilePath));
    CHECK(std::filesystem::exists(Report->BuildSummaryFilePath));

    auto History = Host.BuildService.ListHistory(Context);
    REQUIRE(History);
    const auto EntryIt = std::find_if(
        History->begin(),
        History->end(),
        [&Report](const BuildHistoryEntry& Entry) { return Entry.BuildId == Report->BuildId; });
    REQUIRE(EntryIt != History->end());

    auto Loaded = Host.BuildService.LoadHistoryReport(Context, Report->BuildId);
    REQUIRE(Loaded);
    CHECK(Loaded->BuildId == Report->BuildId);
    CHECK(Loaded->RequestHash == Report->RequestHash);
}

TEST_CASE("Editor build service retries prior frozen requests and resumes prior successful nodes", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildRetryGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildRetryGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions FirstPlanner{};
    FirstPlanner.BuildId = "20260322-120201-editor-retry-a";
    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.EnablePersistentNodeCache = false;

    auto FirstReport = Host.BuildService.PackageActiveProject(Context, {}, FirstPlanner, ExecutionOptions);
    REQUIRE(FirstReport);
    CHECK(FirstReport->Status == EBuildExecutionStatus::Succeeded);

    BuildPlannerOptions SecondPlanner{};
    SecondPlanner.BuildId = "20260322-120202-editor-retry-b";
    auto SecondReport = Host.BuildService.RetryBuild(Context, FirstReport->BuildId, SecondPlanner, ExecutionOptions);
    REQUIRE(SecondReport);
    CHECK(SecondReport->Status == EBuildExecutionStatus::Succeeded);
    CHECK(std::any_of(SecondReport->NodeRecords.begin(),
                      SecondReport->NodeRecords.end(),
                      [](const BuildNodeExecutionRecord& Record) { return Record.CacheHit; }));

    auto Comparison = Host.BuildService.CompareHistory(Context, FirstReport->BuildId, SecondReport->BuildId);
    REQUIRE(Comparison);
    CHECK(Comparison->SameRequestHash);
}

TEST_CASE("Editor build service surfaces failed build reasons in status text and console logs", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildFailureGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildFailureGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260323-070001-editor-failure";

    TestBuildNodeExecutor Executor{};
    Executor.EnableFailure = true;
    Executor.FailureType = EBuildNodeType::BuildCode;

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.NodeExecutor = &Executor;
    ExecutionOptions.EnablePersistentNodeCache = false;

    auto Report = Host.BuildService.PackageActiveProject(Context, {}, PlannerOptions, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Failed);
    CHECK(Host.BuildService.StatusMessage().find("Synthetic executor failure") != std::string::npos);
    CHECK(Host.BuildService.StatusMessage().find("Packaged build " + PlannerOptions.BuildId + " with status Failed.") !=
          std::string::npos);

    const std::string ConsoleLog = Host.BuildService.ConsoleLogText();
    CHECK(ConsoleLog.find("Synthetic executor failure") != std::string::npos);
    CHECK(ConsoleLog.find("NodeFailed") != std::string::npos);
    CHECK(ConsoleLog.find("[SnAPI][EditorBuild] Packaged build " + PlannerOptions.BuildId + " with status Failed.") !=
          std::string::npos);
}

TEST_CASE("Editor build service sanitizes carriage-return and ANSI-heavy console output", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildConsoleGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildConsoleGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260323-130101-editor-console";

    TestBuildNodeExecutor Executor{};
    Executor.EmitConsoleNoise = true;

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.NodeExecutor = &Executor;
    ExecutionOptions.EnablePersistentNodeCache = false;

    auto Report = Host.BuildService.PackageActiveProject(Context, {}, PlannerOptions, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);

    const std::string ConsoleLog = Host.BuildService.ConsoleLogText();
    CHECK(ConsoleLog.find('\r') == std::string::npos);
    CHECK(ConsoleLog.find("\x1b") == std::string::npos);
    CHECK(ConsoleLog.find("Synthetic stdout line") != std::string::npos);
    CHECK(ConsoleLog.find("Synthetic warning line") != std::string::npos);
    CHECK_FALSE(ConsoleLog.starts_with('\n'));
    CHECK(ConsoleLog.find("\n\n\n") == std::string::npos);
}

TEST_CASE("Editor build service returns a recent UI-safe tail for large console logs", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildLargeConsoleGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildLargeConsoleGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260323-190101-editor-large-console";

    TestBuildNodeExecutor Executor{};
    Executor.LargeConsoleNoiseLineCount = 4000u;

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.NodeExecutor = &Executor;
    ExecutionOptions.EnablePersistentNodeCache = false;

    auto Report = Host.BuildService.PackageActiveProject(Context, {}, PlannerOptions, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);

    const std::string ConsoleLog = Host.BuildService.ConsoleLogText();
    CHECK(ConsoleLog.find("[Console truncated to recent output]") != std::string::npos);
    CHECK(ConsoleLog.find("Synthetic large console line 0") == std::string::npos);
    CHECK(ConsoleLog.find("Synthetic large console line 3999") != std::string::npos);
    CHECK_FALSE(ConsoleLog.starts_with('\n'));
    CHECK(ConsoleLog.find("\n\n\n") == std::string::npos);
}

TEST_CASE("Editor build service queues package builds asynchronously for the editor shell", "[Build][Editor]")
{
    TestEditorBuildHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorBuildAsyncGame", Root.Path.string()));

    const std::filesystem::path ProjectFilePath = Root.Path / "EditorBuildAsyncGame" / "project.snproj.json";
    AddHostDevelopmentProfile(ProjectFilePath);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260323-130001-editor-async";

    REQUIRE(Host.BuildService.QueuePackageActiveProject(Context, {}, PlannerOptions));
    CHECK(Host.BuildService.IsBusy());
    CHECK(Host.BuildService.StatusMessage().find("in the background") != std::string::npos);

    DrainAsyncBuild(Host, Context);

    REQUIRE(Host.BuildService.LastReport().has_value());
    CHECK(Host.BuildService.LastReport()->BuildId == PlannerOptions.BuildId);
    CHECK(Host.BuildService.LastReport()->Status == EBuildExecutionStatus::Succeeded);
}
