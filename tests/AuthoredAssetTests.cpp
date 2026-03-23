#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "AuthoredAssetJson.h"
#include "GameFramework.hpp"
#include "NodeCast.h"
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <Material.hpp>
#include <IVertexStreamSource.hpp>
#include <ShaderCompilationManager.hpp>
#include "RenderAssets/MeshRuntimeAssets.h"
#endif

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

struct ScopedAssetRoot
{
    std::filesystem::path Previous{};

    explicit ScopedAssetRoot(const std::filesystem::path& Path)
        : Previous(SPathResolver::Instance().AssetRoot())
    {
        REQUIRE(SPathResolver::Instance().SetAssetRoot(Path));
    }

    ~ScopedAssetRoot()
    {
        (void)SPathResolver::Instance().SetAssetRoot(Previous);
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

template<typename TValue>
std::vector<std::uint8_t> BytesFromVector(const std::vector<TValue>& Values)
{
    static_assert(std::is_trivially_copyable_v<TValue>);

    std::vector<std::uint8_t> Bytes(sizeof(TValue) * Values.size());
    if (!Bytes.empty())
    {
        std::memcpy(Bytes.data(), Values.data(), Bytes.size());
    }
    return Bytes;
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

#if defined(SNAPI_GF_ENABLE_RENDERER)
[[nodiscard]] bool CanCompileRuntimeMaterialsInTests()
{
    return SnAPI::Graphics::ShaderCompilationManager::TryInstance() != nullptr;
}
#endif

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

    const auto* MaterialDescriptor = AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<MaterialAsset>());
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
        AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<MaterialInstanceAsset>());
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

    MaterialAsset Material{};
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
#if defined(SNAPI_GF_ENABLE_RENDERER)
        if (CanCompileRuntimeMaterialsInTests())
        {
            auto DirectLoadResult = Manager->LoadShared<SnAPI::Graphics::Material>("Rendering/UnitTest.material");
            const std::string DirectLoadError = DirectLoadResult ? std::string{} : DirectLoadResult.error();
            INFO("Direct load error: " << DirectLoadError);
            REQUIRE(DirectLoadResult);
            REQUIRE(*DirectLoadResult);
            CHECK((*DirectLoadResult)->ShaderModuleName() == "UnitTestShader");

            auto GetResult = Manager->GetShared<SnAPI::Graphics::Material>("Rendering/UnitTest.material");
            const std::string GetError = GetResult ? std::string{} : GetResult.error();
            INFO("Get error: " << GetError);
            REQUIRE(GetResult);
            REQUIRE(*GetResult);
            CHECK((*GetResult)->ShaderModuleName() == "UnitTestShader");
        }
        else
        {
            INFO("Skipping runtime material load checks because ShaderCompilationManager is not initialized in this test process.");
        }
#endif
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

    MaterialAsset Material{};
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

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (CanCompileRuntimeMaterialsInTests())
    {
        auto GetMaterialA = Manager->GetShared<SnAPI::Graphics::Material>("Rendering/CacheUnit.material");
        REQUIRE(GetMaterialA);
        REQUIRE(*GetMaterialA);
        CHECK((*GetMaterialA)->ShaderModuleName() == "CacheShader");

        auto GetMaterialB = Manager->GetShared<SnAPI::Graphics::Material>("Rendering/CacheUnit.material");
        REQUIRE(GetMaterialB);
        REQUIRE(*GetMaterialB);
        CHECK(*GetMaterialA == *GetMaterialB);

        auto LoadMaterialA = Manager->LoadShared<SnAPI::Graphics::Material>("Rendering/CacheUnit.material");
        REQUIRE(LoadMaterialA);
        REQUIRE(*LoadMaterialA);
        auto LoadMaterialB = Manager->LoadShared<SnAPI::Graphics::Material>("Rendering/CacheUnit.material");
        REQUIRE(LoadMaterialB);
        REQUIRE(*LoadMaterialB);
        CHECK(*LoadMaterialA != *LoadMaterialB);
        CHECK(*LoadMaterialA != *GetMaterialA);
        CHECK((*LoadMaterialA)->ShaderModuleName() == "CacheShader");
    }
    else
    {
        INFO("Skipping runtime material cache checks because ShaderCompilationManager is not initialized in this test process.");
    }
#endif

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

TEST_CASE("Authored asset refs load source documents without going through AssetManager", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    Conduit::GraphAsset Graph{};
    Graph.Name = "AuthoredGraph";
    Graph.Variables.push_back({
        .Id = NewUuid(),
        .Name = "Counter",
        .Type = StaticTypeId<int>(),
        .DefaultValue = Conduit::SerializedValue::FromValue(42).value(),
    });
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);
    WriteTextFile(Root.Path / "Conduit" / "AuthoredGraph.conduitgraph", *GraphJson);

