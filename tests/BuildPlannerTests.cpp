#include <algorithm>
#include <chrono>
#include <filesystem>
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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_build_planner_test_" + Stamp);
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
     * @brief Create one project descriptor on disk for planner tests.
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

} // namespace

TEST_CASE("BuildPlannerService creates a deterministic staged graph for resolved profile requests", "[Build][Planner]")
{
    TempDir Root{};

    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = SetValue(std::string("Windows"));
    WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
    WindowsDevelopment.SelectedLevels.IsSet = true;
    WindowsDevelopment.SelectedLevels.Values = {"Levels/Main.level"};
    WindowsDevelopment.IncludeFolders.IsSet = true;
    WindowsDevelopment.IncludeFolders.Values = {"Assets/Shared"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "PlannerHost", {WindowsDevelopment});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions Options{};
    Options.BuildId = "20260322-010203-abcdef12";

    auto FirstPlan = BuildPlannerService::CreatePlan(*Resolved, Options);
    REQUIRE(FirstPlan);
    auto SecondPlan = BuildPlannerService::CreatePlan(*Resolved, Options);
    REQUIRE(SecondPlan);

    CHECK(FirstPlan->BuildId == "20260322-010203-abcdef12");
    CHECK(FirstPlan->RequestHash == Resolved->RequestHash);
    CHECK(FirstPlan->HistoryDirectory ==
          (Resolved->Project.SavedRootDirectory / "BuildHistory" / Options.BuildId).lexically_normal());
    CHECK(FirstPlan->StageDirectory ==
          (FirstPlan->HistoryDirectory / "Stage" / "PlannerHost_WindowsDevelopment_Windows_Development")
              .lexically_normal());
    REQUIRE(FirstPlan->Nodes.size() == 18u);

    const BuildGraphNode& LoadProject = FirstPlan->Nodes.front();
    CHECK(LoadProject.Id == 1u);
    CHECK(LoadProject.Stage == EBuildStage::Preflight);
    CHECK(LoadProject.Type == EBuildNodeType::LoadProject);
    CHECK(LoadProject.Name == "Load Project Descriptor");
    CHECK_FALSE(LoadProject.Cacheable);

    const BuildGraphNode& ConfigureCMake = FirstPlan->Nodes.at(6);
    CHECK(ConfigureCMake.Type == EBuildNodeType::ConfigureCMake);
    CHECK(ConfigureCMake.Dependencies == std::vector<std::uint32_t>{3u, 6u});
    CHECK(ConfigureCMake.Outputs ==
          std::vector<std::string>{
              (Resolved->Project.IntermediateRootDirectory / "Build" / "windows" / "development" /
               "docker_snapi_windows-msvc_2026_03" / "CMakeCache.txt")
                  .lexically_normal()
                  .generic_string()});

    const BuildGraphNode& WriteBuildReport = FirstPlan->Nodes.back();
    CHECK(WriteBuildReport.Stage == EBuildStage::Finalize);
    CHECK(WriteBuildReport.Type == EBuildNodeType::WriteBuildReport);
    CHECK(WriteBuildReport.Dependencies == std::vector<std::uint32_t>{11u, 17u});
    CHECK_FALSE(WriteBuildReport.Cacheable);

    const auto ValidationIssues = BuildPlannerService::Validate(*FirstPlan);
    CHECK(ValidationIssues.empty());

    auto SerializedResult = BuildPlannerService::Serialize(*FirstPlan, 2);
    REQUIRE(SerializedResult);
    const nlohmann::ordered_json Serialized = nlohmann::ordered_json::parse(*SerializedResult, nullptr, false);
    REQUIRE_FALSE(Serialized.is_discarded());
    CHECK(Serialized["BuildId"] == Options.BuildId);
    CHECK(Serialized["RequestHash"] == Resolved->RequestHash);
    REQUIRE(Serialized["Nodes"].is_array());
    CHECK(Serialized["Nodes"].size() == FirstPlan->Nodes.size());
    CHECK(Serialized["Nodes"][0]["Type"] == "LoadProject");

    auto SecondSerializedResult = BuildPlannerService::Serialize(*SecondPlan, 2);
    REQUIRE(SecondSerializedResult);
    CHECK(*SerializedResult == *SecondSerializedResult);
}

TEST_CASE("BuildPlannerService validates malformed graphs", "[Build][Planner]")
{
    BuildGraph Invalid{};
    Invalid.BuildId = "broken";
    Invalid.Nodes = {
        BuildGraphNode{
            .Id = 1u,
            .Stage = EBuildStage::Finalize,
            .Type = EBuildNodeType::WriteBuildReport,
            .Name = "",
            .Dependencies = {2u},
            .Inputs = {},
            .Outputs = {},
            .CacheKey = "broken:report",
            .Cacheable = false,
        },
    };

    const auto Issues = BuildPlannerService::Validate(Invalid);
    REQUIRE_FALSE(Issues.empty());

    auto HasRule = [&](const std::string_view RuleId)
    {
        return std::ranges::any_of(Issues, [&](const BuildValidationIssue& Issue) { return Issue.RuleId == RuleId; });
    };

    CHECK(HasRule("BuildPlan.RequestHashMissing"));
    CHECK(HasRule("BuildPlan.HistoryDirectoryMissing"));
    CHECK(HasRule("BuildPlan.StageDirectoryMissing"));
    CHECK(HasRule("BuildPlan.NodeNameMissing"));
    CHECK(HasRule("BuildPlan.NodeDependencyMissing"));
}
