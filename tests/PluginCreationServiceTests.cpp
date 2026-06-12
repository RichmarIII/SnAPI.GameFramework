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
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_plugin_creation_test_" + Stamp);
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

} // namespace

TEST_CASE("PluginCreationService creates a default plugin workspace", "[Plugin][Create]")
{
    TempDir Root{};

    PluginCreationRequest Request{};
    Request.PluginName = "Inventory";
    Request.ParentDirectory = Root.Path;

    PluginCreationResult Result{};
    REQUIRE(PluginCreationService::CreatePlugin(Request, &Result));

    const std::filesystem::path PluginRoot = Root.Path / "Inventory";
    CHECK(Result.Plugin.PluginRootDirectory.lexically_normal() == PluginRoot.lexically_normal());
    CHECK(std::filesystem::exists(Result.Plugin.PluginFilePath));
    CHECK(std::filesystem::exists(Result.Plugin.AssetRootDirectory));
    CHECK(std::filesystem::exists(Result.Plugin.CodeRootDirectory));
    CHECK(std::filesystem::exists(Result.Plugin.ConfigRootDirectory));
    CHECK(std::filesystem::exists(Result.Plugin.IntermediateRootDirectory));
    CHECK(std::filesystem::exists(Result.Plugin.SavedRootDirectory));
    CHECK(std::filesystem::exists(Result.PluginCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedPluginModulesCMakePath));
    CHECK(std::filesystem::exists(Result.RuntimeModuleDirectory));
    CHECK(std::filesystem::exists(Result.RuntimeModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.RuntimeModuleCMakePath));
    CHECK(Result.EditorModuleDirectory.empty());
    CHECK(Result.GeneratedFiles.size() == 6);

    REQUIRE(Result.Plugin.Descriptor.Modules.size() == 1);
    CHECK(Result.Plugin.Descriptor.Plugin.Name == "Inventory");
    CHECK(Result.Plugin.Descriptor.Plugin.Version == "0.1.0");
    CHECK(Result.Plugin.Descriptor.Modules.front().Name == "Inventory");
    CHECK(Result.Plugin.Descriptor.Modules.front().Type == EProjectModuleType::Runtime);
    CHECK(Result.Plugin.Descriptor.Modules.front().Root == "Code/Inventory");
    CHECK(Result.Plugin.Descriptor.Modules.front().PublicDependencies == std::vector<std::string>{"SnAPI.GameFramework"});

    CHECK(ReadTextFile(Result.PluginCodeRootCMakePath)
              .find("include(\"${SNAPI_PLUGIN_ROOT_DIR}/Intermediate/Build/Generated/PluginModules.cmake\" OPTIONAL)")
          != std::string::npos);
    CHECK(ReadTextFile(Result.GeneratedPluginModulesCMakePath)
              .find("add_subdirectory(\"${SNAPI_PLUGIN_ROOT_DIR}/Code/Inventory\")")
          != std::string::npos);
    CHECK(ReadTextFile(Result.GeneratedPluginModulesCMakePath)
              .find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE Inventory)")
          != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleCMakePath).find("add_library(Inventory") != std::string::npos);
    CHECK(ReadTextFile(Result.RuntimeModuleDirectory / "include" / "Inventory" / "InventoryModule.h")
              .find("class InventoryModule final")
          != std::string::npos);
}

TEST_CASE("PluginCreationService can generate a companion editor module", "[Plugin][Create]")
{
    TempDir Root{};

    PluginCreationRequest Request{};
    Request.PluginName = "Inventory";
    Request.ParentDirectory = Root.Path;
    Request.Code.CreateStarterEditorModule = true;

    PluginCreationResult Result{};
    REQUIRE(PluginCreationService::CreatePlugin(Request, &Result));

    REQUIRE(Result.Plugin.Descriptor.Modules.size() == 2);
    CHECK(std::filesystem::exists(Result.EditorModuleDirectory));
    CHECK(std::filesystem::exists(Result.EditorModuleRootCMakePath));
    CHECK(std::filesystem::exists(Result.EditorModuleCMakePath));
    CHECK(Result.GeneratedFiles.size() == 10);

    const ProjectModuleDescriptor& RuntimeModule = Result.Plugin.Descriptor.Modules[0];
    const ProjectModuleDescriptor& EditorModule = Result.Plugin.Descriptor.Modules[1];

    CHECK(RuntimeModule.Name == "Inventory");
    CHECK(EditorModule.Name == "InventoryEditor");
    CHECK(EditorModule.Type == EProjectModuleType::Editor);
    CHECK(EditorModule.Root == "Code/InventoryEditor");
    CHECK(EditorModule.LoadInEditor);
    CHECK_FALSE(EditorModule.LoadInRuntime);
    CHECK(EditorModule.PrivateDependencies == std::vector<std::string>{"Inventory", "SnAPI.GameFramework"});

    const std::string GeneratedPluginModules = ReadTextFile(Result.GeneratedPluginModulesCMakePath);
    CHECK(GeneratedPluginModules.find("add_subdirectory(\"${SNAPI_PLUGIN_ROOT_DIR}/Code/InventoryEditor\")")
          != std::string::npos);
    CHECK(GeneratedPluginModules.find("target_link_libraries(SnAPI.GameFramework.Editor PRIVATE InventoryEditor)")
          != std::string::npos);
    CHECK(GeneratedPluginModules.find("target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE InventoryEditor)")
          == std::string::npos);
    CHECK(ReadTextFile(Result.EditorModuleCMakePath)
              .find("target_link_libraries(InventoryEditor PRIVATE\n    Inventory\n    SnAPI.GameFramework\n)")
          != std::string::npos);
}

TEST_CASE("PluginCreationService can skip starter runtime module generation", "[Plugin][Create]")
{
    TempDir Root{};

    PluginCreationRequest Request{};
    Request.PluginName = "ContentOnlyPlugin";
    Request.ParentDirectory = Root.Path;
    Request.Code.CreateStarterRuntimeModule = false;

    PluginCreationResult Result{};
    REQUIRE(PluginCreationService::CreatePlugin(Request, &Result));

    CHECK(std::filesystem::exists(Result.Plugin.PluginFilePath));
    CHECK(std::filesystem::exists(Result.PluginCodeRootCMakePath));
    CHECK(std::filesystem::exists(Result.GeneratedPluginModulesCMakePath));
    CHECK(Result.Plugin.Descriptor.Modules.empty());
    CHECK(Result.RuntimeModuleDirectory.empty());
    CHECK(Result.RuntimeModuleRootCMakePath.empty());
    CHECK(Result.RuntimeModuleCMakePath.empty());
    CHECK(Result.GeneratedFiles.size() == 2);
    CHECK(ReadTextFile(Result.GeneratedPluginModulesCMakePath).find("No plugin modules are currently declared.")
          != std::string::npos);
}
