#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "AuthoredAssetJson.h"
#include "GameFramework.hpp"
#include "NodeCast.h"

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
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_authored_asset_test_" + Stamp);
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

std::unique_ptr<SnAPI::AssetPipeline::AssetManager> MakeSourceOnlyManager(const std::filesystem::path& Root)
{
    SnAPI::AssetPipeline::AssetManagerConfig Config{};
    Config.bEnableSourceAssets = true;
    Config.SourceRoots.push_back({
        .RootPath = Root.string(),
        .Priority = 0,
        .MountPoint = "",
    });

    auto Manager = std::make_unique<SnAPI::AssetPipeline::AssetManager>(Config);
    RegisterAssetPipelinePayloads(Manager->GetRegistry());
    RegisterAssetPipelineFactories(*Manager);
    RegisterAssetPipelineSourceStages(*Manager);
    return Manager;
}

std::size_t CountWorldNodes(World& WorldRef)
{
    std::size_t Count = 0;
    WorldRef.ForEachNode(
        [] (void* UserData, const NodeHandle&, BaseNode&) {
            auto* Counter = static_cast<std::size_t*>(UserData);
            ++(*Counter);
        },
        &Count);
    return Count;
}

} // namespace

TEST_CASE("Authored asset registry discovers built-in source asset types", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    const TypeInfo* GraphType = TypeRegistry::Instance().Find(StaticTypeId<Conduit::GraphAsset>());
    REQUIRE(GraphType != nullptr);
    CHECK(TypeRegistry::Instance().IsA(GraphType->Id, StaticTypeId<IAsset>()));
    Conduit::GraphAsset GraphInstance{};
    CHECK(TypeRegistry::Instance().Cast(GraphType->Id, StaticTypeId<IAsset>(), &GraphInstance) != nullptr);

    const TypeInfo* ClassType = TypeRegistry::Instance().Find(StaticTypeId<Conduit::ClassAsset>());
    REQUIRE(ClassType != nullptr);
    CHECK(TypeRegistry::Instance().IsA(ClassType->Id, StaticTypeId<IAsset>()));
    Conduit::ClassAsset ClassInstance{};
    CHECK(TypeRegistry::Instance().Cast(ClassType->Id, StaticTypeId<IAsset>(), &ClassInstance) != nullptr);

    AuthoredAssetRegistry::Instance().EnsureBuilt();
    std::string AssetList{};
    bool FoundGraphTypeIdMatch = false;
    bool FoundClassTypeIdMatch = false;
    for (const AuthoredAssetDescriptor& Descriptor : AuthoredAssetRegistry::Instance().All())
    {
        if (!AssetList.empty())
        {
            AssetList += ", ";
        }
        AssetList += Descriptor.Type ? Descriptor.Type->Name : "<null>";
        if (Descriptor.Type && Descriptor.Type->Name == std::string_view(TTypeNameV<Conduit::GraphAsset>))
        {
            FoundGraphTypeIdMatch = (Descriptor.AssetType == StaticTypeId<Conduit::GraphAsset>());
        }
        if (Descriptor.Type && Descriptor.Type->Name == std::string_view(TTypeNameV<Conduit::ClassAsset>))
        {
            FoundClassTypeIdMatch = (Descriptor.AssetType == StaticTypeId<Conduit::ClassAsset>());
        }
    }
    CAPTURE(AssetList);
    CHECK(FoundGraphTypeIdMatch);
    CHECK(FoundClassTypeIdMatch);
    INFO("Registry diagnostics count: " << AuthoredAssetRegistry::Instance().Diagnostics().size());
    for (const std::string& Diagnostic : AuthoredAssetRegistry::Instance().Diagnostics())
    {
        INFO(Diagnostic);
    }
    CHECK(AuthoredAssetRegistry::Instance().IsValid());

    const auto* MaterialDescriptor = AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<MaterialPayload>());
    REQUIRE(MaterialDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".material") == MaterialDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourceAssetKind(AssetKindMaterial()) == MaterialDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourcePayloadType(PayloadMaterial()) == MaterialDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedAssetKind(AssetKindMaterial()) == MaterialDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedPayloadType(PayloadMaterial()) == MaterialDescriptor);
    CHECK(MaterialDescriptor->DisplayName == "Material");
    CHECK(MaterialDescriptor->FileExtension == ".material");
    CHECK(MaterialDescriptor->SourceAssetKind == AssetKindMaterial());
    CHECK(MaterialDescriptor->CookedAssetKind == AssetKindMaterial());
    CHECK(MaterialDescriptor->SourcePayloadType == PayloadMaterial());
    CHECK(MaterialDescriptor->CookedPayloadType == PayloadMaterial());

    const auto* MaterialInstanceDescriptor =
        AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<MaterialInstancePayload>());
    REQUIRE(MaterialInstanceDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".materialinstance") == MaterialInstanceDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourceAssetKind(AssetKindMaterialInstance()) == MaterialInstanceDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourcePayloadType(PayloadMaterialInstance()) == MaterialInstanceDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedAssetKind(AssetKindMaterialInstance()) == MaterialInstanceDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedPayloadType(PayloadMaterialInstance()) == MaterialInstanceDescriptor);
    CHECK(MaterialInstanceDescriptor->DisplayName == "Material Instance");
    CHECK(MaterialInstanceDescriptor->FileExtension == ".materialinstance");
    CHECK(MaterialInstanceDescriptor->SourceAssetKind == AssetKindMaterialInstance());
    CHECK(MaterialInstanceDescriptor->CookedAssetKind == AssetKindMaterialInstance());
    CHECK(MaterialInstanceDescriptor->SourcePayloadType == PayloadMaterialInstance());
    CHECK(MaterialInstanceDescriptor->CookedPayloadType == PayloadMaterialInstance());

    const auto* GraphDescriptor =
        AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<Conduit::GraphAsset>());
    REQUIRE(GraphDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".conduitgraph") == GraphDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourceAssetKind(AssetKindConduitGraph()) == GraphDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourcePayloadType(PayloadConduitGraph()) == GraphDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedAssetKind(AssetKindConduitGraph()) == GraphDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedPayloadType(PayloadConduitGraph()) == GraphDescriptor);
    CHECK(GraphDescriptor->DisplayName == "Conduit Graph");
    CHECK(GraphDescriptor->FileExtension == ".conduitgraph");
    CHECK(GraphDescriptor->SourceAssetKind == AssetKindConduitGraph());
    CHECK(GraphDescriptor->CookedAssetKind == AssetKindConduitGraph());
    CHECK(GraphDescriptor->SourcePayloadType == PayloadConduitGraph());
    CHECK(GraphDescriptor->CookedPayloadType == PayloadConduitGraph());

    const auto* ClassDescriptor =
        AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<Conduit::ClassAsset>());
    REQUIRE(ClassDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".conduitclass") == ClassDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourceAssetKind(AssetKindConduitClass()) == ClassDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindBySourcePayloadType(PayloadConduitClass()) == ClassDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedAssetKind(AssetKindConduitClass()) == ClassDescriptor);
    CHECK(AuthoredAssetRegistry::Instance().FindByCookedPayloadType(PayloadConduitClass()) == ClassDescriptor);
    CHECK(ClassDescriptor->DisplayName == "Conduit Class");
    CHECK(ClassDescriptor->FileExtension == ".conduitclass");
    CHECK(ClassDescriptor->SourceAssetKind == AssetKindConduitClass());
    CHECK(ClassDescriptor->CookedAssetKind == AssetKindConduitClass());
    CHECK(ClassDescriptor->SourcePayloadType == PayloadConduitClass());
    CHECK(ClassDescriptor->CookedPayloadType == PayloadConduitClass());

    const auto* PrefabDescriptor = AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<NodeAsset>());
    REQUIRE(PrefabDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".prefab") == PrefabDescriptor);
    CHECK(PrefabDescriptor->DisplayName == "Prefab");
    CHECK(PrefabDescriptor->SourceAssetKind == AssetKindNode());
    CHECK(PrefabDescriptor->SourcePayloadType == PayloadNodeSource());
    CHECK(PrefabDescriptor->CookedAssetKind == AssetKindNode());
    CHECK(PrefabDescriptor->CookedPayloadType == PayloadNode());

    const auto* LevelDescriptor = AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<LevelAsset>());
    REQUIRE(LevelDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".level") == LevelDescriptor);
    CHECK(LevelDescriptor->DisplayName == "Level");
    CHECK(LevelDescriptor->SourceAssetKind == AssetKindLevel());
    CHECK(LevelDescriptor->SourcePayloadType == PayloadLevelSource());
    CHECK(LevelDescriptor->CookedAssetKind == AssetKindLevel());
    CHECK(LevelDescriptor->CookedPayloadType == PayloadLevel());

    const auto* WorldDescriptor = AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<WorldAsset>());
    REQUIRE(WorldDescriptor != nullptr);
    CHECK(AuthoredAssetRegistry::Instance().FindByExtension(".world") == WorldDescriptor);
    CHECK(WorldDescriptor->DisplayName == "World");
    CHECK(WorldDescriptor->SourceAssetKind == AssetKindWorld());
    CHECK(WorldDescriptor->SourcePayloadType == PayloadWorldSource());
    CHECK(WorldDescriptor->CookedAssetKind == AssetKindWorld());
    CHECK(WorldDescriptor->CookedPayloadType == PayloadWorld());
}

