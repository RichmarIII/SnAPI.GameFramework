#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_build_execution_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Ec{};
            std::filesystem::remove_all(Path, Ec);
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
     * @brief Create one project descriptor on disk for build-execution tests.
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

        const std::filesystem::path StartupLevelPath =
            ProjectRoot / "Assets" / std::filesystem::path(Descriptor.Startup.StartupLevelAsset);
        std::error_code DirectoryError{};
        std::filesystem::create_directories(StartupLevelPath.parent_path(), DirectoryError);
        if (DirectoryError)
        {
            throw std::runtime_error("Failed to create startup level directory: " + DirectoryError.message());
        }

        std::ofstream StartupLevelStream(StartupLevelPath, std::ios::binary | std::ios::trunc);
        if (!StartupLevelStream.is_open())
        {
            throw std::runtime_error("Failed to write startup level fixture");
        }
        StartupLevelStream << "{}";
        if (!StartupLevelStream.good())
        {
            throw std::runtime_error("Failed to flush startup level fixture");
        }

        const std::filesystem::path ConfigRoot = ProjectRoot / Descriptor.Paths.ConfigRoot;
        std::filesystem::create_directories(ConfigRoot, DirectoryError);
        if (DirectoryError)
        {
            throw std::runtime_error("Failed to create config directory: " + DirectoryError.message());
        }

        const std::filesystem::path CodeRoot = ProjectRoot / Descriptor.Paths.CodeRoot;
        std::filesystem::create_directories(CodeRoot, DirectoryError);
        if (DirectoryError)
        {
            throw std::runtime_error("Failed to create code directory: " + DirectoryError.message());
        }

        std::ofstream SourceStream(CodeRoot / "Gameplay.cpp", std::ios::binary | std::ios::trunc);
        if (!SourceStream.is_open())
        {
            throw std::runtime_error("Failed to write code fixture");
        }
        SourceStream << "// synthetic code fixture\n";
        if (!SourceStream.good())
        {
            throw std::runtime_error("Failed to flush code fixture");
        }

        return ProjectFilePath;
    }

    /**
     * @brief Test executor that can synthesize cache hits and a targeted failure.
     */
    class TestBuildNodeExecutor final : public IBuildNodeExecutor
    {
    public:
        EBuildNodeType FailureType = EBuildNodeType::WriteBuildReport;
        bool EnableFailure = false;

        [[nodiscard]] TExpected<BuildNodeExecutionResult> Execute(const ResolvedBuildRequest&,
                                                                  const BuildGraph&,
                                                                  const BuildGraphNode& Node) override
        {
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
                Stream << "Synthetic test output for " << Node.Name << "\n";
            }

            return BuildNodeExecutionResult{
                .CacheHit = Node.Type == EBuildNodeType::LoadProject,
                .Message = Node.Type == EBuildNodeType::LoadProject ? "Loaded from synthetic cache."
                                                                    : "Executed by synthetic test executor.",
                .Outputs = Node.Outputs,
            };
        }
    };

} // namespace