    TAssetRef<Conduit::GraphAsset> GraphRef("Conduit/AuthoredGraph.conduitgraph");
    auto LoadResult = GraphRef.Load();
    REQUIRE(LoadResult);
    REQUIRE(*LoadResult);
    CHECK((*LoadResult)->Name == "AuthoredGraph");
    REQUIRE((*LoadResult)->Variables.size() == 1);
    CHECK((*LoadResult)->Variables.front().Name == "Counter");
}

TEST_CASE("Authored asset refs compile runtime assets from authored source payloads on demand", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    Conduit::GraphAsset Graph{};
    Graph.Name = "PayloadGraph";
    Graph.Variables.push_back({
        .Id = NewUuid(),
        .Name = "Counter",
        .Type = StaticTypeId<int>(),
        .DefaultValue = Conduit::SerializedValue::FromValue(17).value(),
    });
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);
    WriteTextFile(Root.Path / "Conduit" / "PayloadGraph.conduitgraph", *GraphJson);

    auto Manager = MakeSourceOnlyManager(Root.Path);
    TAssetRef<Conduit::GraphAsset> GraphRef("Conduit/PayloadGraph.conduitgraph");

    auto LoadRuntimeResult = GraphRef.LoadRuntime<Conduit::GraphAsset>(*Manager);
    REQUIRE(LoadRuntimeResult);
    REQUIRE(*LoadRuntimeResult);
    CHECK((*LoadRuntimeResult)->Name == "PayloadGraph");

    auto RuntimeInfo = Manager->FindAsset("Conduit/PayloadGraph.conduitgraph");
    REQUIRE(RuntimeInfo);
    CHECK(RuntimeInfo->Name == "Conduit/PayloadGraph.conduitgraph");

    auto GetRuntimeA = GraphRef.GetRuntime<Conduit::GraphAsset>(*Manager);
    REQUIRE(GetRuntimeA);
    REQUIRE(GetRuntimeA->IsValid());
    REQUIRE(GetRuntimeA->Get() != nullptr);
    CHECK(GetRuntimeA->Get()->Name == "PayloadGraph");

    auto GetRuntimeB = GraphRef.GetRuntime<Conduit::GraphAsset>(*Manager);
    REQUIRE(GetRuntimeB);
    REQUIRE(GetRuntimeB->IsValid());
    REQUIRE(GetRuntimeB->Get() != nullptr);
    CHECK(GetRuntimeA->Get() == GetRuntimeB->Get());
}

TEST_CASE("Authored static mesh assets compile into runtime mesh payloads on demand", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    StaticMeshAsset Mesh{};
    Mesh.Mesh.Name = "Triangle";
    Mesh.Mesh.BoundsMin = {0.0f, 0.0f, 0.0f};
    Mesh.Mesh.BoundsMax = {1.0f, 1.0f, 0.0f};
    Mesh.Mesh.SubMeshes.push_back({
        .IndexOffset = 0,
        .IndexCount = 3,
        .MaterialSlot = 0,
        .BoundsMin = {0.0f, 0.0f, 0.0f},
        .BoundsMax = {1.0f, 1.0f, 0.0f},
    });

    const std::vector<float> Positions = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    Mesh.Streams.push_back({
        .Semantic = EMeshStreamSemantic::Position,
        .Bytes = BytesFromVector(Positions),
        .ElementCount = 3,
        .StrideBytes = sizeof(float) * 3,
        .Compress = false,
    });

    const std::vector<std::uint32_t> Indices = {0u, 1u, 2u};
    Mesh.Streams.push_back({
        .Semantic = EMeshStreamSemantic::Index,
        .Bytes = BytesFromVector(Indices),
        .ElementCount = static_cast<std::uint32_t>(Indices.size()),
        .StrideBytes = sizeof(std::uint32_t),
        .Compress = false,
    });

    auto MeshJson = SerializeAuthoredAssetToJson(Mesh);
    REQUIRE(MeshJson);
    WriteTextFile(Root.Path / "Rendering" / "Triangle.staticmesh", *MeshJson);

    TAssetRef<StaticMeshAsset> MeshRef("Rendering/Triangle.staticmesh");
    auto LoadedMesh = MeshRef.Load();
    REQUIRE(LoadedMesh);
    REQUIRE(*LoadedMesh);
    CHECK((*LoadedMesh)->Mesh.Name == "Triangle");
    REQUIRE((*LoadedMesh)->Streams.size() == 2);

    auto Manager = MakeSourceOnlyManager(Root.Path);

    const auto MeshEntries = TAssetRef<StaticMeshAsset>::EnumerateCompatibleAssets(*Manager);
    CHECK(std::any_of(MeshEntries.begin(), MeshEntries.end(), [](const TAssetRef<StaticMeshAsset>::TEntry& Entry) {
        return Entry.Name == "Rendering/Triangle.staticmesh";
    }));

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto SharedRuntimeMesh = MeshRef.GetRuntimeShared<SnAPI::Graphics::IVertexStreamSource>(*Manager);
    REQUIRE(SharedRuntimeMesh);
    REQUIRE(*SharedRuntimeMesh);
    auto MeshInfo = Manager->FindAsset("Rendering/Triangle.staticmesh");
    REQUIRE(MeshInfo);
    const std::string ExpectedMeshDebugToken = "asset-id://" + MeshInfo->Id.ToString();
    CHECK((*SharedRuntimeMesh)->DebugName().find(ExpectedMeshDebugToken) != std::string_view::npos);
    CHECK((*SharedRuntimeMesh)->VertexCount() == 3u);
    CHECK((*SharedRuntimeMesh)->IndexCount() == 3u);
    CHECK((*SharedRuntimeMesh)->SubMeshCount() == 1u);
#endif
}

