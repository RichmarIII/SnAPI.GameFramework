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

struct TempDir
{
    std::filesystem::path Path{};

    TempDir()
    {
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_project_runtime_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

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

std::size_t CountWorldNodes(World& WorldRef)
{
    std::size_t Count = 0;
    WorldRef.ForEachNode(
        [](void* UserData, const NodeHandle&, BaseNode&) {
            auto* Counter = static_cast<std::size_t*>(UserData);
            ++(*Counter);
        },
        &Count);
    return Count;
}

std::size_t CountNodesOfType(World& WorldRef, const TypeId& Type, const bool RootsOnly = false)
{
    std::size_t Count = 0;
    WorldRef.ForEachNode([&Count, Type, RootsOnly](const NodeHandle&, BaseNode& Node) {
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

    const std::string ProjectConfig =
        std::string("{\n") +
        "  \"version\": 1,\n"
        "  \"name\": \"" + std::string(ProjectName) + "\",\n"
        "  \"assetRoot\": \"Assets\",\n"
        "  \"startupLevelAsset\": \"Levels/Startup.level\",\n"
        "  \"defaultRenderSettings\": \"\"\n"
        "}\n";
    WriteTextFile(ProjectFilePath, ProjectConfig);

    return ProjectFilePath;
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
    CHECK(std::filesystem::path(RuntimeHost.Project().ProjectFilePath).lexically_normal() == ProjectFilePath.lexically_normal());
    CHECK(std::filesystem::path(RuntimeHost.Project().AssetRootDirectory).lexically_normal()
          == (ProjectFilePath.parent_path() / "Assets").lexically_normal());
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
        .Fields = {
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

    const std::string ProjectConfig =
        std::string("{\n") +
        "  \"version\": 1,\n"
        "  \"name\": \"RuntimeRenderSettingsProject\",\n"
        "  \"assetRoot\": \"Assets\",\n"
        "  \"startupLevelAsset\": \"Levels/Startup.level\",\n"
        "  \"defaultRenderSettings\": \"" +
        SourceAssetIdFromLogicalName("Rendering/ProjectDefaultRenderSettings.prefab").ToString() + "\"\n"
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
