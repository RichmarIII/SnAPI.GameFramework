#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_module_creation_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Ec{};
            std::filesystem::remove_all(Path, Ec);
        }
    };

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

    /**
     * @brief Create one content-only project used as the host for module-creation tests.
     * @param Root Temporary parent directory.
     * @param ProjectName Stable project name.
     * @return Project creation result.
     */
    [[nodiscard]] ProjectCreationResult CreateContentOnlyProject(const std::filesystem::path& Root,
                                                                 const std::string_view ProjectName)
    {
        ProjectCreationRequest Request{};
        Request.ProjectName = std::string(ProjectName);
        Request.ParentDirectory = Root;
        Request.Code.CreateStarterRuntimeModule = false;

        ProjectCreationResult CreatedProject{};
        const Result CreateResult = ProjectCreationService::CreateProject(Request, &CreatedProject);
        if (!CreateResult)
        {
            throw std::runtime_error(CreateResult.error().Message);
        }
        return CreatedProject;
    }

    /**
     * @brief Create one content-only plugin used as the host for plugin-module creation tests.
     * @param Root Temporary parent directory.
     * @param PluginName Stable plugin name.
     * @return Plugin creation result.
     */
    [[nodiscard]] PluginCreationResult CreateContentOnlyPlugin(const std::filesystem::path& Root,
                                                               const std::string_view PluginName)
    {
        PluginCreationRequest Request{};
        Request.PluginName = std::string(PluginName);
        Request.ParentDirectory = Root;
        Request.Code.CreateStarterRuntimeModule = false;

        PluginCreationResult CreatedPlugin{};
        const Result CreateResult = PluginCreationService::CreatePlugin(Request, &CreatedPlugin);
        if (!CreateResult)
        {
            throw std::runtime_error(CreateResult.error().Message);
        }
        return CreatedPlugin;
    }

} // namespace

TEST_CASE("ModuleCreationService creates a runtime module for an existing project", "[Project][Module]")
{
    TempDir Root{};
    const ProjectCreationResult Project = CreateContentOnlyProject(Root.Path, "ModuleHost");

    ModuleCreationRequest Request{};
    Request.ProjectFilePath = Project.Project.ProjectFilePath;
    Request.ModuleName = "GameplayCore";
    Request.ModuleType = EProjectModuleType::Runtime;

    ModuleCreationResult Result{};
    REQUIRE(ModuleCreationService::CreateModule(Request, &Result));

    CHECK(Result.Project.Descriptor.Startup.DefaultGameClass.empty());
    CHECK(Result.Project.Descriptor.Startup.DefaultGameModeClass.empty());
    REQUIRE(Result.Project.Descriptor.Modules.size() == 1);
    REQUIRE(Result.GeneratedFiles.size() == 6);
    CHECK(std::filesystem::exists(Result.ModuleDirectory));
    CHECK(std::filesystem::exists(Result.ModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.ModuleCMakePath));
    CHECK(std::filesystem::exists(Result.ModuleHeaderPath));
    CHECK(std::filesystem::exists(Result.ModuleSourcePath));
    CHECK(std::filesystem::exists(Result.ProjectCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedProjectModulesCMakePath));
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(), Result.ProjectCodeRootCMakePath) !=
          Result.GeneratedFiles.end());
    CHECK(std::find(Result.GeneratedFiles.begin(), Result.GeneratedFiles.end(),
                    Result.GeneratedProjectModulesCMakePath) != Result.GeneratedFiles.end());

    const ProjectModuleDescriptor& Module = Result.Project.Descriptor.Modules.front();
    CHECK(Module.Name == "GameplayCore");
    CHECK(Module.Type == EProjectModuleType::Runtime);
    CHECK(Module.Root == "Modules/GameplayCore");
    CHECK(Module.LoadInEditor);
    CHECK(Module.LoadInRuntime);
    CHECK(Module.PublicDependencies == std::vector<std::string>{"SnAPI.GameFramework"});
    CHECK(Module.PrivateDependencies.empty());

    CHECK(ReadTextFile(Result.ModuleRootCMakePath)
              .find("include(\"${CMAKE_CURRENT_LIST_DIR}/GameplayCore.CMakeLists.txt\")") != std::string::npos);
    CHECK(ReadTextFile(Result.ModuleCMakePath).find("add_library(GameplayCore") != std::string::npos);
    CHECK(ReadTextFile(Result.ModuleCMakePath)
              .find("target_link_libraries(GameplayCore PUBLIC\n    SnAPI.GameFramework\n)") != std::string::npos);
    CHECK(ReadTextFile(Result.ModuleHeaderPath).find("class GameplayCoreModule final") != std::string::npos);
    CHECK(ReadTextFile(Result.ModuleSourcePath).find("return \"GameplayCore\";") != std::string::npos);
    CHECK(ReadTextFile(Result.GeneratedProjectModulesCMakePath)
              .find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE GameplayCore)") != std::string::npos);
}