TEST_CASE("Runtime mesh stream source rebuilds submesh bounds from geometry when authored bounds are stale", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    StaticMeshAsset Mesh{};
    Mesh.Mesh.Name = "OffsetTriangle";
    Mesh.Mesh.BoundsMin = {10.0f, 20.0f, 30.0f};
    Mesh.Mesh.BoundsMax = {12.0f, 22.0f, 32.0f};
    Mesh.Mesh.SubMeshes.push_back({
        .IndexOffset = 0u,
        .IndexCount = 3u,
        .MaterialSlot = 0u,
        .BoundsMin = {0.0f, 0.0f, 0.0f},
        .BoundsMax = {0.0f, 0.0f, 0.0f},
    });

    const std::vector<std::array<float, 3>> Positions = {
        {10.0f, 20.0f, 30.0f},
        {12.0f, 20.0f, 30.0f},
        {10.0f, 22.0f, 32.0f},
    };
    Mesh.Streams.push_back({
        .Semantic = EMeshStreamSemantic::Position,
        .Bytes = BytesFromVector(Positions),
        .ElementCount = static_cast<std::uint32_t>(Positions.size()),
        .StrideBytes = sizeof(float) * 3u,
        .Compress = false,
    });

    const std::vector<std::uint32_t> Indices = {0u, 1u, 2u};
    Mesh.Streams.push_back({
        .Semantic = EMeshStreamSemantic::Index,
        .Bytes = BytesFromVector(Indices),
        .ElementCount = static_cast<std::uint32_t>(Indices.size()),
        .StrideBytes = sizeof(std::uint32_t),
        .Compress = false,
    });

    auto MeshJson = SerializeAuthoredAssetToJson(Mesh);
    REQUIRE(MeshJson);
    WriteTextFile(Root.Path / "Rendering" / "OffsetTriangle.staticmesh", *MeshJson);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto Manager = MakeSourceOnlyManager(Root.Path);
    TAssetRef<StaticMeshAsset> MeshRef("Rendering/OffsetTriangle.staticmesh");
    auto SharedRuntimeMesh = MeshRef.GetRuntimeShared<SnAPI::Graphics::IVertexStreamSource>(*Manager);
    REQUIRE(SharedRuntimeMesh);
    REQUIRE(*SharedRuntimeMesh);
    REQUIRE((*SharedRuntimeMesh)->SubMeshCount() == 1u);

    SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
    REQUIRE((*SharedRuntimeMesh)->SubMesh(0u, SubMesh));
    CHECK(SubMesh.BoundingBoxMin.x() == 10.0f);
    CHECK(SubMesh.BoundingBoxMin.y() == 20.0f);
    CHECK(SubMesh.BoundingBoxMin.z() == 30.0f);
    CHECK(SubMesh.BoundingBoxMax.x() == 12.0f);
    CHECK(SubMesh.BoundingBoxMax.y() == 22.0f);
    CHECK(SubMesh.BoundingBoxMax.z() == 32.0f);
#endif
}