TEST_CASE("Asset manager JIT loads authored source assets by logical name", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};

    MaterialPayload Material{};
    Material.ShaderModule = "UnitTestShader";
    Material.ShadingModel = "DefaultLit";
    Material.FeatureAlbedoMap = true;
    Material.FeatureInstancing = true;
    auto MaterialJson = SerializeAuthoredAssetToJson(Material);
    REQUIRE(MaterialJson);
    WriteTextFile(Root.Path / "Rendering" / "UnitTest.material", *MaterialJson);

    Conduit::GraphAsset Graph{};
    Graph.Name = "UnitTestGraph";
    Graph.Variables.push_back({
        .Id = NewUuid(),
        .Name = "Counter",
        .Type = StaticTypeId<int>(),
        .DefaultValue = Conduit::SerializedValue::FromValue(7).value(),
    });
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);
    WriteTextFile(Root.Path / "Conduit" / "UnitTest.conduitgraph", *GraphJson);

    {
        auto Manager = MakeSourceOnlyManager(Root.Path);
        auto DirectLoadResult = Manager->Load<MaterialAssetRuntime>("Rendering/UnitTest.material");
        const std::string DirectLoadError = DirectLoadResult ? std::string{} : DirectLoadResult.error();
        INFO("Direct load error: " << DirectLoadError);
        REQUIRE(DirectLoadResult);
        REQUIRE(*DirectLoadResult);
        CHECK((*DirectLoadResult)->ShaderModule == "UnitTestShader");

        auto GetResult = Manager->Get<MaterialAssetRuntime>("Rendering/UnitTest.material");
        const std::string GetError = GetResult ? std::string{} : GetResult.error();
        INFO("Get error: " << GetError);
        REQUIRE(GetResult);
        REQUIRE(GetResult->IsValid());
        REQUIRE(GetResult->Get() != nullptr);
        CHECK(GetResult->Get()->ShaderModule == "UnitTestShader");
        CHECK(GetResult->Get()->FeatureInstancing);
    }

    {
        auto Manager = MakeSourceOnlyManager(Root.Path);
        auto LoadResult = Manager->Load<Conduit::GraphAsset>("Conduit/UnitTest.conduitgraph");
        REQUIRE(LoadResult);
        REQUIRE(*LoadResult);
        CHECK((*LoadResult)->Name == "UnitTestGraph");
        REQUIRE((*LoadResult)->Variables.size() == 1);
        CHECK((*LoadResult)->Variables.front().Name == "Counter");
        CHECK((*LoadResult)->Variables.front().Type == StaticTypeId<int>());

        auto CatalogResult = Manager->FindAsset("Conduit/UnitTest.conduitgraph");
        REQUIRE(CatalogResult);
        CHECK(CatalogResult->Name == "Conduit/UnitTest.conduitgraph");

        auto BasenameResult = Manager->FindAsset("UnitTest.conduitgraph");
        REQUIRE_FALSE(BasenameResult);
    }
}

