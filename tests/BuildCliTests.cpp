#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <ranges>

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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_build_cli_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Error{};
            std::filesystem::remove_all(Path, Error);
        }
    };

} // namespace

TEST_CASE("BuildCliService creates workspaces and adds modules", "[Build][CLI]")
{
    TempDir Root{};

    BuildCliOptions Options{};
    Options.CurrentWorkingDirectory = Root.Path;

    const std::filesystem::path ProjectRoot = Root.Path / "CliGame";
    const BuildCliResult CreateProjectResult =
        BuildCliService::Run({"build", "create-project", "--name", "CliGame", "--dest", ProjectRoot.string(),
                              "--display-name", "CLI Game", "--editor-module"},
                             Options);
    REQUIRE(CreateProjectResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE_FALSE(CreateProjectResult.ArtifactPaths.empty());

    const std::filesystem::path ProjectFile = ProjectRoot / "project.snproj.json";
    REQUIRE(std::filesystem::exists(ProjectFile));

    const BuildCliResult AddModuleResult =
        BuildCliService::Run({"add-module", "--project", ProjectFile.string(), "--name", "GameplaySystems", "--type",
                              "runtime", "--public-dep", "SnAPI.GameFramework", "--define", "CLI_TEST_BUILD"},
                             Options);
    REQUIRE(AddModuleResult.ExitCode == EBuildCliExitCode::Success);
    CHECK(std::filesystem::exists(ProjectRoot / "Modules" / "GameplaySystems" / "CMakeLists.txt"));

    const std::filesystem::path PluginRoot = Root.Path / "CliPlugin";
    const BuildCliResult CreatePluginResult =
        BuildCliService::Run({"create-plugin", "--name", "CliPlugin", "--dest", PluginRoot.string(),
                              "--display-name", "CLI Plugin", "--editor-module"},
                             Options);
    REQUIRE(CreatePluginResult.ExitCode == EBuildCliExitCode::Success);

    const std::filesystem::path PluginFile = PluginRoot / "plugin.snplugin.json";
    REQUIRE(std::filesystem::exists(PluginFile));

    const BuildCliResult AddPluginModuleResult =
        BuildCliService::Run({"add-module", "--plugin", PluginFile.string(), "--name", "CliPluginTools", "--type",
                              "editor", "--private-dep", "CliPlugin"},
                             Options);
    REQUIRE(AddPluginModuleResult.ExitCode == EBuildCliExitCode::Success);
    CHECK(std::filesystem::exists(PluginRoot / "Modules" / "CliPluginTools" / "CMakeLists.txt"));
}

TEST_CASE("BuildCliService validates and plans build requests", "[Build][CLI]")
{
    TempDir Root{};

    BuildCliOptions Options{};
    Options.CurrentWorkingDirectory = Root.Path;

    const std::filesystem::path ProjectRoot = Root.Path / "CliValidationGame";
    const BuildCliResult CreateProjectResult =
        BuildCliService::Run({"create-project", "--name", "CliValidationGame", "--dest", ProjectRoot.string()}, Options);
    REQUIRE(CreateProjectResult.ExitCode == EBuildCliExitCode::Success);

    const std::filesystem::path ProjectFile = ProjectRoot / "project.snproj.json";
    REQUIRE(std::filesystem::exists(ProjectFile));

    const BuildCliResult ValidateResult =
        BuildCliService::Run({"validate", "--project", ProjectFile.string(), "--platform", "Windows", "--config",
                              "Development", "--container", "docker://snapi/windows-msvc:2026.03", "--build-id",
                              "20260322-130001-validate"},
                             Options);
    REQUIRE(ValidateResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(ValidateResult.PlannedGraph.has_value());
    CHECK(ValidateResult.PlannedGraph->BuildId == "20260322-130001-validate");

    const BuildCliResult PlanOnlyResult =
        BuildCliService::Run({"package", "--project", ProjectFile.string(), "--platform", "Windows", "--config",
                              "Development", "--container", "docker://snapi/windows-msvc:2026.03", "--skip-code",
                              "--skip-assets", "--plan-only", "--build-id", "20260322-130002-plan"},
                             Options);
    REQUIRE(PlanOnlyResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(PlanOnlyResult.PlannedGraph.has_value());
    CHECK(PlanOnlyResult.PlannedGraph->BuildId == "20260322-130002-plan");
    CHECK_FALSE(PlanOnlyResult.ExecutionReport.has_value());
}

TEST_CASE("BuildCliService blocks validation when the project startup level is missing", "[Build][CLI]")
{
    TempDir Root{};

    BuildCliOptions Options{};
    Options.CurrentWorkingDirectory = Root.Path;

    const std::filesystem::path ProjectRoot = Root.Path / "CliInvalidStartupGame";
    const BuildCliResult CreateProjectResult =
        BuildCliService::Run({"create-project", "--name", "CliInvalidStartupGame", "--dest", ProjectRoot.string()}, Options);
    REQUIRE(CreateProjectResult.ExitCode == EBuildCliExitCode::Success);

    const std::filesystem::path ProjectFile = ProjectRoot / "project.snproj.json";
    const std::filesystem::path StartupLevelPath = ProjectRoot / "Assets" / "Levels" / "StarterLevel.level";
    std::error_code Error{};
    REQUIRE(std::filesystem::exists(StartupLevelPath));
    std::filesystem::remove(StartupLevelPath, Error);
    REQUIRE_FALSE(Error);

    const BuildCliResult ValidateResult =
        BuildCliService::Run({"validate", "--project", ProjectFile.string(), "--platform", "Windows", "--config",
                              "Development"},
                             Options);
    REQUIRE(ValidateResult.ExitCode == EBuildCliExitCode::ValidationFailed);
    CHECK(ValidateResult.StandardError.find("startup level asset") != std::string::npos);
}

TEST_CASE("BuildCliService executes package builds and promotes final outputs", "[Build][CLI]")
{
    TempDir Root{};

    BuildCliOptions Options{};
    Options.CurrentWorkingDirectory = Root.Path;

    const std::filesystem::path ProjectRoot = Root.Path / "CliPackageGame";
    const BuildCliResult CreateProjectResult =
        BuildCliService::Run({"create-project", "--name", "CliPackageGame", "--dest", ProjectRoot.string()}, Options);
    REQUIRE(CreateProjectResult.ExitCode == EBuildCliExitCode::Success);

    const std::filesystem::path ProjectFile = ProjectRoot / "project.snproj.json";
    const std::filesystem::path PackageRoot = Root.Path / "PackageOutput";

    const BuildCliResult PackageResult =
        BuildCliService::Run({"package", "--project", ProjectFile.string(), "--platform", "Windows", "--config",
                              "Development", "--container", "docker://snapi/windows-msvc:2026.03", "--skip-code",
                              "--skip-assets", "--dest", PackageRoot.string(), "--archive", "--build-id",
                              "20260322-130003-package"},
                             Options);
    REQUIRE(PackageResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(PackageResult.ExecutionReport.has_value());

    CHECK(PackageResult.ExecutionReport->Status == EBuildExecutionStatus::Succeeded);
    CHECK(std::filesystem::exists(PackageResult.ExecutionReport->BuildReportFilePath));
    CHECK(std::filesystem::exists(PackageResult.ExecutionReport->PackageDirectoryPath));
    CHECK(std::filesystem::exists(PackageResult.ExecutionReport->ArchiveFilePath));
    CHECK(PackageResult.ExecutionReport->PackageOutputRootDirectory == PackageRoot.lexically_normal());
}

TEST_CASE("BuildCliService supports structured history inspection and retry flows", "[Build][CLI]")
{
    TempDir Root{};

    BuildCliOptions Options{};
    Options.CurrentWorkingDirectory = Root.Path;
    Options.Execution.EnablePersistentNodeCache = false;

    const std::filesystem::path ProjectRoot = Root.Path / "CliHistoryGame";
    const BuildCliResult CreateProjectResult =
        BuildCliService::Run({"create-project", "--name", "CliHistoryGame", "--dest", ProjectRoot.string()}, Options);
    REQUIRE(CreateProjectResult.ExitCode == EBuildCliExitCode::Success);

    const std::filesystem::path ProjectFile = ProjectRoot / "project.snproj.json";
    REQUIRE(std::filesystem::exists(ProjectFile));

    const BuildCliResult FirstPackageResult =
        BuildCliService::Run({"package", "--project", ProjectFile.string(), "--platform", "Windows", "--config",
                              "Development", "--container", "docker://snapi/windows-msvc:2026.03", "--skip-code",
                              "--skip-assets", "--build-id", "20260322-130004-history-a"},
                             Options);
    REQUIRE(FirstPackageResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(FirstPackageResult.ExecutionReport.has_value());

    const BuildCliResult RetryResult =
        BuildCliService::Run({"retry", "--project", ProjectFile.string(), "--from-build-id",
                              "20260322-130004-history-a", "--skip-code", "--skip-assets", "--build-id",
                              "20260322-130005-history-b", "--json", "--json-events"},
                             Options);
    REQUIRE(RetryResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(RetryResult.ExecutionReport.has_value());
    CHECK(std::ranges::any_of(RetryResult.ExecutionReport->NodeRecords, [](const BuildNodeExecutionRecord& Record)
                              { return Record.CacheHit; }));

    const nlohmann::ordered_json RetryJson = nlohmann::ordered_json::parse(RetryResult.StandardOutput, nullptr, false);
    REQUIRE_FALSE(RetryJson.is_discarded());
    CHECK(RetryJson["Command"] == "retry");
    CHECK(RetryJson["Report"]["BuildId"] == "20260322-130005-history-b");
    CHECK(RetryJson["RebuildAll"] == false);
    CHECK(RetryJson["Events"].is_array());

    const BuildCliResult RebuildAllRetryResult =
        BuildCliService::Run({"retry", "--project", ProjectFile.string(), "--from-build-id",
                              "20260322-130004-history-a", "--skip-code", "--skip-assets", "--build-id",
                              "20260322-130006-history-c", "--rebuild-all"},
                             Options);
    REQUIRE(RebuildAllRetryResult.ExitCode == EBuildCliExitCode::Success);
    REQUIRE(RebuildAllRetryResult.ExecutionReport.has_value());
    CHECK(std::ranges::none_of(RebuildAllRetryResult.ExecutionReport->NodeRecords,
                               [](const BuildNodeExecutionRecord& Record) { return Record.CacheHit; }));

    const BuildCliResult HistoryListResult =
        BuildCliService::Run({"history", "list", "--project", ProjectFile.string(), "--json"}, Options);
    REQUIRE(HistoryListResult.ExitCode == EBuildCliExitCode::Success);
    const nlohmann::ordered_json HistoryListJson =
        nlohmann::ordered_json::parse(HistoryListResult.StandardOutput, nullptr, false);
    REQUIRE_FALSE(HistoryListJson.is_discarded());
    CHECK(HistoryListJson["Entries"].is_array());
    CHECK(HistoryListJson["Entries"].size() >= 2u);

    const BuildCliResult HistoryShowResult =
        BuildCliService::Run({"history", "show", "--project", ProjectFile.string(), "--build-id",
                              "20260322-130005-history-b", "--json"},
                             Options);
    REQUIRE(HistoryShowResult.ExitCode == EBuildCliExitCode::Success);
    const nlohmann::ordered_json HistoryShowJson =
        nlohmann::ordered_json::parse(HistoryShowResult.StandardOutput, nullptr, false);
    REQUIRE_FALSE(HistoryShowJson.is_discarded());
    CHECK(HistoryShowJson["Report"]["BuildId"] == "20260322-130005-history-b");

    const BuildCliResult HistoryCompareResult =
        BuildCliService::Run({"history", "compare", "--project", ProjectFile.string(), "--left",
                              "20260322-130004-history-a", "--right", "20260322-130005-history-b", "--json"},
                             Options);
    REQUIRE(HistoryCompareResult.ExitCode == EBuildCliExitCode::Success);
    const nlohmann::ordered_json HistoryCompareJson =
        nlohmann::ordered_json::parse(HistoryCompareResult.StandardOutput, nullptr, false);
    REQUIRE_FALSE(HistoryCompareJson.is_discarded());
    CHECK(HistoryCompareJson["LeftBuildId"] == "20260322-130004-history-a");
    CHECK(HistoryCompareJson["RightBuildId"] == "20260322-130005-history-b");
}