TEST_CASE("Runtime mesh assets preserve baked material refs and reuse shared runtime instances", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    MaterialAsset Material{};
    Material.ShaderModule = "UnitTestShader";
    auto MaterialJson = SerializeAuthoredAssetToJson(Material);
    REQUIRE(MaterialJson);
    WriteTextFile(Root.Path / "Rendering" / "Surface.material", *MaterialJson);

    MaterialInstanceAsset MaterialInstance{};
    MaterialInstance.ParentMaterial.AssetName = "Rendering/Surface.material";
    auto MaterialInstanceJson = SerializeAuthoredAssetToJson(MaterialInstance);
    REQUIRE(MaterialInstanceJson);
    WriteTextFile(Root.Path / "Rendering" / "Surface.materialinstance", *MaterialInstanceJson);

    StaticMeshAsset StaticMesh{};
    StaticMesh.Mesh.Name = "RuntimeMesh";
    StaticMesh.Mesh.SubMeshes.push_back({
        .IndexOffset = 0u,
        .IndexCount = 3u,
        .MaterialSlot = 0u,
        .BoundsMin = {0.0f, 0.0f, 0.0f},
        .BoundsMax = {1.0f, 1.0f, 0.0f},
    });
    StaticMesh.Mesh.MaterialInstances.emplace_back("Rendering/Surface.materialinstance");

    const std::vector<std::array<float, 3>> Positions = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };
    StaticMesh.Streams.push_back({
        .Semantic = EMeshStreamSemantic::Position,
        .Bytes = BytesFromVector(Positions),
        .ElementCount = static_cast<std::uint32_t>(Positions.size()),
        .StrideBytes = sizeof(float) * 3u,
        .Compress = false,
    });

    const std::vector<std::uint32_t> Indices = {0u, 1u, 2u};
    StaticMesh.Streams.push_back({
        .Semantic = EMeshStreamSemantic::Index,
        .Bytes = BytesFromVector(Indices),
        .ElementCount = static_cast<std::uint32_t>(Indices.size()),
        .StrideBytes = sizeof(std::uint32_t),
        .Compress = false,
    });

    auto StaticMeshJson = SerializeAuthoredAssetToJson(StaticMesh);
    REQUIRE(StaticMeshJson);
    WriteTextFile(Root.Path / "Rendering" / "RuntimeMesh.staticmesh", *StaticMeshJson);

    SkeletalMeshAsset SkeletalMesh{};
    SkeletalMesh.BaseMesh = StaticMesh;
    SkeletalMesh.Bones = {};
    auto SkeletalMeshJson = SerializeAuthoredAssetToJson(SkeletalMesh);
    REQUIRE(SkeletalMeshJson);
    WriteTextFile(Root.Path / "Rendering" / "RuntimeMesh.skeletalmesh", *SkeletalMeshJson);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto Manager = MakeSourceOnlyManager(Root.Path);

    TAssetRef<StaticMeshAsset> StaticMeshRef("Rendering/RuntimeMesh.staticmesh");
    auto SharedStaticMesh = StaticMeshRef.GetRuntimeShared<StaticMeshRuntime>(*Manager);
    REQUIRE(SharedStaticMesh);
    REQUIRE(*SharedStaticMesh);
    REQUIRE((*SharedStaticMesh)->StreamSource);
    REQUIRE((*SharedStaticMesh)->MaterialRefs.size() == 1u);
    CHECK((*SharedStaticMesh)->MaterialRefs.front().GetAssetName() == "Rendering/Surface.materialinstance");

    auto SharedStaticMeshAgain = StaticMeshRef.GetRuntimeShared<StaticMeshRuntime>(*Manager);
    REQUIRE(SharedStaticMeshAgain);
    REQUIRE(*SharedStaticMeshAgain);
    CHECK((*SharedStaticMeshAgain).get() == (*SharedStaticMesh).get());

    TAssetRef<SkeletalMeshAsset> SkeletalMeshRef("Rendering/RuntimeMesh.skeletalmesh");
    auto SharedSkeletalMesh = SkeletalMeshRef.GetRuntimeShared<SkeletalMeshRuntime>(*Manager);
    REQUIRE(SharedSkeletalMesh);
    REQUIRE(*SharedSkeletalMesh);
    REQUIRE((*SharedSkeletalMesh)->StreamSource);
    REQUIRE((*SharedSkeletalMesh)->MaterialRefs.size() == 1u);
    CHECK((*SharedSkeletalMesh)->MaterialRefs.front().GetAssetName() == "Rendering/Surface.materialinstance");

    auto SharedSkeletalMeshAgain = SkeletalMeshRef.GetRuntimeShared<SkeletalMeshRuntime>(*Manager);
    REQUIRE(SharedSkeletalMeshAgain);
    REQUIRE(*SharedSkeletalMeshAgain);
    CHECK((*SharedSkeletalMeshAgain).get() == (*SharedSkeletalMesh).get());
#endif
}

TEST_CASE("Conduit class compilation resolves graph refs from authored source assets", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    Conduit::GraphAsset Graph{};
    Graph.Name = "CompiledFromSource";
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);
    WriteTextFile(Root.Path / "Conduit" / "CompiledFromSource.conduitgraph", *GraphJson);

    Conduit::ClassAsset Class{};
    Class.Name = "CompiledClass";
    Class.HostType = StaticTypeId<PawnBase>();
    Class.Graph.EditAssetName() = "Conduit/CompiledFromSource.conduitgraph";
    auto ClassJson = SerializeAuthoredAssetToJson(Class);
    REQUIRE(ClassJson);
    WriteTextFile(Root.Path / "Conduit" / "CompiledClass.conduitclass", *ClassJson);

    auto Manager = MakeSourceOnlyManager(Root.Path);
    TAssetRef<Conduit::ClassAsset> ClassRef("Conduit/CompiledClass.conduitclass");
    auto LoadedClass = ClassRef.LoadAsset();
    REQUIRE(LoadedClass);
    REQUIRE(*LoadedClass);

    auto CompileResult = Conduit::CompileClassAsset(**LoadedClass, *Manager);
    REQUIRE(CompileResult);
    CHECK(CompileResult->HostType == StaticTypeId<PawnBase>());
    CHECK(CompileResult->SourceGraph.Name == "CompiledFromSource");
}

