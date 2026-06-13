#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

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
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_project_creation_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

/**
 * @brief Write one UTF-8 test file, creating parent directories as needed.
 * @param Path File path to write.
 * @param Text Full file contents.
 */
void WriteTextFile(const std::filesystem::path& Path, const std::string& Text)
{
    std::error_code Ec{};
    std::filesystem::create_directories(Path.parent_path(), Ec);
    REQUIRE_FALSE(Ec);

    std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
    REQUIRE(Out.is_open());
    Out.write(Text.data(), static_cast<std::streamsize>(Text.size()));
    REQUIRE(Out.good());
}

/**
 * @brief Read one UTF-8 test file fully into memory.
 * @param Path File path to read.
 * @return File contents.
 */
[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    CHECK(Input.is_open());
    if (!Input.is_open())
    {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("ProjectCreationService creates a default project workspace", "[Project][Create]")
{
    TempDir Root{};

    ProjectCreationRequest Request{};
    Request.ProjectName = "BlankGame";
    Request.ParentDirectory = Root.Path;

    ProjectCreationResult Result{};
    REQUIRE(ProjectCreationService::CreateProject(Request, &Result));

    const std::filesystem::path ProjectRoot = Root.Path / "BlankGame";
    CHECK(Result.Project.ProjectRootDirectory.lexically_normal() == ProjectRoot.lexically_normal());
    CHECK(std::filesystem::exists(Result.Project.ProjectFilePath));
    CHECK(std::filesystem::exists(Result.Project.AssetRootDirectory));
    CHECK(std::filesystem::exists(Result.Project.CodeRootDirectory));
    CHECK(std::filesystem::exists(Result.Project.ConfigRootDirectory));
    CHECK(std::filesystem::exists(Result.Project.IntermediateRootDirectory));
    CHECK(std::filesystem::exists(Result.Project.SavedRootDirectory));
    CHECK(std::filesystem::exists(Result.StartupLevelAssetPath));
    CHECK(std::filesystem::exists(Result.ProjectCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedProjectModulesCMakePath));
    CHECK(std::filesystem::exists(Result.RuntimeModuleDirectory));
    CHECK(std::filesystem::exists(Result.RuntimeModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.RuntimeModuleCMakePath));
    CHECK(std::filesystem::exists(Result.DefaultGameConfigPath));
    CHECK(Result.StarterScriptPath.empty());
    CHECK(Result.ShaderDirectory.empty());
    CHECK(Result.GeneratedFiles.size() == 11);
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(), Result.ProjectCodeRootCMakePath)
          != Result.GeneratedFiles.end());
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(), Result.GeneratedProjectModulesCMakePath)
          != Result.GeneratedFiles.end());
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(), Result.RuntimeModuleRootCMakePath)
          != Result.GeneratedFiles.end());
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(), Result.RuntimeModuleCMakePath)
          != Result.GeneratedFiles.end());
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(), Result.DefaultGameConfigPath)
          != Result.GeneratedFiles.end());

    CHECK(Result.Project.Descriptor.Project.Name == "BlankGame");
    CHECK(Result.Project.Descriptor.Project.DisplayName == "BlankGame");
    CHECK(Result.Project.Descriptor.Paths.AssetRoot == "Assets");
    CHECK(Result.Project.Descriptor.Startup.StartupLevelAsset == "Levels/StarterLevel.level");
    CHECK(Result.Project.Descriptor.Startup.DefaultGameClass == "BlankGame::BlankGame");
    CHECK(Result.Project.Descriptor.Startup.DefaultGameModeClass == "BlankGame::BlankGameMode");
    REQUIRE(Result.Project.Descriptor.Modules.size() == 1);
    CHECK(Result.Project.Descriptor.Modules.front().Name == "BlankGame");
    CHECK(Result.Project.Descriptor.Modules.front().Root == "Modules/BlankGame");
    CHECK(Result.Project.Descriptor.Modules.front().Type == EProjectModuleType::Runtime);
    CHECK(Result.Project.Descriptor.Modules.front().PublicDependencies == std::vector<std::string>{"SnAPI.GameFramework"});

    CHECK(ReadTextFile(Result.ProjectCodeRootCMakePath)
              .find("include(\"${SNAPI_PROJECT_ROOT_DIR}/Intermediate/Build/Generated/ProjectModules.cmake\" OPTIONAL)")
          != std::string::npos);
    CHECK(ReadTextFile(Result.GeneratedProjectModulesCMakePath)
              .find("add_subdirectory(\"${SNAPI_PROJECT_ROOT_DIR}/Modules/BlankGame\")")
          != std::string::npos);
    CHECK(ReadTextFile(Result.GeneratedProjectModulesCMakePath)
              .find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE BlankGame)")
          != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleRootCMakePath).find("include(\"${CMAKE_CURRENT_LIST_DIR}/BlankGame.CMakeLists.txt\")")
          != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleCMakePath).find("add_library(BlankGame") != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleDirectory / "Public" / "BlankGame" / "BlankGameGame.h")
              .find("class BlankGame final")
          != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleDirectory / "Public" / "BlankGame" / "BlankGameGameMode.h")
              .find("class BlankGameMode final")
          != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleDirectory / "Private" / "BlankGameGame.cpp")
              .find("std::make_unique<BlankGameMode>()")
          != std::string::npos);

    const nlohmann::ordered_json DefaultGameConfig =
        nlohmann::ordered_json::parse(ReadTextFile(Result.DefaultGameConfigPath), nullptr, false);
    REQUIRE_FALSE(DefaultGameConfig.is_discarded());
    CHECK(DefaultGameConfig["ProjectName"] == "BlankGame");
    CHECK(DefaultGameConfig["DefaultGameClass"] == "BlankGame::BlankGame");
    CHECK(DefaultGameConfig["DefaultGameModeClass"] == "BlankGame::BlankGameMode");

    LevelAsset StartupLevel{};
    REQUIRE(DeserializeAuthoredAssetFromJson(ReadTextFile(Result.StartupLevelAssetPath), StartupLevel));
    CHECK(StartupLevel.Name.empty());
    CHECK(StartupLevel.Nodes.empty());
}