TEST_CASE("ModuleCreationService creates an editor module with custom descriptor settings", "[Project][Module]")
{
    TempDir Root{};
    const ProjectCreationResult Project = CreateContentOnlyProject(Root.Path, "ModuleHost");

    ModuleCreationRequest RuntimeRequest{};
    RuntimeRequest.ProjectFilePath = Project.Project.ProjectFilePath;
    RuntimeRequest.ModuleName = "GameplayCore";
    RuntimeRequest.ModuleType = EProjectModuleType::Runtime;
    REQUIRE(ModuleCreationService::CreateModule(RuntimeRequest));

    ModuleCreationRequest EditorRequest{};
    EditorRequest.ProjectFilePath = Project.Project.ProjectFilePath;
    EditorRequest.ModuleName = "GameplayToolsEditor";
    EditorRequest.ModuleType = EProjectModuleType::Editor;
    EditorRequest.NamespaceRoot = "Tooling";
    EditorRequest.PrivateDependencies = {"GameplayCore"};
    EditorRequest.PreprocessorDefinitions = {"SNAPI_TOOLING=1"};

    ModuleCreationResult Result{};
    REQUIRE(ModuleCreationService::CreateModule(EditorRequest, &Result));

    REQUIRE(Result.Project.Descriptor.Modules.size() == 2);
    CHECK(Result.Module.Name == "GameplayToolsEditor");
    CHECK(Result.Module.Type == EProjectModuleType::Editor);
    CHECK(Result.Module.Root == "Modules/GameplayToolsEditor");
    CHECK(Result.Module.LoadInEditor);
    CHECK_FALSE(Result.Module.LoadInRuntime);
    CHECK(Result.Module.PublicDependencies.empty());
    CHECK(Result.Module.PrivateDependencies == std::vector<std::string>{"GameplayCore", "SnAPI.GameFramework"});
    CHECK(Result.Module.PreprocessorDefinitions == std::vector<std::string>{"SNAPI_TOOLING=1"});

    const std::string ModuleHeader = ReadTextFile(Result.ModuleHeaderPath);
    const std::string ModuleSource = ReadTextFile(Result.ModuleSourcePath);
    const std::string ModuleCMake = ReadTextFile(Result.ModuleCMakePath);
    const std::string ProjectModulesCMake = ReadTextFile(Result.GeneratedProjectModulesCMakePath);

    CHECK(ModuleHeader.find("namespace Tooling") != std::string::npos);
    CHECK(ModuleHeader.find("class GameplayToolsEditorModule final") != std::string::npos);
    CHECK(ModuleHeader.find("RegisterEditorServices(SnAPI::GameFramework::Editor::EditorServiceContext& Context)") !=
          std::string::npos);
    CHECK(ModuleSource.find("void GameplayToolsEditorModule::RegisterEditorServices") != std::string::npos);
    CHECK(ModuleCMake.find(
              "target_link_libraries(GameplayToolsEditor PRIVATE\n    GameplayCore\n    SnAPI.GameFramework\n)") !=
          std::string::npos);
    CHECK(ModuleCMake.find("target_compile_definitions(GameplayToolsEditor PRIVATE\n    SNAPI_TOOLING=1\n)") !=
          std::string::npos);
    CHECK(ProjectModulesCMake.find("target_link_libraries(SnAPI.GameFramework.Editor PRIVATE GameplayToolsEditor)") !=
          std::string::npos);
    CHECK(ProjectModulesCMake.find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE GameplayToolsEditor)") ==
          std::string::npos);
}