TEST_CASE("Authored asset refs enumerate authored asset types by file extension", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    MaterialAsset Material{};
    Material.ShaderModule = "UnitTestShader";
    auto MaterialJson = SerializeAuthoredAssetToJson(Material);
    REQUIRE(MaterialJson);
    WriteTextFile(Root.Path / "Rendering" / "UnitTest.material", *MaterialJson);

    MaterialInstanceAsset MaterialInstance{};
    MaterialInstance.ParentMaterial.AssetName = "Rendering/UnitTest.material";
    auto MaterialInstanceJson = SerializeAuthoredAssetToJson(MaterialInstance);
    REQUIRE(MaterialInstanceJson);
    WriteTextFile(Root.Path / "Rendering" / "UnitTest.materialinstance", *MaterialInstanceJson);

    Conduit::GraphAsset Graph{};
    Graph.Name = "EnumerationGraph";
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);
    WriteTextFile(Root.Path / "Conduit" / "EnumerationGraph.conduitgraph", *GraphJson);

    auto Manager = MakeSourceOnlyManager(Root.Path);

    const auto MaterialEntries = TAssetRef<MaterialAsset>::EnumerateCompatibleAssets(*Manager);
    CHECK(std::any_of(MaterialEntries.begin(), MaterialEntries.end(), [](const TAssetRef<MaterialAsset>::TEntry& Entry) {
        return Entry.Name == "Rendering/UnitTest.material";
    }));
    CHECK_FALSE(std::any_of(MaterialEntries.begin(), MaterialEntries.end(), [](const TAssetRef<MaterialAsset>::TEntry& Entry) {
        return Entry.Name == "Rendering/UnitTest.materialinstance";
    }));
    CHECK_FALSE(std::any_of(MaterialEntries.begin(), MaterialEntries.end(), [](const TAssetRef<MaterialAsset>::TEntry& Entry) {
        return Entry.Name == "Conduit/EnumerationGraph.conduitgraph";
    }));

    const auto GraphEntries = TAssetRef<Conduit::GraphAsset>::EnumerateCompatibleAssets(*Manager);
    CHECK(std::any_of(GraphEntries.begin(), GraphEntries.end(), [](const TAssetRef<Conduit::GraphAsset>::TEntry& Entry) {
        return Entry.Name == "Conduit/EnumerationGraph.conduitgraph";
    }));
    CHECK_FALSE(std::any_of(GraphEntries.begin(), GraphEntries.end(), [](const TAssetRef<Conduit::GraphAsset>::TEntry& Entry) {
        return Entry.Name == "Rendering/UnitTest.material";
    }));
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

TEST_CASE("Authored asset JSON round-trips conduit node input defaults", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    Conduit::GraphAsset Graph{};
    Graph.Name = "NodeInputDefaults";
    Graph.Nodes = {
        Conduit::GraphNodeAsset{
            .Kind = Conduit::EGraphAssetNodeKind::SelfMethodCall,
            .MemberName = "SetLabelText",
            .InputDefaults = {
                Conduit::GraphNodeInputDefaultAsset{
                    .PinKey = "Arg0",
                    .Value = Conduit::SerializedValue::FromValue(std::string("Ready")).value(),
                },
            },
        },
    };

    auto JsonResult = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(JsonResult);

    const nlohmann::json Root = nlohmann::json::parse(*JsonResult);
    REQUIRE(Root["Asset"].contains("Nodes"));
    REQUIRE(Root["Asset"]["Nodes"].is_array());
    REQUIRE(Root["Asset"]["Nodes"].size() == 1);
    REQUIRE(Root["Asset"]["Nodes"][0].contains("InputDefaults"));
    REQUIRE(Root["Asset"]["Nodes"][0]["InputDefaults"].is_array());
    REQUIRE(Root["Asset"]["Nodes"][0]["InputDefaults"].size() == 1);
    CHECK(Root["Asset"]["Nodes"][0]["InputDefaults"][0]["PinKey"] == "Arg0");
    CHECK(Root["Asset"]["Nodes"][0]["InputDefaults"][0]["Value"]["Type"] == TTypeNameV<std::string>);
    CHECK(Root["Asset"]["Nodes"][0]["InputDefaults"][0]["Value"]["Value"] == "Ready");

    Conduit::GraphAsset Loaded{};
    std::vector<std::string> Diagnostics{};
    auto LoadResult = DeserializeAuthoredAssetFromJson(*JsonResult, Loaded, Diagnostics);
    REQUIRE(LoadResult);
    CHECK(Diagnostics.empty());
    REQUIRE(Loaded.Nodes.size() == 1);
    REQUIRE(Loaded.Nodes[0].InputDefaults.size() == 1);
    CHECK(Loaded.Nodes[0].InputDefaults[0].PinKey == "Arg0");
    CHECK(Loaded.Nodes[0].InputDefaults[0].Value.Type == StaticTypeId<std::string>());

    std::string DecodedValue{};
    REQUIRE(DeserializeReflectedValueInto(Loaded.Nodes[0].InputDefaults[0].Value.Type,
                                          &DecodedValue,
                                          Loaded.Nodes[0].InputDefaults[0].Value.Bytes.data(),
                                          Loaded.Nodes[0].InputDefaults[0].Value.Bytes.size()));
    CHECK(DecodedValue == "Ready");
}