TEST_CASE("ProjectCreationService can generate a companion editor module", "[Project][Create]")
{
    TempDir Root{};

    ProjectCreationRequest Request{};
    Request.ProjectName = "EditorReadyGame";
    Request.ParentDirectory = Root.Path;
    Request.Code.CreateStarterEditorModule = true;

    ProjectCreationResult Result{};
    REQUIRE(ProjectCreationService::CreateProject(Request, &Result));

    REQUIRE(Result.Project.Descriptor.Modules.size() == 2);
    CHECK(std::filesystem::exists(Result.EditorModuleDirectory));
    CHECK(std::filesystem::exists(Result.EditorModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.EditorModuleCMakePath));
    CHECK(Result.GeneratedFiles.size() == 15);

    const ProjectModuleDescriptor& RuntimeModule = Result.Project.Descriptor.Modules[0];
    const ProjectModuleDescriptor& EditorModule = Result.Project.Descriptor.Modules[1];

    CHECK(RuntimeModule.Name == "EditorReadyGame");
    CHECK(RuntimeModule.Type == EProjectModuleType::Runtime);
    CHECK(EditorModule.Name == "EditorReadyGameEditor");
    CHECK(EditorModule.Type == EProjectModuleType::Editor);
    CHECK(EditorModule.Root == "Modules/EditorReadyGameEditor");
    CHECK(EditorModule.LoadInEditor);
    CHECK_FALSE(EditorModule.LoadInRuntime);
    CHECK(EditorModule.PrivateDependencies == std::vector<std::string>{"EditorReadyGame", "SnAPI.GameFramework"});

    const std::string GeneratedProjectModules = ReadTextFile(Result.GeneratedProjectModulesCMakePath);
    CHECK(GeneratedProjectModules.find("add_subdirectory(\"${SNAPI_PROJECT_ROOT_DIR}/Modules/EditorReadyGame\")")
          != std::string::npos);
    CHECK(GeneratedProjectModules.find("add_subdirectory(\"${SNAPI_PROJECT_ROOT_DIR}/Modules/EditorReadyGameEditor\")")
          != std::string::npos);
    CHECK(GeneratedProjectModules.find("target_link_libraries(SnAPI.GameFramework.Editor PRIVATE EditorReadyGame)")
          != std::string::npos);
    CHECK(GeneratedProjectModules.find("target_link_libraries(SnAPI.GameFramework.Editor PRIVATE EditorReadyGameEditor)")
          != std::string::npos);
    CHECK(GeneratedProjectModules.find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE EditorReadyGame)")
          != std::string::npos);
    CHECK(GeneratedProjectModules.find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE EditorReadyGameEditor)")
          == std::string::npos);

    CHECK(ReadTextFile(Result.EditorModuleRootCMakePath)
              .find("include(\"${CMAKE_CURRENT_LIST_DIR}/EditorReadyGameEditor.CMakeLists.txt\")")
          != std::string::npos);
    CHECK(ReadTextFile(Result.EditorModuleCMakePath)
              .find("target_link_libraries(EditorReadyGameEditor PRIVATE\n    EditorReadyGame\n    SnAPI.GameFramework\n)")
          != std::string::npos);
    CHECK(ReadTextFile(Result.EditorModuleDirectory / "Public" / "EditorReadyGameEditor" /
                       "EditorReadyGameEditorModule.h")
              .find("RegisterEditorServices(SnAPI::GameFramework::Editor::EditorServiceContext& Context)")
          != std::string::npos);
}