TEST_CASE("Asset manager Get caches source-JIT runtime objects while Load stays uncached", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};

    MaterialPayload Material{};
    Material.ShaderModule = "CacheShader";
    Material.ShadingModel = "DefaultLit";
    Material.FeatureInstancing = true;
    auto MaterialJson = SerializeAuthoredAssetToJson(Material);
    REQUIRE(MaterialJson);
    WriteTextFile(Root.Path / "Rendering" / "CacheUnit.material", *MaterialJson);

    Conduit::GraphAsset Graph{};
    Graph.Name = "CacheGraph";
    Graph.Variables.push_back({
        .Id = NewUuid(),
        .Name = "Counter",
        .Type = StaticTypeId<int>(),
        .DefaultValue = Conduit::SerializedValue::FromValue(12).value(),
    });
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);
    WriteTextFile(Root.Path / "Conduit" / "CacheUnit.conduitgraph", *GraphJson);

    auto Manager = MakeSourceOnlyManager(Root.Path);

    auto GetMaterialA = Manager->Get<MaterialAssetRuntime>("Rendering/CacheUnit.material");
    REQUIRE(GetMaterialA);
    REQUIRE(GetMaterialA->IsValid());
    REQUIRE(GetMaterialA->Get() != nullptr);
    CHECK(GetMaterialA->Get()->ShaderModule == "CacheShader");

    auto GetMaterialB = Manager->Get<MaterialAssetRuntime>("Rendering/CacheUnit.material");
    REQUIRE(GetMaterialB);
    REQUIRE(GetMaterialB->IsValid());
    REQUIRE(GetMaterialB->Get() != nullptr);
    CHECK(GetMaterialA->Get() == GetMaterialB->Get());

    auto LoadMaterialA = Manager->Load<MaterialAssetRuntime>("Rendering/CacheUnit.material");
    REQUIRE(LoadMaterialA);
    REQUIRE(*LoadMaterialA);
    auto LoadMaterialB = Manager->Load<MaterialAssetRuntime>("Rendering/CacheUnit.material");
    REQUIRE(LoadMaterialB);
    REQUIRE(*LoadMaterialB);
    CHECK(LoadMaterialA->get() != LoadMaterialB->get());
    CHECK(LoadMaterialA->get() != GetMaterialA->Get());
    CHECK((*LoadMaterialA)->ShaderModule == "CacheShader");

    auto GetGraphA = Manager->Get<Conduit::GraphAsset>("Conduit/CacheUnit.conduitgraph");
    REQUIRE(GetGraphA);
    REQUIRE(GetGraphA->IsValid());
    REQUIRE(GetGraphA->Get() != nullptr);
    CHECK(GetGraphA->Get()->Name == "CacheGraph");
    REQUIRE(GetGraphA->Get()->Variables.size() == 1);

    auto GetGraphB = Manager->Get<Conduit::GraphAsset>("Conduit/CacheUnit.conduitgraph");
    REQUIRE(GetGraphB);
    REQUIRE(GetGraphB->IsValid());
    REQUIRE(GetGraphB->Get() != nullptr);
    CHECK(GetGraphA->Get() == GetGraphB->Get());

    auto LoadGraph = Manager->Load<Conduit::GraphAsset>("Conduit/CacheUnit.conduitgraph");
    REQUIRE(LoadGraph);
    REQUIRE(*LoadGraph);
    CHECK(LoadGraph->get() != GetGraphA->Get());
    CHECK((*LoadGraph)->Name == "CacheGraph");
}

