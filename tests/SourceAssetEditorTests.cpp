#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <typeindex>

#include <catch2/catch_test_macros.hpp>

#include "AuthoredAssetJson.h"
#include "Conduit/Editor/Service.h"
#include "Editor/EditorAssetService.h"
#include "Editor/IEditorService.h"
#include "GameFramework.hpp"
#include "PathResolver.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Editor;

namespace
{

struct SourceAssetEditorNodeHost : BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNodeHost";
};

void EnsureSourceAssetEditorNodeHostRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNodeHost>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorNodeHost>(SourceAssetEditorNodeHost::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

struct TempDir
{
    std::filesystem::path Path{};

    TempDir()
    {
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_source_editor_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

std::string ReadTextFile(const std::filesystem::path& Path)
{
    std::ifstream In(Path, std::ios::binary);
    REQUIRE(In.is_open());
    std::ostringstream Buffer{};
    Buffer << In.rdbuf();
    return Buffer.str();
}

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

struct TestEditorHost final : IEditorServiceHost
{
    GameRuntime Runtime{};
    EditorAssetService AssetService{};
    Conduit::Editor::ConduitEditorService ConduitService{};

    TestEditorHost()
    {
        REQUIRE(Runtime.Init({}));
        EditorServiceContext Context(*this);
        REQUIRE(AssetService.Initialize(Context));
        REQUIRE(ConduitService.Initialize(Context));
    }

    ~TestEditorHost() override
    {
        EditorServiceContext Context(*this);
        ConduitService.Shutdown(Context);
        AssetService.Shutdown(Context);
        Runtime.Shutdown();
    }

    [[nodiscard]] GameRuntime& RuntimeForServices() override
    {
        return Runtime;
    }

    [[nodiscard]] const GameRuntime& RuntimeForServices() const override
    {
        return Runtime;
    }

    [[nodiscard]] IEditorService* ResolveServiceForContext(const std::type_index& Type) override
    {
        if (Type == std::type_index(typeid(EditorAssetService)))
        {
            return &AssetService;
        }
        if (Type == std::type_index(typeid(Conduit::Editor::ConduitEditorService)))
        {
            return &ConduitService;
        }
        return nullptr;
    }

    [[nodiscard]] const IEditorService* ResolveServiceForContext(const std::type_index& Type) const override
    {
        if (Type == std::type_index(typeid(EditorAssetService)))
        {
            return &AssetService;
        }
        if (Type == std::type_index(typeid(Conduit::Editor::ConduitEditorService)))
        {
            return &ConduitService;
        }
        return nullptr;
    }
};

} // namespace

TEST_CASE("Editor asset discovery shows source files and skips cooked packs", "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    MaterialPayload Material{};
    Material.ShaderModule = "DiscoveryShader";
    auto MaterialJson = SerializeAuthoredAssetToJson(Material);
    REQUIRE(MaterialJson);

    Conduit::GraphAsset Graph{};
    Graph.Name = "DiscoveryGraph";
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);

    {
        std::error_code Ec{};
        std::filesystem::create_directories(Root.Path / "Levels", Ec);
        REQUIRE_FALSE(Ec);
        std::ofstream Pack(Root.Path / "Levels" / "Ignored.snpak", std::ios::binary | std::ios::trunc);
        REQUIRE(Pack.is_open());
        Pack << "not a real pack";
    }
    {
        std::error_code Ec{};
        std::filesystem::create_directories(Root.Path / "Rendering", Ec);
        REQUIRE_FALSE(Ec);
        std::ofstream Out(Root.Path / "Rendering" / "Visible.material", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(MaterialJson->data(), static_cast<std::streamsize>(MaterialJson->size()));
    }
    {
        std::error_code Ec{};
        std::filesystem::create_directories(Root.Path / "Conduit", Ec);
        REQUIRE_FALSE(Ec);
        std::ofstream Out(Root.Path / "Conduit" / "Visible.conduitgraph", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(GraphJson->data(), static_cast<std::streamsize>(GraphJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());

    const auto& Assets = Host.AssetService.Assets();
    CHECK_FALSE(Assets.empty());
    CHECK(std::none_of(Assets.begin(), Assets.end(), [](const EditorAssetService::DiscoveredAsset& Asset) {
        return Asset.Key.ends_with(".snpak");
    }));
    CHECK(std::any_of(Assets.begin(), Assets.end(), [](const EditorAssetService::DiscoveredAsset& Asset) {
        return Asset.Key == "Rendering/Visible.material" && Asset.AssetType == StaticTypeId<MaterialPayload>();
    }));
    CHECK(std::any_of(Assets.begin(), Assets.end(), [](const EditorAssetService::DiscoveredAsset& Asset) {
        return Asset.Key == "Conduit/Visible.conduitgraph" &&
               Asset.AssetType == StaticTypeId<Conduit::GraphAsset>();
    }));

    (void)Context;
}

TEST_CASE("Editor asset service can create and persist generic authored source assets", "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(Context, StaticTypeId<MaterialPayload>(), "UnitTestMaterial", "Rendering"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    const std::string CreatedSourcePath = Created->SourceFilePath;
    REQUIRE(CreatedKey == "Rendering/UnitTestMaterial.material");
    REQUIRE(std::filesystem::path(CreatedSourcePath).lexically_normal() ==
            (Root.Path / "Rendering" / "UnitTestMaterial.material").lexically_normal());

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<MaterialPayload>());
    auto* Material = static_cast<MaterialPayload*>(Session.TargetObject);
    REQUIRE(Material != nullptr);
    Material->ShaderModule = "SavedUnitTestShader";
    Material->FeatureAlphaBlend = true;

    Host.AssetService.TickAssetEditorSession(0.25f);
    CHECK(Host.AssetService.AssetEditorSession().RuntimeDirty);

    REQUIRE(Host.AssetService.SaveAssetByKey(CreatedKey));

    MaterialPayload SavedMaterial{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Rendering" / "UnitTestMaterial.material"),
        SavedMaterial));
    CHECK(SavedMaterial.ShaderModule == "SavedUnitTestShader");
    CHECK(SavedMaterial.FeatureAlphaBlend);

    REQUIRE(Host.AssetService.RenameAssetByKey("Rendering/UnitTestMaterial.material", "RenamedMaterial"));
    const auto* Renamed = Host.AssetService.SelectedAsset();
    REQUIRE(Renamed != nullptr);
    CHECK(Renamed->Key == "Rendering/RenamedMaterial.material");
    CHECK(std::filesystem::exists(Root.Path / "Rendering" / "RenamedMaterial.material"));
    CHECK_FALSE(std::filesystem::exists(Root.Path / "Rendering" / "UnitTestMaterial.material"));

    REQUIRE(Host.AssetService.DeleteAssetByKey("Rendering/RenamedMaterial.material"));
    CHECK_FALSE(std::filesystem::exists(Root.Path / "Rendering" / "RenamedMaterial.material"));
}

TEST_CASE("Editor asset service routes Conduit source assets through the Conduit document service", "[Assets][Editor][Source][Conduit]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::GraphAsset>(),
        "GameplayLogic",
        "Conduit"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Conduit/GameplayLogic.conduitgraph");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(Context, CreatedKey));
    CHECK_FALSE(Host.AssetService.AssetEditorSession().IsOpen);

    auto* Document = Host.ConduitService.FindDocument(CreatedKey);
    REQUIRE(Document != nullptr);
    CHECK(Document->Asset().Name.empty());

    auto AddedVariable = Host.ConduitService.CreateVariable("Health", StaticTypeId<int>());
    REQUIRE(AddedVariable);
    REQUIRE(Host.ConduitService.SelectVariable((*AddedVariable)->Id));
    REQUIRE(Host.ConduitService.SetSelectedVariableDefaultText("42"));

    auto EntryNode = Host.ConduitService.SpawnNode("entry.custom");
    REQUIRE(EntryNode);
    REQUIRE(Host.ConduitService.SetSelectedNodePrimaryText("OnInteract"));

    auto LabelNode = Host.ConduitService.SpawnNode("builtin.label");
    REQUIRE(LabelNode);
    REQUIRE(Host.ConduitService.SetSelectedNodePrimaryText("LoopStart"));

    auto BranchNode = Host.ConduitService.SpawnNode("builtin.branch");
    REQUIRE(BranchNode);
    REQUIRE(Host.ConduitService.SetSelectedNodePrimaryText("LoopStart"));
    REQUIRE(Host.ConduitService.SetSelectedNodeSecondaryText("LoopExit"));

    REQUIRE(Host.AssetService.SaveAssetByKey(Context, CreatedKey));

    Conduit::GraphAsset SavedGraph{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Conduit" / "GameplayLogic.conduitgraph"),
        SavedGraph));
    REQUIRE(SavedGraph.Variables.size() == 1);
    CHECK(SavedGraph.Variables.front().Name == "Health");
    CHECK(SavedGraph.Variables.front().Type == StaticTypeId<int>());
    REQUIRE(SavedGraph.Nodes.size() == 3);
    CHECK(SavedGraph.Nodes[0].Kind == Conduit::EGraphAssetNodeKind::EntryPoint);
    CHECK(SavedGraph.Nodes[0].EntryPointName == "OnInteract");
    CHECK(SavedGraph.Nodes[1].Kind == Conduit::EGraphAssetNodeKind::Label);
    CHECK(SavedGraph.Nodes[1].LabelName == "LoopStart");
    CHECK(SavedGraph.Nodes[2].Kind == Conduit::EGraphAssetNodeKind::Branch);
    CHECK(SavedGraph.Nodes[2].LabelName == "LoopStart");
    CHECK(SavedGraph.Nodes[2].FalseLabelName == "LoopExit");
}

TEST_CASE("Editor asset service routes Conduit class source assets through the Conduit document service",
          "[Assets][Editor][Source][Conduit]")
{
    EnsureSourceAssetEditorNodeHostRegistered();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::GraphAsset>(),
        "EnemyGraph",
        "Conduit"));
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::ClassAsset>(),
        "EnemyClass",
        "Conduit"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Conduit/EnemyClass.conduitclass");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(Context, CreatedKey));
    CHECK_FALSE(Host.AssetService.AssetEditorSession().IsOpen);

    auto* Document = Host.ConduitService.FindClassDocument(CreatedKey);
    REQUIRE(Document != nullptr);
    REQUIRE(Host.ConduitService.SetActiveClassHostType(StaticTypeId<SourceAssetEditorNodeHost>()));
    REQUIRE(Host.ConduitService.RenameActiveClass("EnemyController"));
    REQUIRE(Host.ConduitService.SetActiveClassGraph("Conduit/EnemyGraph.conduitgraph"));

    REQUIRE(Host.AssetService.SaveAssetByKey(Context, CreatedKey));

    Conduit::ClassAsset SavedClass{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Conduit" / "EnemyClass.conduitclass"),
        SavedClass));
    CHECK(SavedClass.Name == "EnemyController");
    CHECK(SavedClass.HostType == StaticTypeId<SourceAssetEditorNodeHost>());
    CHECK(SavedClass.Graph.GetAssetName() == "Conduit/EnemyGraph.conduitgraph");
}