TEST_CASE("BuildExecutionService executes planned graphs and writes history artifacts", "[Build][Execute]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ExecutionHost", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-020304-feedbeef";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    std::vector<BuildEvent> Events{};
    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.EventSink = [&](const BuildEvent& Event) { Events.push_back(Event); };

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);

    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);
    CHECK(Report->BuildId == PlannerOptions.BuildId);
    CHECK(Report->RequestHash == Resolved->RequestHash);
    CHECK(Report->NodeRecords.size() == Plan->Nodes.size());
    CHECK_FALSE(Report->OutputFiles.empty());
    CHECK(std::filesystem::exists(Report->BuildRequestFilePath));
    CHECK(std::filesystem::exists(Report->BuildPlanFilePath));
    CHECK(std::filesystem::exists(Report->BuildReportFilePath));
    CHECK(std::filesystem::exists(Report->BuildSummaryFilePath));
    CHECK(std::filesystem::exists(Report->PackageDirectoryPath));
    CHECK(std::filesystem::exists(Report->PackageOutputRootDirectory));
    CHECK_FALSE(Report->StageLogFilePaths.empty());

    const std::filesystem::path PackageManifestPath = Report->StageDirectory / "Metadata" / "PackageManifest.json";
    const std::filesystem::path StageFileHashesPath = Report->StageDirectory / "Metadata" / "StageFileHashes.json";
    const std::filesystem::path ResolvedRuntimeConfigPath = Report->StageDirectory / "Config" / "ResolvedRuntimeConfig.json";
    const std::filesystem::path CookManifestPath = Report->HistoryDirectory / "Manifests" / "CookManifest.json";
    CHECK(std::filesystem::exists(PackageManifestPath));
    CHECK(std::filesystem::exists(StageFileHashesPath));
    CHECK(std::filesystem::exists(ResolvedRuntimeConfigPath));
    CHECK(std::filesystem::exists(CookManifestPath));

    REQUIRE_FALSE(Events.empty());
    CHECK(Events.front().Kind == EBuildEventKind::BuildStarted);
    CHECK(std::ranges::any_of(Events, [](const BuildEvent& Event) { return Event.Kind == EBuildEventKind::BuildPlanReady; }));
    CHECK(std::ranges::any_of(Events, [](const BuildEvent& Event) { return Event.Kind == EBuildEventKind::BuildFinished; }));

    auto ReportJson = BuildExecutionService::SerializeReport(*Report, 2);
    REQUIRE(ReportJson);
    const nlohmann::ordered_json Serialized = nlohmann::ordered_json::parse(*ReportJson, nullptr, false);
    REQUIRE_FALSE(Serialized.is_discarded());
    CHECK(Serialized["BuildId"] == PlannerOptions.BuildId);
    CHECK(Serialized["Status"] == "Succeeded");
    CHECK(Serialized["Nodes"].size() == Report->NodeRecords.size());

    std::ifstream PackageManifestStream(PackageManifestPath, std::ios::binary);
    REQUIRE(PackageManifestStream.is_open());
    const std::string PackageManifestText((std::istreambuf_iterator<char>(PackageManifestStream)),
                                          std::istreambuf_iterator<char>());
    const nlohmann::ordered_json PackageManifestJson =
        nlohmann::ordered_json::parse(PackageManifestText, nullptr, false);
    REQUIRE_FALSE(PackageManifestJson.is_discarded());
    CHECK(PackageManifestJson["BuildId"] == PlannerOptions.BuildId);
    CHECK(PackageManifestJson["ProjectName"] == "ExecutionHost");
    CHECK(PackageManifestJson["TargetPlatform"] == "Windows");
    CHECK(PackageManifestJson["OutputFiles"].is_array());
    CHECK(PackageManifestJson["OutputFiles"].size() >= 2u);

    std::ifstream StageHashesStream(StageFileHashesPath, std::ios::binary);
    REQUIRE(StageHashesStream.is_open());
    const std::string StageHashesText((std::istreambuf_iterator<char>(StageHashesStream)),
                                      std::istreambuf_iterator<char>());
    const nlohmann::ordered_json StageHashesJson = nlohmann::ordered_json::parse(StageHashesText, nullptr, false);
    REQUIRE_FALSE(StageHashesJson.is_discarded());
    CHECK(StageHashesJson["BuildId"] == PlannerOptions.BuildId);
    CHECK(StageHashesJson["Files"].is_array());
    CHECK(StageHashesJson["Files"].size() >= 2u);
}

TEST_CASE("BuildExecutionService records cache hits and node failures from custom executors", "[Build][Execute]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ExecutionFailureHost", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-020305-deadbeef";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    std::vector<BuildEvent> Events{};
    TestBuildNodeExecutor Executor{};
    Executor.EnableFailure = true;
    Executor.FailureType = EBuildNodeType::BuildCode;

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.EventSink = [&](const BuildEvent& Event) { Events.push_back(Event); };
    ExecutionOptions.NodeExecutor = &Executor;

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);

    CHECK(Report->Status == EBuildExecutionStatus::Failed);
    REQUIRE_FALSE(Report->NodeRecords.empty());
    CHECK(Report->NodeRecords.front().CacheHit);
    CHECK(std::ranges::any_of(Report->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.Type == EBuildNodeType::BuildCode &&
                                       Record.Status == EBuildNodeExecutionStatus::Failed; }));

    CHECK(std::ranges::any_of(Events, [](const BuildEvent& Event) { return Event.Kind == EBuildEventKind::NodeCacheHit; }));
    CHECK(std::ranges::any_of(Events, [](const BuildEvent& Event) { return Event.Kind == EBuildEventKind::NodeFailed; }));
    CHECK(std::ranges::any_of(Events, [](const BuildEvent& Event)
                              { return Event.Kind == EBuildEventKind::BuildFinished &&
                                       Event.Message.find("Failed") != std::string::npos; }));

    CHECK(std::filesystem::exists(Report->BuildReportFilePath));
    CHECK(std::filesystem::exists(Report->BuildSummaryFilePath));
    CHECK(std::ranges::any_of(Report->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.Status == EBuildNodeExecutionStatus::Cancelled; }));
}