TEST_CASE("Authored asset JSON saves reflected nested fields with structured values", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    Conduit::GraphAsset Graph{};
    Graph.Name = "StructuredSave";
    Graph.Variables.push_back({
        .Id = NewUuid(),
        .Name = "Counter",
        .Type = StaticTypeId<int>(),
        .DefaultValue = Conduit::SerializedValue::FromValue(7).value(),
    });

    auto JsonResult = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(JsonResult);

    const nlohmann::json Root = nlohmann::json::parse(*JsonResult);
    REQUIRE(Root.contains("Asset"));
    REQUIRE(Root["Asset"].contains("Name"));
    CHECK(Root["Asset"]["Name"] == "StructuredSave");
    REQUIRE(Root["Asset"].contains("Variables"));
    REQUIRE(Root["Asset"]["Variables"].is_array());
    REQUIRE(Root["Asset"]["Variables"].size() == 1);
    REQUIRE(Root["Asset"]["Variables"][0].contains("DefaultValue"));
    REQUIRE(Root["Asset"]["Variables"][0]["DefaultValue"].contains("Type"));
    REQUIRE(Root["Asset"]["Variables"][0]["DefaultValue"].contains("Value"));
    CHECK(Root["Asset"]["Variables"][0]["DefaultValue"]["Type"] == TTypeNameV<int>);
    CHECK(Root["Asset"]["Variables"][0]["DefaultValue"]["Value"] == 7);
}