TEST_CASE("ModuleCreationService can generate runtime gameplay bootstrap templates", "[Project][Module]")
{
    TempDir Root{};
    const ProjectCreationResult Project = CreateContentOnlyProject(Root.Path, "GameplayBootstrapHost");

    ModuleCreationRequest Request{};
    Request.ProjectFilePath = Project.Project.ProjectFilePath;
    Request.ModuleName = "CombatRuntime";
    Request.ModuleType = EProjectModuleType::Runtime;
    Request.GenerateGameplayBootstrap = true;

    ModuleCreationResult Result{};
    REQUIRE(ModuleCreationService::CreateModule(Request, &Result));

    REQUIRE(Result.GeneratedFiles.size() == 10);
    CHECK(std::filesystem::exists(Result.GameHeaderPath));
    CHECK(std::filesystem::exists(Result.GameSourcePath));
    CHECK(std::filesystem::exists(Result.GameModeHeaderPath));
    CHECK(std::filesystem::exists(Result.GameModeSourcePath));

    const std::string GameHeader = ReadTextFile(Result.GameHeaderPath);
    const std::string GameSource = ReadTextFile(Result.GameSourcePath);
    const std::string GameModeHeader = ReadTextFile(Result.GameModeHeaderPath);
    const std::string GameModeSource = ReadTextFile(Result.GameModeSourcePath);
    const std::string ModuleCMake = ReadTextFile(Result.ModuleCMakePath);

    CHECK(GameHeader.find("class CombatRuntimeGame final") != std::string::npos);
    CHECK(GameHeader.find("CreateInitialGameMode") != std::string::npos);
    CHECK(GameSource.find("return std::make_unique<CombatRuntimeGameMode>()") != std::string::npos);
    CHECK(GameModeHeader.find("class CombatRuntimeGameMode final") != std::string::npos);
    CHECK(GameModeSource.find("CombatRuntimeGameMode::Initialize") != std::string::npos);
    CHECK(ModuleCMake.find("Private/CombatRuntimeGame.cpp") != std::string::npos);
    CHECK(ModuleCMake.find("Private/CombatRuntimeGameMode.cpp") != std::string::npos);
}

TEST_CASE("ModuleCreationService rejects duplicate module names", "[Project][Module]")
{
    TempDir Root{};
    const ProjectCreationResult Project = CreateContentOnlyProject(Root.Path, "ModuleHost");

    ModuleCreationRequest Request{};
    Request.ProjectFilePath = Project.Project.ProjectFilePath;
    Request.ModuleName = "GameplayCore";
    Request.ModuleType = EProjectModuleType::Runtime;

    REQUIRE(ModuleCreationService::CreateModule(Request));

    auto DuplicateResult = ModuleCreationService::CreateModule(Request);
    REQUIRE_FALSE(DuplicateResult);
    CHECK(DuplicateResult.error().Code == EErrorCode::AlreadyExists);

    auto ResolvedProjectResult = ProjectDescriptorService::LoadResolved(Project.Project.ProjectFilePath.string());
    REQUIRE(ResolvedProjectResult);
    REQUIRE(ResolvedProjectResult->Descriptor.Modules.size() == 1);
    CHECK(ResolvedProjectResult->Descriptor.Modules.front().Name == "GameplayCore");
}

TEST_CASE("ModuleCreationService creates a runtime module for an existing plugin", "[Plugin][Module]")
{
    TempDir Root{};
    const PluginCreationResult Plugin = CreateContentOnlyPlugin(Root.Path, "InventoryHost");

    PluginModuleCreationRequest Request{};
    Request.PluginFilePath = Plugin.Plugin.PluginFilePath;
    Request.ModuleName = "InventoryRuntime";
    Request.ModuleType = EProjectModuleType::Runtime;

    PluginModuleCreationResult Result{};
    REQUIRE(ModuleCreationService::CreatePluginModule(Request, &Result));

    REQUIRE(Result.Plugin.Descriptor.Modules.size() == 1);
    REQUIRE(Result.GeneratedFiles.size() == 6);
    CHECK(std::filesystem::exists(Result.ModuleDirectory));
    CHECK(std::filesystem::exists(Result.ModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.ModuleCMakePath));
    CHECK(std::filesystem::exists(Result.ModuleHeaderPath));
    CHECK(std::filesystem::exists(Result.ModuleSourcePath));
    CHECK(std::filesystem::exists(Result.PluginCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedPluginModulesCMakePath));

    const ProjectModuleDescriptor& Module = Result.Plugin.Descriptor.Modules.front();
    CHECK(Module.Name == "InventoryRuntime");
    CHECK(Module.Type == EProjectModuleType::Runtime);
    CHECK(Module.Root == "Modules/InventoryRuntime");
    CHECK(Module.LoadInEditor);
    CHECK(Module.LoadInRuntime);
    CHECK(Module.PublicDependencies == std::vector<std::string>{"SnAPI.GameFramework"});
    CHECK(Module.PrivateDependencies.empty());

    CHECK(ReadTextFile(Result.ModuleRootCMakePath)
              .find("include(\"${CMAKE_CURRENT_LIST_DIR}/InventoryRuntime.CMakeLists.txt\")") != std::string::npos);
    CHECK(ReadTextFile(Result.ModuleCMakePath).find("add_library(InventoryRuntime") != std::string::npos);
    CHECK(ReadTextFile(Result.GeneratedPluginModulesCMakePath)
              .find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE InventoryRuntime)") !=
          std::string::npos);
}