TEST_CASE("Authored asset JSON load tolerates missing and unknown reflected fields", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    MaterialAsset Material{};
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

    MaterialInstanceAsset Material{};
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

TEST_CASE("Empty level assets round-trip through source payload serialization", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    LevelAsset Source{};

    std::vector<std::uint8_t> Bytes{};
    REQUIRE(SerializeLevelAsset(Source, Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto RoundTrip = DeserializeLevelAsset(Bytes.data(), Bytes.size());
    REQUIRE(RoundTrip);
    CHECK(RoundTrip->Name.empty());
    CHECK(RoundTrip->Nodes.empty());
    CHECK(RoundTrip->LogicalName.empty());
    CHECK(RoundTrip->AssetId.IsNull());
}

TEST_CASE("Level source payload binary round-trips preserve authored identity", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    LevelAsset Source{};
    Source.SetPersistentIdentity(::SnAPI::AssetPipeline::AssetId::Generate(), "Levels/Identity.level");

    std::vector<std::uint8_t> Bytes{};
    REQUIRE(SerializeLevelAsset(Source, Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto RoundTrip = DeserializeLevelAsset(Bytes.data(), Bytes.size());
    REQUIRE(RoundTrip);
    CHECK(RoundTrip->AssetId == Source.AssetId);
    CHECK(RoundTrip->LogicalName == Source.LogicalName);
    CHECK(RoundTrip->Name.empty());
    CHECK(RoundTrip->Nodes.empty());
}

TEST_CASE("Asset manager loads empty authored level source assets by logical name", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    TempDir Root{};

    LevelAsset Source{};
    auto LevelJson = SerializeAuthoredAssetToJson(Source);
    REQUIRE(LevelJson);
    WriteTextFile(Root.Path / "Levels" / "Empty.level", *LevelJson);

    auto Manager = MakeSourceOnlyManager(Root.Path);

    World LevelWorld("EmptyLevelLoadWorld");
    NodeHandle LoadedLevelHandle{};
    LevelAssetLoadParams LevelParams{};
    LevelParams.TargetWorld = &LevelWorld;
    LevelParams.OutCreatedLevel = &LoadedLevelHandle;

    auto LevelLoad = Manager->Load<Level>("Levels/Empty.level", LevelParams);
    REQUIRE(LevelLoad);
    REQUIRE(*LevelLoad);

    auto* LoadedLevel = NodeCast<Level>(LoadedLevelHandle.Borrowed());
    REQUIRE(LoadedLevel != nullptr);
    CHECK(LoadedLevel->Children().empty());
    CHECK(CountWorldNodes(LevelWorld) == 1);
}

TEST_CASE("Prefab capture resolves subtree components from UUID-only live node handles", "[Assets][Source]")
{
    RegisterBuiltinTypes();

    World SourceWorld("PrefabCaptureWorld");
    auto RootHandleResult = SourceWorld.CreateNode("PrefabRoot");
    REQUIRE(RootHandleResult);
    auto ChildHandleResult = SourceWorld.CreateNode("PrefabChild");
    REQUIRE(ChildHandleResult);
    NodeHandle RootHandle = *RootHandleResult;
    NodeHandle ChildHandle = *ChildHandleResult;
    REQUIRE(SourceWorld.AttachChild(RootHandle, ChildHandle));

    auto* RootNode = RootHandleResult->Borrowed();
    REQUIRE(RootNode != nullptr);
    auto* ChildNode = ChildHandleResult->Borrowed();
    REQUIRE(ChildNode != nullptr);

    auto RootTransform = RootNode->Add<TransformComponent>();
    REQUIRE(RootTransform);
    RootTransform->Position = Vec3(1.0, 2.0, 3.0);

    auto ChildTransform = ChildNode->Add<TransformComponent>();
    REQUIRE(ChildTransform);
    ChildTransform->Position = Vec3(4.0, 5.0, 6.0);

    // Mimic editor/serialized-handle flows where only UUID identity is available and the
    // component lookup must rehydrate the owner handle through the world.
    RootNode->Handle(NodeHandle{RootNode->Id()});
    ChildNode->Handle(NodeHandle{ChildNode->Id()});

    auto CaptureResult = CaptureNodeAsset(*RootNode);
    REQUIRE(CaptureResult);
    REQUIRE(CaptureResult->Nodes.size() == 1);

    const NodeObjectAsset& RootAsset = CaptureResult->Nodes.front();
    REQUIRE(RootAsset.Components.size() == 1);
    CHECK(RootAsset.Components.front().Type == StaticTypeId<TransformComponent>());
    REQUIRE(RootAsset.Children.size() == 1);
    REQUIRE(RootAsset.Children.front().Components.size() == 1);
    CHECK(RootAsset.Children.front().Components.front().Type == StaticTypeId<TransformComponent>());

    auto JsonResult = SerializeAuthoredAssetToJson(*CaptureResult);
    REQUIRE(JsonResult);

    NodeAsset RoundTripped{};
    REQUIRE(DeserializeAuthoredAssetFromJson(*JsonResult, RoundTripped));
    REQUIRE(RoundTripped.Nodes.size() == 1);
    REQUIRE(RoundTripped.Nodes.front().Components.size() == 1);
    CHECK(RoundTripped.Nodes.front().Components.front().Type == StaticTypeId<TransformComponent>());
    REQUIRE(RoundTripped.Nodes.front().Children.size() == 1);
    REQUIRE(RoundTripped.Nodes.front().Children.front().Components.size() == 1);
    CHECK(RoundTripped.Nodes.front().Children.front().Components.front().Type == StaticTypeId<TransformComponent>());
}

TEST_CASE("Prefab capture skips read-only Conduit class component diagnostics", "[Assets][Source][Conduit]")
{
    RegisterBuiltinTypes();

    World SourceWorld("ConduitPrefabCaptureWorld");
    auto RootHandleResult = SourceWorld.CreateNode("ConduitRoot");
    REQUIRE(RootHandleResult);

    BaseNode* RootNode = RootHandleResult->Borrowed();
    REQUIRE(RootNode != nullptr);

    auto ComponentResult = RootNode->Add<Conduit::ClassComponent>();
    REQUIRE(ComponentResult);
    ComponentResult->Class.EditAssetName() = "Conduit/TestClass.conduitclass";

    auto CaptureResult = CaptureNodeAsset(*RootNode);
    REQUIRE(CaptureResult);
    REQUIRE(CaptureResult->Nodes.size() == 1);
    REQUIRE(CaptureResult->Nodes.front().Components.size() == 1);

    const auto& Fields = CaptureResult->Nodes.front().Components.front().Fields;
    CHECK(std::any_of(Fields.begin(), Fields.end(), [](const NodeFieldAsset& Field) {
        return Field.Name == "Class";
    }));
    CHECK(std::none_of(Fields.begin(), Fields.end(), [](const NodeFieldAsset& Field) {
        return Field.Name == "Bound" || Field.Name == "LastError";
    }));

    auto JsonResult = SerializeAuthoredAssetToJson(*CaptureResult);
    REQUIRE(JsonResult);
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)

TEST_CASE("Authored asset JSON saves collider enum and flags fields semantically", "[Assets][Source][Physics]")
{
    RegisterBuiltinTypes();

    World SourceWorld("ColliderSemanticSaveWorld");
    auto NodeHandleResult = SourceWorld.CreateNode("ColliderNode");
    REQUIRE(NodeHandleResult);

    BaseNode* Node = NodeHandleResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto ColliderResult = Node->Add<ColliderComponent>();
    REQUIRE(ColliderResult);

    auto& Settings = ColliderResult->EditSettings();
    Settings.Shape = SnAPI::Physics::EShapeType::Sphere;
    Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
    Settings.Mask = CollisionMaskFlags(ECollisionFilterBits::WorldStatic)
                  | CollisionMaskFlags(ECollisionFilterBits::WorldDynamic);

    auto CaptureResult = CaptureNodeAsset(*Node);
    REQUIRE(CaptureResult);

    auto JsonResult = SerializeAuthoredAssetToJson(*CaptureResult);
    REQUIRE(JsonResult);

    const nlohmann::json Root = nlohmann::json::parse(*JsonResult);
    const auto& SettingsJson = Root["Asset"]["Nodes"][0]["Components"][0]["Fields"][0]["Value"]["Value"];

    REQUIRE(SettingsJson.contains("Shape"));
    CHECK(SettingsJson["Shape"] == "Sphere");

    REQUIRE(SettingsJson.contains("Layer"));
    REQUIRE(SettingsJson["Layer"].is_array());
    REQUIRE(SettingsJson["Layer"].size() == 1);
    CHECK(SettingsJson["Layer"][0] == "WorldStatic");

    REQUIRE(SettingsJson.contains("Mask"));
    REQUIRE(SettingsJson["Mask"].is_array());
    REQUIRE(SettingsJson["Mask"].size() == 2);
    CHECK(std::find(SettingsJson["Mask"].begin(), SettingsJson["Mask"].end(), "WorldStatic") != SettingsJson["Mask"].end());
    CHECK(std::find(SettingsJson["Mask"].begin(), SettingsJson["Mask"].end(), "WorldDynamic") != SettingsJson["Mask"].end());

    CHECK(JsonResult->find("\"$type\":\"SnAPI::Physics::EShapeType\"") == std::string::npos);
    CHECK(JsonResult->find("\"$type\":\"SnAPI::GameFramework::CollisionFilterFlags\"") == std::string::npos);
}

TEST_CASE("Asset manager loads legacy opaque collider enum and flags JSON from authored levels", "[Assets][Source][Physics]")
{
    RegisterBuiltinTypes();

    World SourceWorld("LegacyOpaqueColliderLevelSourceWorld");
    auto NodeHandleResult = SourceWorld.CreateNode("LegacyOpaqueColliderNode");
    REQUIRE(NodeHandleResult);

    BaseNode* Node = NodeHandleResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto ColliderResult = Node->Add<ColliderComponent>();
    REQUIRE(ColliderResult);

    auto& Settings = ColliderResult->EditSettings();
    Settings.Shape = SnAPI::Physics::EShapeType::Box;
    Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
    Settings.Mask = kCollisionMaskAll;

    auto CaptureResult = CaptureNodeAsset(*Node);
    REQUIRE(CaptureResult);

    LevelAsset LevelSource{};
    LevelSource.Name = "LegacyOpaqueLevel";
    LevelSource.Nodes = CaptureResult->Nodes;

    auto JsonResult = SerializeAuthoredAssetToJson(LevelSource);
    REQUIRE(JsonResult);

    nlohmann::json Root = nlohmann::json::parse(*JsonResult);
    auto& SettingsJson = Root["Asset"]["Nodes"][0]["Components"][0]["Fields"][0]["Value"]["Value"];
    SettingsJson["Shape"] = {
        {"$bytes", nlohmann::json::array({1})},
        {"$type", "SnAPI::Physics::EShapeType"},
    };
    SettingsJson["Layer"] = {
        {"$bytes", nlohmann::json::array({2, 0, 0, 0})},
        {"$type", "SnAPI::GameFramework::CollisionFilterFlags"},
    };
    SettingsJson["Mask"] = {
        {"$bytes", nlohmann::json::array({255, 255, 255, 255})},
        {"$type", "SnAPI::GameFramework::CollisionFilterFlags"},
    };

    TempDir RootDir{};
    WriteTextFile(RootDir.Path / "Levels" / "LegacyOpaque.level", Root.dump(2));

    auto Manager = MakeSourceOnlyManager(RootDir.Path);

    World LevelWorld("LegacyOpaqueColliderLevelWorld");
    NodeHandle LoadedLevelHandle{};
    LevelAssetLoadParams LevelParams{};
    LevelParams.TargetWorld = &LevelWorld;
    LevelParams.OutCreatedLevel = &LoadedLevelHandle;

    auto LevelLoad = Manager->Load<Level>("Levels/LegacyOpaque.level", LevelParams);
    REQUIRE(LevelLoad);
    REQUIRE(*LevelLoad);

    Level* LoadedLevel = NodeCast<Level>(LoadedLevelHandle.Borrowed());
    REQUIRE(LoadedLevel != nullptr);
    REQUIRE(LoadedLevel->Children().size() == 1);

    NodeHandle LoadedChildHandle = LoadedLevel->Children().front();
    BaseNode* LoadedChild = LoadedChildHandle.Borrowed();
    REQUIRE(LoadedChild != nullptr);

    auto LoadedCollider = LoadedChild->Component<ColliderComponent>();
    REQUIRE(LoadedCollider);
    CHECK(LoadedCollider->GetSettings().Shape == SnAPI::Physics::EShapeType::Box);
    CHECK(LoadedCollider->GetSettings().Layer == CollisionLayerFlags(ECollisionFilterBits::WorldDynamic));
    CHECK(LoadedCollider->GetSettings().Mask == kCollisionMaskAll);
}

#endif // SNAPI_GF_ENABLE_PHYSICS