TEST_CASE("Authored asset JSON load tolerates missing and unknown reflected fields", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    MaterialPayload Material{};
    Material.ShaderModule = "OldShader";
    Material.ShadingModel = "OldModel";
    Material.FeatureInstancing = true;
    Material.FeatureAlphaBlend = true;

    const std::string Source = R"json(
{
  "Asset": {
    "ShaderModule": "LoadedShader",
    "UnknownFutureField": 12345
  }
}
)json";

    auto LoadResult = DeserializeAuthoredAssetFromJson(Source, Material);
    REQUIRE(LoadResult);
    CHECK(Material.ShaderModule == "LoadedShader");
    CHECK(Material.ShadingModel.empty());
    CHECK_FALSE(Material.FeatureInstancing);
    CHECK_FALSE(Material.FeatureAlphaBlend);
}

TEST_CASE("Authored asset JSON save keeps material vector overrides as arrays", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    MaterialInstancePayload Material{};
    Material.Vectors.push_back({
        .Name = "Tint",
        .Value = {1.0f, 0.5f, 0.25f, 1.0f},
    });

    auto JsonResult = SerializeAuthoredAssetToJson(Material);
    REQUIRE(JsonResult);

    const nlohmann::json Root = nlohmann::json::parse(*JsonResult);
    REQUIRE(Root.contains("Asset"));
    REQUIRE(Root["Asset"].contains("Vectors"));
    REQUIRE(Root["Asset"]["Vectors"].is_array());
    REQUIRE(Root["Asset"]["Vectors"].size() == 1);
    REQUIRE(Root["Asset"]["Vectors"][0].contains("Value"));
    REQUIRE(Root["Asset"]["Vectors"][0]["Value"].is_array());
    CHECK(Root["Asset"]["Vectors"][0]["Value"].size() == 4);
    CHECK(Root["Asset"]["Vectors"][0]["Value"][0] == 1.0f);
    CHECK(Root["Asset"]["Vectors"][0]["Value"][1] == 0.5f);
    CHECK(Root["Asset"]["Vectors"][0]["Value"][2] == 0.25f);
    CHECK(Root["Asset"]["Vectors"][0]["Value"][3] == 1.0f);
}

TEST_CASE("Authored asset JSON load reports field diagnostics while keeping defaults", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    Conduit::GraphAsset Graph{};
    Graph.Name = "Diagnostics";

    const std::string Source = R"json(
{
  "Asset": {
    "Name": "ImportedGraph",
    "Variables": [
      {
        "Id": "not-a-uuid",
        "Name": "Health",
        "Type": "int"
      }
    ]
  }
}
)json";

    AuthoredAssetImportDiagnostics Diagnostics{};
    auto LoadResult = DeserializeAuthoredAssetFromJson(Source, Graph, Diagnostics);
    REQUIRE(LoadResult);
    CHECK(Graph.Name == "ImportedGraph");
    CHECK(Graph.Variables.empty());
    REQUIRE_FALSE(Diagnostics.empty());
    CHECK(Diagnostics.front().find("Variables") != std::string::npos);
}