TEST_CASE("BuildExecutionService reuses persistent node cache across identical builds", "[Build][Execute]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ExecutionCacheHost", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions FirstPlanner{};
    FirstPlanner.BuildId = "20260322-020306-cache-a";
    auto FirstPlan = BuildPlannerService::CreatePlan(*Resolved, FirstPlanner);
    REQUIRE(FirstPlan);

    auto FirstReport = BuildExecutionService::Execute(*Resolved, *FirstPlan, {});
    REQUIRE(FirstReport);
    CHECK(FirstReport->Status == EBuildExecutionStatus::Succeeded);
    CHECK(std::ranges::none_of(FirstReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                               { return Record.CacheHit; }));

    BuildPlannerOptions SecondPlanner{};
    SecondPlanner.BuildId = "20260322-020307-cache-b";
    auto SecondPlan = BuildPlannerService::CreatePlan(*Resolved, SecondPlanner);
    REQUIRE(SecondPlan);

    auto SecondReport = BuildExecutionService::Execute(*Resolved, *SecondPlan, {});
    REQUIRE(SecondReport);
    CHECK(SecondReport->Status == EBuildExecutionStatus::Succeeded);
    CHECK(std::ranges::any_of(SecondReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.CacheHit; }));
    CHECK(std::ranges::any_of(SecondReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.Type == EBuildNodeType::CookAssets && Record.CacheHit; }));
    CHECK(std::ranges::none_of(SecondReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                               { return Record.Type == EBuildNodeType::BuildCode && Record.CacheHit; }));
}

TEST_CASE("BuildExecutionService invalidates persistent cache when authored asset content changes", "[Build][Execute]")
{
    TempDir Root{};

    BuildProfile LinuxDevelopment{};
    LinuxDevelopment.Name = "LinuxDevelopment";
    LinuxDevelopment.Platform = SetValue(std::string("Linux"));
    LinuxDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    LinuxDevelopment.SelectedLevels.IsSet = true;
    LinuxDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ExecutionCacheInvalidation", {LinuxDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "LinuxDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions FirstPlanner{};
    FirstPlanner.BuildId = "20260323-190001-cache-a";
    auto FirstPlan = BuildPlannerService::CreatePlan(*Resolved, FirstPlanner);
    REQUIRE(FirstPlan);

    auto FirstReport = BuildExecutionService::Execute(*Resolved, *FirstPlan, {});
    REQUIRE(FirstReport);
    CHECK(FirstReport->Status == EBuildExecutionStatus::Succeeded);

    const std::filesystem::path StartupLevelPath =
        Resolved->Project.AssetRootDirectory / std::filesystem::path("Levels/Main.level");
    std::ofstream StartupLevelStream(StartupLevelPath, std::ios::binary | std::ios::trunc);
    REQUIRE(StartupLevelStream.is_open());
    StartupLevelStream << "{ \"Changed\": true }\n";
    REQUIRE(StartupLevelStream.good());
    StartupLevelStream.close();

    std::error_code TimestampError{};
    std::filesystem::last_write_time(
        StartupLevelPath, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(2), TimestampError);
    REQUIRE_FALSE(TimestampError);

    BuildPlannerOptions SecondPlanner{};
    SecondPlanner.BuildId = "20260323-190002-cache-b";
    auto SecondPlan = BuildPlannerService::CreatePlan(*Resolved, SecondPlanner);
    REQUIRE(SecondPlan);

    auto SecondReport = BuildExecutionService::Execute(*Resolved, *SecondPlan, {});
    REQUIRE(SecondReport);
    CHECK(SecondReport->Status == EBuildExecutionStatus::Succeeded);

    const auto CookRecordIt = std::ranges::find_if(
        SecondReport->NodeRecords, [](const BuildNodeExecutionRecord& Record) {
            return Record.Type == EBuildNodeType::CookAssets;
        });
    REQUIRE(CookRecordIt != SecondReport->NodeRecords.end());
    CHECK_FALSE(CookRecordIt->CacheHit);
}

