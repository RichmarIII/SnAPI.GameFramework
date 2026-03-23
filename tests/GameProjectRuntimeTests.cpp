#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "AssetPipelineIds.h"
#include "AuthoredAssetJson.h"
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace
{

    /**
     * @brief Temporary per-test directory that is removed on scope exit.
     */
    struct TempDir
    {
        std::filesystem::path Path{};

        TempDir()
        {
            const auto Stamp = std::to_string(
                static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_project_runtime_test_" + Stamp);
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
     * @brief Count all live nodes in a world.
     * @param WorldRef World to inspect.
     * @return Total node count.
     */
    std::size_t CountWorldNodes(World& WorldRef)
    {
        std::size_t Count = 0;
        WorldRef.ForEachNode(
            [](void* UserData, const NodeHandle&, BaseNode&)
            {
                auto* Counter = static_cast<std::size_t*>(UserData);
                ++(*Counter);
            },
            &Count);
        return Count;
    }

    /**
     * @brief Count world nodes assignable to one reflected type.
     * @param WorldRef World to inspect.
     * @param Type Reflected type to match.
     * @param RootsOnly `true` to count only root nodes.
     * @return Matching node count.
     */
    std::size_t CountNodesOfType(World& WorldRef, const TypeId& Type, const bool RootsOnly = false)
    {
        std::size_t Count = 0;
        WorldRef.ForEachNode(
            [&Count, Type, RootsOnly](const NodeHandle&, BaseNode& Node)
            {
                if (RootsOnly && !Node.Parent().IsNull())
                {
                    return;
                }

                if (TypeRegistry::Instance().IsA(Node.TypeKey(), Type))
                {
                    ++Count;
                }
            });
        return Count;
    }

    /**
     * @brief Create a minimal legacy-schema project used by runtime integration tests.
     * @param Root Temporary root that owns the project folder.
     * @param ProjectName Project name to author into the descriptor.
     * @return Path to the created project descriptor.
     */
    std::filesystem::path CreateBasicProject(const TempDir& Root, const std::string_view ProjectName)
    {
        const std::filesystem::path ProjectRoot = Root.Path / std::string(ProjectName);
        const std::filesystem::path AssetRoot = ProjectRoot / "Assets";
        const std::filesystem::path StartupLevelPath = AssetRoot / "Levels" / "Startup.level";
        const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

        LevelAsset StartupLevel{};
        StartupLevel.Name = "Startup";

        auto LevelJson = SerializeAuthoredAssetToJson(StartupLevel);
        REQUIRE(LevelJson);
        WriteTextFile(StartupLevelPath, *LevelJson);

        const std::string ProjectConfig = std::string("{\n") +
            "  \"version\": 1,\n"
            "  \"name\": \"" +
            std::string(ProjectName) +
            "\",\n"
            "  \"assetRoot\": \"Assets\",\n"
            "  \"startupLevelAsset\": \"Levels/Startup.level\",\n"
            "  \"defaultRenderSettings\": \"\"\n"
            "}\n";
        WriteTextFile(ProjectFilePath, ProjectConfig);

        return ProjectFilePath;
    }

    /**
     * @brief Create a minimal structured-schema project used by runtime integration tests.
     * @param Root Temporary root that owns the project folder.
     * @param ProjectName Project name to author into the descriptor.
     * @return Path to the created structured project descriptor.
     */
    std::filesystem::path CreateStructuredProject(const TempDir& Root, const std::string_view ProjectName)
    {
        const std::filesystem::path ProjectRoot = Root.Path / std::string(ProjectName);
        const std::filesystem::path AssetRoot = ProjectRoot / "Assets";
        const std::filesystem::path StartupLevelPath = AssetRoot / "Levels" / "Startup.level";
        const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

        LevelAsset StartupLevel{};
        StartupLevel.Name = "Startup";

        auto LevelJson = SerializeAuthoredAssetToJson(StartupLevel);
        REQUIRE(LevelJson);
        WriteTextFile(StartupLevelPath, *LevelJson);

        ProjectDescriptor Descriptor{};
        Descriptor.Project.Name = std::string(ProjectName);
        Descriptor.Project.DisplayName = std::string(ProjectName) + " Display";
        Descriptor.Paths.AssetRoot = "Assets";
        Descriptor.Startup.StartupLevelAsset = "Levels/Startup.level";
        Descriptor.Startup.DefaultGameClass = std::string(ProjectName) + "::Game";
        BuildProfile WindowsDevelopment{};
        WindowsDevelopment.Name = "WindowsDevelopment";
        WindowsDevelopment.Platform = BuildProfileValue<std::string>{.IsSet = true, .Value = std::string("Windows")};
        WindowsDevelopment.Configuration =
            BuildProfileValue<EBuildConfiguration>{.IsSet = true, .Value = EBuildConfiguration::Development};
        Descriptor.Profiles.push_back(std::move(WindowsDevelopment));

        REQUIRE(ProjectDescriptorService::Save(Descriptor, ProjectFilePath.string()));
        return ProjectFilePath;
    }

    /**
     * @brief Create a minimal packaged-runtime bootstrap directory used by runtime integration tests.
     * @param Root Temporary root that owns the packaged output tree.
     * @param ProjectName Project name to author into the bootstrap config.
     * @return Path to `Config/ResolvedRuntimeConfig.json`.
     */
    std::filesystem::path CreatePackagedRuntimeBootstrap(const TempDir& Root, const std::string_view ProjectName)
    {
        const std::filesystem::path PackageRoot = Root.Path / (std::string(ProjectName) + "_Package");
        const std::filesystem::path AssetRoot = PackageRoot / "Assets";
        const std::filesystem::path StartupLevelPath = AssetRoot / "Levels" / "Startup.level";
        const std::filesystem::path BootstrapConfigPath = PackageRoot / "Config" / "ResolvedRuntimeConfig.json";

        LevelAsset StartupLevel{};
        StartupLevel.Name = "Startup";

        auto LevelJson = SerializeAuthoredAssetToJson(StartupLevel);
        REQUIRE(LevelJson);
        WriteTextFile(StartupLevelPath, *LevelJson);

        const std::string ConfigText = std::string("{\n") +
            "  \"BuildId\": \"runtime-packaged-test\",\n"
            "  \"ProjectName\": \"" +
            std::string(ProjectName) +
            "\",\n"
            "  \"AssetRoot\": \"Assets\",\n"
            "  \"Startup\": {\n"
            "    \"StartupLevelAsset\": \"Levels/Startup.level\",\n"
            "    \"DefaultRenderSettingsAssetId\": \"\",\n"
            "    \"DefaultGameClass\": \"\",\n"
            "    \"DefaultGameModeClass\": \"\"\n"
            "  }\n"
            "}\n";
        WriteTextFile(BootstrapConfigPath, ConfigText);

        return BootstrapConfigPath;
    }

} // namespace

TEST_CASE("GameProjectRuntime loads a project startup level", "[Runtime][Project]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    const std::filesystem::path ProjectFilePath = CreateBasicProject(Root, "RuntimeLoadProject");

    GameProjectRuntime RuntimeHost{};
    GameProjectRuntimeSettings Settings{};
    Settings.ProjectFilePath = ProjectFilePath.string();
    Settings.Runtime.WorldName = "RuntimeLoadProjectWorld";
    Settings.Runtime.RegisterBuiltins = true;

    REQUIRE(RuntimeHost.Initialize(Settings));
    REQUIRE(RuntimeHost.IsInitialized());

    CHECK(RuntimeHost.Project().IsLoaded);
    CHECK(RuntimeHost.Project().Name == "RuntimeLoadProject");
    CHECK(std::filesystem::path(RuntimeHost.Project().ProjectFilePath).lexically_normal() ==
          ProjectFilePath.lexically_normal());
    CHECK(std::filesystem::path(RuntimeHost.Project().AssetRootDirectory).lexically_normal() ==
          (ProjectFilePath.parent_path() / "Assets").lexically_normal());
    CHECK(RuntimeHost.Runtime().World().Levels().size() == 1);
    CHECK(CountWorldNodes(RuntimeHost.Runtime().World()) == 2);

    RuntimeHost.Shutdown();
}

TEST_CASE("GameProjectRuntime loads the structured project descriptor schema", "[Runtime][Project]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    const std::filesystem::path ProjectFilePath = CreateStructuredProject(Root, "StructuredRuntimeProject");

    GameProjectRuntime RuntimeHost{};
    GameProjectRuntimeSettings Settings{};
    Settings.ProjectFilePath = ProjectFilePath.string();
    Settings.Runtime.WorldName = "StructuredRuntimeWorld";
    Settings.Runtime.RegisterBuiltins = true;

    REQUIRE(RuntimeHost.Initialize(Settings));
    REQUIRE(RuntimeHost.IsInitialized());

    CHECK(RuntimeHost.Project().IsLoaded);
    CHECK(RuntimeHost.Project().Name == "StructuredRuntimeProject");
    CHECK(RuntimeHost.Project().AssetRoot == "Assets");
    CHECK(RuntimeHost.Project().StartupLevelAsset == "Levels/Startup.level");
    CHECK(std::filesystem::path(RuntimeHost.Project().ProjectFilePath).lexically_normal() ==
          ProjectFilePath.lexically_normal());
    CHECK(std::filesystem::path(RuntimeHost.Project().AssetRootDirectory).lexically_normal() ==
          (ProjectFilePath.parent_path() / "Assets").lexically_normal());
    CHECK(RuntimeHost.Runtime().World().Levels().size() == 1);
    CHECK(CountWorldNodes(RuntimeHost.Runtime().World()) == 2);

    RuntimeHost.Shutdown();
}

TEST_CASE("GameProjectRuntime loads packaged runtime bootstrap metadata", "[Runtime][Project][Packaged]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    const std::filesystem::path BootstrapConfigPath =
        CreatePackagedRuntimeBootstrap(Root, "PackagedRuntimeProject");

    GameProjectRuntime RuntimeHost{};
    GameProjectRuntimeSettings Settings{};
    Settings.BootstrapPath = BootstrapConfigPath.string();
    Settings.Runtime.WorldName = "PackagedRuntimeWorld";
    Settings.Runtime.RegisterBuiltins = true;

    REQUIRE(RuntimeHost.Initialize(Settings));
    REQUIRE(RuntimeHost.IsInitialized());

    CHECK(RuntimeHost.Project().IsLoaded);
    CHECK(RuntimeHost.Project().Name == "PackagedRuntimeProject");
    CHECK(std::filesystem::path(RuntimeHost.Project().ProjectFilePath).lexically_normal() ==
          BootstrapConfigPath.lexically_normal());
    CHECK(std::filesystem::path(RuntimeHost.Project().ProjectRootDirectory).lexically_normal() ==
          BootstrapConfigPath.parent_path().parent_path().lexically_normal());
    CHECK(std::filesystem::path(RuntimeHost.Project().AssetRootDirectory).lexically_normal() ==
          (BootstrapConfigPath.parent_path().parent_path() / "Assets").lexically_normal());
    CHECK(RuntimeHost.Runtime().World().Levels().size() == 1);
    CHECK(CountWorldNodes(RuntimeHost.Runtime().World()) == 2);

    RuntimeHost.Shutdown();
}

TEST_CASE("GameProjectRuntime restarts gameplay host after project content loads", "[Runtime][Project]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    const std::filesystem::path ProjectFilePath = CreateBasicProject(Root, "RuntimeGameplayProject");

    GameProjectRuntime RuntimeHost{};
    GameProjectRuntimeSettings Settings{};
    Settings.ProjectFilePath = ProjectFilePath.string();
    Settings.Runtime.WorldName = "RuntimeGameplayProjectWorld";
    Settings.Runtime.RegisterBuiltins = true;
    Settings.Runtime.Gameplay = GameRuntimeGameplaySettings{};

    REQUIRE(RuntimeHost.Initialize(Settings));
    REQUIRE(RuntimeHost.IsInitialized());
    REQUIRE(RuntimeHost.Runtime().Gameplay() != nullptr);
    CHECK(RuntimeHost.Runtime().Gameplay()->IsInitialized());
    CHECK(RuntimeHost.Runtime().Gameplay()->LocalPlayers().size() == 1);

    RuntimeHost.Shutdown();
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
TEST_CASE("GameProjectRuntime does not inject project default render settings over authored startup settings",
          "[Runtime][Project][Renderer]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    const std::filesystem::path ProjectRoot = Root.Path / "RuntimeRenderSettingsProject";
    const std::filesystem::path AssetRoot = ProjectRoot / "Assets";
    const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

    NodeAsset FogPrefab{};
    FogPrefab.Name = "ProjectFogParams";
    FogPrefab.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<HeightFogParamsNode>(),
        .Name = "ProjectFogParams",
        .Active = true,
    });
    auto FogJson = SerializeAuthoredAssetToJson(FogPrefab);
    REQUIRE(FogJson);
    WriteTextFile(AssetRoot / "Rendering" / "ProjectFogParams.prefab", *FogJson);

    TAssetRef<HeightFogParamsNode> FogRef{};
    FogRef.EditAssetName() = "Rendering/ProjectFogParams.prefab";
    FogRef.EditAssetId() = SourceAssetIdFromLogicalName(FogRef.GetAssetName()).ToString();

    NodeAsset DefaultRenderSettingsPrefab{};
    DefaultRenderSettingsPrefab.Name = "ProjectDefaultRenderSettings";
    DefaultRenderSettingsPrefab.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<WorldRenderSettings>(),
        .Name = "ProjectDefaultRenderSettings",
        .Active = true,
        .Fields =
            {
                NodeFieldAsset{
                    .Name = "HeightFogParams",
                    .Value = Conduit::SerializedValue::FromValue(FogRef).value(),
                },
            },
    });
    auto RenderSettingsJson = SerializeAuthoredAssetToJson(DefaultRenderSettingsPrefab);
    REQUIRE(RenderSettingsJson);
    WriteTextFile(AssetRoot / "Rendering" / "ProjectDefaultRenderSettings.prefab", *RenderSettingsJson);

    LevelAsset StartupLevel{};
    StartupLevel.Name = "Startup";
    StartupLevel.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<WorldRenderSettings>(),
        .Name = "AuthoredWorldRenderSettings",
        .Active = true,
    });
    auto LevelJson = SerializeAuthoredAssetToJson(StartupLevel);
    REQUIRE(LevelJson);
    WriteTextFile(AssetRoot / "Levels" / "Startup.level", *LevelJson);

    const std::string ProjectConfig = std::string("{\n") +
        "  \"version\": 1,\n"
        "  \"name\": \"RuntimeRenderSettingsProject\",\n"
        "  \"assetRoot\": \"Assets\",\n"
        "  \"startupLevelAsset\": \"Levels/Startup.level\",\n"
        "  \"defaultRenderSettings\": \"" +
        SourceAssetIdFromLogicalName("Rendering/ProjectDefaultRenderSettings.prefab").ToString() +
        "\"\n"
        "}\n";
    WriteTextFile(ProjectFilePath, ProjectConfig);

    GameProjectRuntime RuntimeHost{};
    GameProjectRuntimeSettings Settings{};
    Settings.ProjectFilePath = ProjectFilePath.string();
    Settings.Runtime.WorldName = "RuntimeRenderSettingsWorld";
    Settings.Runtime.RegisterBuiltins = true;

    REQUIRE(RuntimeHost.Initialize(Settings));
    REQUIRE(RuntimeHost.IsInitialized());
    CHECK(CountNodesOfType(RuntimeHost.Runtime().World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(RuntimeHost.Runtime().World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    RuntimeHost.Shutdown();
}
#endif