TEST_CASE("ProjectCreationService copies optional starter resources", "[Project][Create]")
{
    TempDir Root{};

    LevelAsset StarterLevel{};
    auto StarterLevelJson = SerializeAuthoredAssetToJson(StarterLevel);
    REQUIRE(StarterLevelJson);

    const std::filesystem::path TemplateRoot = Root.Path / "Templates";
    const std::filesystem::path StarterLevelTemplate = TemplateRoot / "Starter.level";
    const std::filesystem::path StarterScriptTemplate = TemplateRoot / "Scripts" / "starter.lua";
    const std::filesystem::path ShaderTemplateDirectory = TemplateRoot / "Shaders";
    WriteTextFile(StarterLevelTemplate, *StarterLevelJson);
    WriteTextFile(StarterScriptTemplate, "-- starter script\n");
    WriteTextFile(ShaderTemplateDirectory / "Common" / "Test.slang", "// shader\n");

    ProjectCreationRequest Request{};
    Request.ProjectName = "TemplateGame";
    Request.ParentDirectory = Root.Path;
    Request.Templates.StarterLevelTemplateSourcePath = StarterLevelTemplate;
    Request.Templates.StarterScriptTemplateSourcePath = StarterScriptTemplate;
    Request.Templates.ShaderTemplateDirectory = ShaderTemplateDirectory;

    ProjectCreationResult Result{};
    REQUIRE(ProjectCreationService::CreateProject(Request, &Result));

    CHECK(std::filesystem::exists(Result.StartupLevelAssetPath));
    CHECK(std::filesystem::exists(Result.ProjectCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedProjectModulesCMakePath));
    CHECK(std::filesystem::exists(Result.StarterScriptPath));
    CHECK(std::filesystem::exists(Result.RuntimeModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.ShaderDirectory / "Common" / "Test.slang"));
    CHECK(std::filesystem::exists(Result.RuntimeModuleDirectory / "Private" / "TemplateGameModule.cpp"));
    CHECK(std::filesystem::exists(Result.DefaultGameConfigPath));
    CHECK(ReadTextFile(Result.StartupLevelAssetPath) == *StarterLevelJson);
    CHECK(ReadTextFile(Result.StarterScriptPath) == "-- starter script\n");
}

TEST_CASE("ProjectCreationService can skip starter runtime module generation", "[Project][Create]")
{
    TempDir Root{};

    ProjectCreationRequest Request{};
    Request.ProjectName = "ContentOnlyProject";
    Request.ParentDirectory = Root.Path;
    Request.Code.CreateStarterRuntimeModule = false;

    ProjectCreationResult Result{};
    REQUIRE(ProjectCreationService::CreateProject(Request, &Result));

    CHECK(std::filesystem::exists(Result.StartupLevelAssetPath));
    CHECK(std::filesystem::exists(Result.ProjectCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedProjectModulesCMakePath));
    CHECK(Result.Project.Descriptor.Modules.empty());
    CHECK(Result.Project.Descriptor.Startup.DefaultGameClass.empty());
    CHECK(Result.Project.Descriptor.Startup.DefaultGameModeClass.empty());
    CHECK(Result.RuntimeModuleDirectory.empty());
    CHECK(Result.RuntimeModuleRootCMakePath.empty());
    CHECK(Result.RuntimeModuleCMakePath.empty());
    CHECK(Result.DefaultGameConfigPath.empty());
    CHECK(Result.GeneratedFiles.size() == 2);

    CHECK(ReadTextFile(Result.GeneratedProjectModulesCMakePath).find("No project modules are currently declared.")
          != std::string::npos);
}