TEST_CASE("BuildExecutionService cancels cooperatively and records the cancellation reason", "[Build][Execute]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ExecutionCancelHost", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-020308-cancelled";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    std::vector<BuildEvent> Events{};
    std::size_t FinishedNodeCount = 0u;

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.EventSink = [&](const BuildEvent& Event)
    {
        Events.push_back(Event);
        if (Event.Kind == EBuildEventKind::NodeFinished)
        {
            ++FinishedNodeCount;
        }
    };
    ExecutionOptions.CancellationRequested = [&FinishedNodeCount]() { return FinishedNodeCount >= 1u; };

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);

    CHECK(Report->Status == EBuildExecutionStatus::Cancelled);
    CHECK(Report->CancellationReason == EBuildCancellationReason::UserRequested);
    REQUIRE(Report->NodeRecords.size() == Plan->Nodes.size());
    CHECK(Report->NodeRecords.front().Status == EBuildNodeExecutionStatus::Succeeded);
    CHECK(std::ranges::all_of(Report->NodeRecords.begin() + 1,
                              Report->NodeRecords.end(),
                              [](const BuildNodeExecutionRecord& Record)
                              { return Record.Status == EBuildNodeExecutionStatus::Cancelled; }));
    CHECK(std::ranges::any_of(Events, [](const BuildEvent& Event)
                              { return Event.Kind == EBuildEventKind::BuildCancelled; }));
    CHECK(Report->PackageDirectoryPath.empty());

    auto Loaded = BuildHistoryService::LoadReport(Report->BuildReportFilePath);
    REQUIRE(Loaded);
    CHECK(Loaded->Status == EBuildExecutionStatus::Cancelled);
    CHECK(Loaded->CancellationReason == EBuildCancellationReason::UserRequested);
}

TEST_CASE("BuildExecutionService resumes prior successful nodes without persistent cache", "[Build][Execute]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ExecutionResumeHost", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions FirstPlanner{};
    FirstPlanner.BuildId = "20260322-020309-resume-a";
    auto FirstPlan = BuildPlannerService::CreatePlan(*Resolved, FirstPlanner);
    REQUIRE(FirstPlan);

    TestBuildNodeExecutor FirstExecutor{};
    FirstExecutor.EnableFailure = true;
    FirstExecutor.FailureType = EBuildNodeType::BuildCode;

    BuildExecutionOptions FirstOptions{};
    FirstOptions.NodeExecutor = &FirstExecutor;
    FirstOptions.EnablePersistentNodeCache = false;

    auto FirstReport = BuildExecutionService::Execute(*Resolved, *FirstPlan, FirstOptions);
    REQUIRE(FirstReport);
    CHECK(FirstReport->Status == EBuildExecutionStatus::Failed);

    BuildPlannerOptions SecondPlanner{};
    SecondPlanner.BuildId = "20260322-020310-resume-b";
    auto SecondPlan = BuildPlannerService::CreatePlan(*Resolved, SecondPlanner);
    REQUIRE(SecondPlan);

    TestBuildNodeExecutor SecondExecutor{};
    BuildExecutionOptions SecondOptions{};
    SecondOptions.NodeExecutor = &SecondExecutor;
    SecondOptions.EnablePersistentNodeCache = false;
    SecondOptions.ResumeBaselineReport = std::addressof(*FirstReport);

    auto SecondReport = BuildExecutionService::Execute(*Resolved, *SecondPlan, SecondOptions);
    REQUIRE(SecondReport);
    CHECK(SecondReport->Status == EBuildExecutionStatus::Succeeded);
    CHECK(std::ranges::any_of(SecondReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.CacheHit && Record.Type == EBuildNodeType::ConfigureCMake; }));
    CHECK(std::ranges::none_of(SecondReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                               { return Record.Type == EBuildNodeType::BuildCode && Record.CacheHit; }));
    CHECK(std::filesystem::exists(SecondReport->StageDirectory / "Metadata" / "PackageManifest.json"));
}
