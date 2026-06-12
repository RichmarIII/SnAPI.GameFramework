#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_build_history_test_" + Stamp);
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
     * @brief Create one project descriptor on disk for build-history tests.
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
     * @brief Test executor that can synthesize a targeted node failure.
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

            return BuildNodeExecutionResult{
                .CacheHit = Node.Type == EBuildNodeType::LoadProject,
                .Message = Node.Type == EBuildNodeType::LoadProject ? "Loaded from synthetic cache."
                                                                    : "Executed by synthetic test executor.",
                .Outputs = Node.Outputs,
            };
        }
    };

    /**
     * @brief Execute one build request and return the final report.
     * @param ProjectFile Project descriptor file path.
     * @param ProfileName Authored build profile name.
     * @param BuildId Stable test build id.
     * @param Options Optional execution options.
     * @return Final execution report.
     */
    [[nodiscard]] BuildExecutionReport ExecuteBuild(const std::filesystem::path& ProjectFile,
                                                    const std::string_view ProfileName,
                                                    const std::string_view BuildId,
                                                    const BuildExecutionOptions& Options = {})
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

        auto Report = BuildExecutionService::Execute(*Resolved, *Plan, Options);
        if (!Report)
        {
            throw std::runtime_error(Report.error().Message);
        }

        return *Report;
    }

} // namespace

TEST_CASE("BuildHistoryService lists complete and incomplete build runs", "[Build][History]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "HistoryHost", {WindowsDevelopment});
    const BuildExecutionReport CompleteReport =
        ExecuteBuild(ProjectFile, "WindowsDevelopment", "20260322-030001-complete");

    const std::filesystem::path IncompleteHistoryDirectory =
        CompleteReport.HistoryDirectory.parent_path() / "20260322-030002-incomplete";
    std::filesystem::create_directories(IncompleteHistoryDirectory);
    std::ofstream(IncompleteHistoryDirectory / "BuildRequest.json") << "{\n  \"BuildId\": \"20260322-030002-incomplete\"\n}\n";

    auto Entries = BuildHistoryService::List(CompleteReport.HistoryDirectory.parent_path().parent_path());
    REQUIRE(Entries);
    REQUIRE(Entries->size() == 2u);

    CHECK((*Entries)[0].BuildId == "20260322-030002-incomplete");
    CHECK((*Entries)[0].State == EBuildHistoryEntryState::Incomplete);
    CHECK((*Entries)[1].BuildId == "20260322-030001-complete");
    CHECK((*Entries)[1].State == EBuildHistoryEntryState::Complete);
    CHECK((*Entries)[1].Status == EBuildExecutionStatus::Succeeded);
    CHECK((*Entries)[1].NodeCount == CompleteReport.NodeRecords.size());
    CHECK((*Entries)[1].OutputFileCount == CompleteReport.OutputFiles.size());

    BuildHistoryListOptions CompleteOnly{};
    CompleteOnly.IncludeIncomplete = false;
    auto CompleteEntries = BuildHistoryService::List(CompleteReport.HistoryDirectory.parent_path().parent_path(),
                                                     CompleteOnly);
    REQUIRE(CompleteEntries);
    REQUIRE(CompleteEntries->size() == 1u);
    CHECK(CompleteEntries->front().BuildId == "20260322-030001-complete");
}

TEST_CASE("BuildHistoryService loads reports and compares build outcomes", "[Build][History]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "HistoryCompareHost", {WindowsDevelopment});

    const BuildExecutionReport SuccessfulReport =
        ExecuteBuild(ProjectFile, "WindowsDevelopment", "20260322-030101-success");

    TestBuildNodeExecutor FailingExecutor{};
    FailingExecutor.EnableFailure = true;
    FailingExecutor.FailureType = EBuildNodeType::BuildCode;

    BuildExecutionOptions FailureOptions{};
    FailureOptions.NodeExecutor = &FailingExecutor;
    FailureOptions.EnablePersistentNodeCache = false;
    const BuildExecutionReport FailedReport =
        ExecuteBuild(ProjectFile, "WindowsDevelopment", "20260322-030102-failed", FailureOptions);

    auto LoadedSuccessful = BuildHistoryService::LoadReport(SuccessfulReport.BuildReportFilePath);
    auto LoadedFailed = BuildHistoryService::LoadReport(FailedReport.BuildReportFilePath);
    REQUIRE(LoadedSuccessful);
    REQUIRE(LoadedFailed);

    CHECK(LoadedSuccessful->BuildId == SuccessfulReport.BuildId);
    CHECK(LoadedSuccessful->Status == EBuildExecutionStatus::Succeeded);
    CHECK(LoadedSuccessful->PackageDirectoryPath == SuccessfulReport.PackageDirectoryPath);
    CHECK(LoadedFailed->BuildId == FailedReport.BuildId);
    CHECK(LoadedFailed->Status == EBuildExecutionStatus::Failed);

    const BuildHistoryComparison Comparison = BuildHistoryService::Compare(*LoadedFailed, *LoadedSuccessful);
    CHECK(Comparison.LeftBuildId == FailedReport.BuildId);
    CHECK(Comparison.RightBuildId == SuccessfulReport.BuildId);
    CHECK(Comparison.SameRequestHash);
    CHECK_FALSE(Comparison.SameStatus);
    CHECK_FALSE(Comparison.AddedOutputFiles.empty());
    CHECK(std::ranges::any_of(Comparison.NodeDeltas, [](const BuildHistoryNodeDelta& Delta)
                              { return Delta.Type == EBuildNodeType::BuildCode &&
                                       Delta.LeftStatus == EBuildNodeExecutionStatus::Failed &&
                                       Delta.RightStatus == EBuildNodeExecutionStatus::Succeeded; }));
}