TEST_CASE("Asset manager JIT loads authored prefab level and world source assets by logical name", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};

    NodeAsset Prefab{};
    Prefab.Name = "UnitPrefab";
    Prefab.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<BaseNode>(),
        .Name = "PrefabRoot",
        .Active = true,
        .Fields = {
            NodeFieldAsset{
                .Name = "Name",
                .Value = Conduit::SerializedValue::FromValue(std::string("PrefabRoot")).value(),
            },
        },
        .Components = {
            NodeComponentAsset{
                .Id = NewUuid(),
                .Type = StaticTypeId<TransformComponent>(),
                .Fields = {},
            },
        },
        .Children = {
            NodeObjectAsset{
                .Id = NewUuid(),
                .Type = StaticTypeId<BaseNode>(),
                .Name = "PrefabChild",
                .Active = true,
                .Fields = {
                    NodeFieldAsset{
                        .Name = "Name",
                        .Value = Conduit::SerializedValue::FromValue(std::string("PrefabChild")).value(),
                    },
                },
            },
        },
    });

    LevelAsset LevelSource{};
    LevelSource.Name = "GameplayLevel";
    LevelSource.Nodes = Prefab.Nodes;

    WorldAsset WorldSource{};
    WorldSource.Name = "GameplayWorld";
    WorldSource.Nodes = Prefab.Nodes;

    auto PrefabJson = SerializeAuthoredAssetToJson(Prefab);
    REQUIRE(PrefabJson);
    WriteTextFile(Root.Path / "Prefabs" / "Unit.prefab", *PrefabJson);

    auto LevelJson = SerializeAuthoredAssetToJson(LevelSource);
    REQUIRE(LevelJson);
    WriteTextFile(Root.Path / "Levels" / "Unit.level", *LevelJson);

    auto WorldJson = SerializeAuthoredAssetToJson(WorldSource);
    REQUIRE(WorldJson);
    WriteTextFile(Root.Path / "Worlds" / "Unit.world", *WorldJson);

    auto Manager = MakeSourceOnlyManager(Root.Path);

    World PrefabWorld("PrefabLoadWorld");
    NodeHandle PrefabRootHandle{};
    NodeAssetLoadParams PrefabParams{};
    PrefabParams.TargetWorld = &PrefabWorld;
    PrefabParams.OutCreatedRoot = &PrefabRootHandle;
    auto PrefabLoad = Manager->Load<BaseNode>("Prefabs/Unit.prefab", PrefabParams);
    REQUIRE(PrefabLoad);
    REQUIRE(*PrefabLoad);
    BaseNode* PrefabRoot = PrefabRootHandle.Borrowed();
    REQUIRE(PrefabRoot != nullptr);
    CHECK(PrefabRoot->Name() == "PrefabRoot");
    CHECK(PrefabRoot->Children().size() == 1);
    CHECK(PrefabRoot->ComponentTypes().size() == 1);
    CHECK(PrefabRoot->ComponentTypes().front() == StaticTypeId<TransformComponent>());
    CHECK(CountWorldNodes(PrefabWorld) == 2);

    World LevelWorld("LevelLoadWorld");
    NodeHandle LoadedLevelHandle{};
    LevelAssetLoadParams LevelParams{};
    LevelParams.TargetWorld = &LevelWorld;
    LevelParams.OutCreatedLevel = &LoadedLevelHandle;
    auto LevelLoad = Manager->Load<Level>("Levels/Unit.level", LevelParams);
    REQUIRE(LevelLoad);
    REQUIRE(*LevelLoad);
    auto* LoadedLevel = NodeCast<Level>(LoadedLevelHandle.Borrowed());
    REQUIRE(LoadedLevel != nullptr);
    CHECK(LoadedLevel->Children().size() == 1);
    CHECK(CountWorldNodes(LevelWorld) == 3);

    auto WorldLoad = Manager->Load<World>("Worlds/Unit.world");
    REQUIRE(WorldLoad);
    REQUIRE(*WorldLoad);
    CHECK(CountWorldNodes(**WorldLoad) == 2);
}