TEST_CASE("ModuleCreationService creates an editor module for an existing plugin", "[Plugin][Module]")
{
    TempDir Root{};
    const PluginCreationResult Plugin = CreateContentOnlyPlugin(Root.Path, "InventoryHost");

    PluginModuleCreationRequest RuntimeRequest{};
    RuntimeRequest.PluginFilePath = Plugin.Plugin.PluginFilePath;
    RuntimeRequest.ModuleName = "InventoryRuntime";
    RuntimeRequest.ModuleType = EProjectModuleType::Runtime;
    REQUIRE(ModuleCreationService::CreatePluginModule(RuntimeRequest));

    PluginModuleCreationRequest EditorRequest{};
    EditorRequest.PluginFilePath = Plugin.Plugin.PluginFilePath;
    EditorRequest.ModuleName = "InventoryEditor";
    EditorRequest.ModuleType = EProjectModuleType::Editor;
    EditorRequest.NamespaceRoot = "InventoryTools";
    EditorRequest.PrivateDependencies = {"InventoryRuntime"};
    EditorRequest.PreprocessorDefinitions = {"SNAPI_INVENTORY_EDITOR=1"};

    PluginModuleCreationResult Result{};
    REQUIRE(ModuleCreationService::CreatePluginModule(EditorRequest, &Result));

    REQUIRE(Result.Plugin.Descriptor.Modules.size() == 2);
    CHECK(Result.Module.Name == "InventoryEditor");
    CHECK(Result.Module.Type == EProjectModuleType::Editor);
    CHECK(Result.Module.Root == "Modules/InventoryEditor");
    CHECK(Result.Module.LoadInEditor);
    CHECK_FALSE(Result.Module.LoadInRuntime);
    CHECK(Result.Module.PrivateDependencies == std::vector<std::string>{"InventoryRuntime", "SnAPI.GameFramework"});
    CHECK(Result.Module.PreprocessorDefinitions == std::vector<std::string>{"SNAPI_INVENTORY_EDITOR=1"});

    const std::string ModuleHeader = ReadTextFile(Result.ModuleHeaderPath);
    const std::string ModuleSource = ReadTextFile(Result.ModuleSourcePath);
    const std::string ModuleCMake = ReadTextFile(Result.ModuleCMakePath);
    const std::string PluginModulesCMake = ReadTextFile(Result.GeneratedPluginModulesCMakePath);

    CHECK(ModuleHeader.find("namespace InventoryTools") != std::string::npos);
    CHECK(ModuleHeader.find("class InventoryEditorModule final") != std::string::npos);
    CHECK(ModuleSource.find("void InventoryEditorModule::RegisterEditorServices") != std::string::npos);
    CHECK(ModuleCMake.find(
              "target_link_libraries(InventoryEditor PRIVATE\n    InventoryRuntime\n    SnAPI.GameFramework\n)") !=
          std::string::npos);
    CHECK(ModuleCMake.find("target_compile_definitions(InventoryEditor PRIVATE\n    SNAPI_INVENTORY_EDITOR=1\n)") !=
          std::string::npos);
    CHECK(PluginModulesCMake.find("target_link_libraries(SnAPI.GameFramework.Editor PRIVATE InventoryEditor)") !=
          std::string::npos);
    CHECK(PluginModulesCMake.find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE InventoryEditor)") ==
          std::string::npos);
}

TEST_CASE("ModuleCreationService rejects duplicate plugin module names", "[Plugin][Module]")
{
    TempDir Root{};
    const PluginCreationResult Plugin = CreateContentOnlyPlugin(Root.Path, "InventoryHost");

    PluginModuleCreationRequest Request{};
    Request.PluginFilePath = Plugin.Plugin.PluginFilePath;
    Request.ModuleName = "InventoryRuntime";
    Request.ModuleType = EProjectModuleType::Runtime;

    REQUIRE(ModuleCreationService::CreatePluginModule(Request));

    auto DuplicateResult = ModuleCreationService::CreatePluginModule(Request);
    REQUIRE_FALSE(DuplicateResult);
    CHECK(DuplicateResult.error().Code == EErrorCode::AlreadyExists);

    auto ResolvedPluginResult = PluginDescriptorService::LoadResolved(Plugin.Plugin.PluginFilePath.string());
    REQUIRE(ResolvedPluginResult);
    REQUIRE(ResolvedPluginResult->Descriptor.Modules.size() == 1);
    CHECK(ResolvedPluginResult->Descriptor.Modules.front().Name == "InventoryRuntime");
}
