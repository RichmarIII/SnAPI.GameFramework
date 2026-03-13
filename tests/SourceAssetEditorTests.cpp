#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <typeindex>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AuthoredAssetJson.h"
#include "Conduit/Editor/Service.h"
#include "Editor/EditorAssetService.h"
#include "Editor/EditorPieService.h"
#include "Editor/IEditorService.h"
#include "GameFramework.hpp"
#include "PathResolver.h"
#include "TypeAutoRegistry.h"
#include "UIContext.h"
#include "UINumberField.h"
#include "UIPropertyPanel.h"
#include "UIText.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Editor;

namespace
{

struct SourceAssetEditorNodeHost : BaseNode, NodeCRTP<SourceAssetEditorNodeHost>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNodeHost";
};

struct SourceAssetEditorDefaultNode : BaseNode, NodeCRTP<SourceAssetEditorDefaultNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorDefaultNode";

    void OnCreate()
    {
        if (!Has<TransformComponent>())
        {
            (void)Add<TransformComponent>();
        }
    }
};

struct SourceAssetEditorNestedSettingsComponent : BaseComponent, ComponentCRTP<SourceAssetEditorNestedSettingsComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNestedSettingsComponent";

    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNestedSettingsComponent::Settings";

        float Scalar = 3.0f;
    };

    Settings& EditSettings() { return m_settings; }
    const Settings& GetSettings() const { return m_settings; }

private:
    Settings m_settings{};
};

struct SourceAssetEditorNestedSettingsNode : BaseNode, NodeCRTP<SourceAssetEditorNestedSettingsNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNestedSettingsNode";

    void OnCreate()
    {
        if (!Has<SourceAssetEditorNestedSettingsComponent>())
        {
            (void)Add<SourceAssetEditorNestedSettingsComponent>();
        }
    }
};

struct SourceAssetEditorCameraNode : BaseNode, NodeCRTP<SourceAssetEditorCameraNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorCameraNode";

    void OnCreate()
    {
        if (!Has<CameraComponent>())
        {
            (void)Add<CameraComponent>();
        }
    }
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

void EnsureSourceAssetEditorDefaultNodeRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorDefaultNode>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorDefaultNode>(SourceAssetEditorDefaultNode::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorNestedSettingsNodeRegistered()
{
    RegisterBuiltinTypes();

    if (!TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNestedSettingsComponent::Settings>()))
    {
        auto SettingsRegisterResult = TTypeBuilder<SourceAssetEditorNestedSettingsComponent::Settings>(
            SourceAssetEditorNestedSettingsComponent::Settings::kTypeName)
            .Field("Scalar", &SourceAssetEditorNestedSettingsComponent::Settings::Scalar)
            .Constructor<>()
            .Register();
        REQUIRE(SettingsRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNestedSettingsComponent>()))
    {
        auto ComponentRegisterResult = TTypeBuilder<SourceAssetEditorNestedSettingsComponent>(
            SourceAssetEditorNestedSettingsComponent::kTypeName)
            .Field("Settings",
                   &SourceAssetEditorNestedSettingsComponent::EditSettings,
                   &SourceAssetEditorNestedSettingsComponent::GetSettings)
            .Constructor<>()
            .Register();
        REQUIRE(ComponentRegisterResult);
    }

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNestedSettingsNode>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorNestedSettingsNode>(SourceAssetEditorNestedSettingsNode::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorCameraNodeRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorCameraNode>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorCameraNode>(SourceAssetEditorCameraNode::kTypeName)
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

#if defined(SNAPI_GF_ENABLE_UI)

void CollectElementAndDescendants(SnAPI::UI::UIContext& Context,
                                  const SnAPI::UI::ElementId Root,
                                  std::vector<SnAPI::UI::ElementId>& Out)
{
    if (Root.Value == 0)
    {
        return;
    }

    Out.push_back(Root);
    auto& Element = Context.GetElement(Root);
    for (std::uint32_t Index = 0; Index < Element.ChildCount(); ++Index)
    {
        CollectElementAndDescendants(Context, Element.ChildAt(Index).GetId(), Out);
    }
}

std::optional<SnAPI::UI::ElementId> FindTextElementByText(SnAPI::UI::UIContext& Context,
                                                          const SnAPI::UI::ElementId Root,
                                                          std::string_view Text)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        auto* Label = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(Id));
        if (!Label)
        {
            continue;
        }

        if (Label->Properties().GetPropertyOr(SnAPI::UI::UIText::TextKey, std::string{}) == Text)
        {
            return Id;
        }
    }

    return std::nullopt;
}

std::vector<SnAPI::UI::UINumberField*> FindNumberFieldsUnder(SnAPI::UI::UIContext& Context,
                                                             const SnAPI::UI::ElementId Root)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);

    std::vector<SnAPI::UI::UINumberField*> Result{};
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        if (auto* NumberField = dynamic_cast<SnAPI::UI::UINumberField*>(&Context.GetElement(Id)))
        {
            Result.push_back(NumberField);
        }
    }

    return Result;
}

#endif

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

TEST_CASE("Editor asset service creates typed prefabs that open in the hierarchy editor", "[Assets][Editor][Source]")
{
    EnsureSourceAssetEditorNodeHostRegistered();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<SourceAssetEditorNodeHost>(),
        "TypedEnemy",
        "Gameplay"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/TypedEnemy.prefab");
    REQUIRE(std::filesystem::exists(Root.Path / "Gameplay" / "TypedEnemy.prefab"));

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.CanEditHierarchy);
    REQUIRE(Session.TargetType == StaticTypeId<SourceAssetEditorNodeHost>());
    REQUIRE(Session.SelectedNode.IsValidSlowByUuid());
    REQUIRE(Session.Nodes.size() == 1);

    REQUIRE(Host.AssetService.AddAssetEditorComponent(Session.SelectedNode, StaticTypeId<TransformComponent>()));
    REQUIRE(Host.AssetService.AddAssetEditorNode(Session.SelectedNode, StaticTypeId<BaseNode>()));
    Host.AssetService.TickAssetEditorSession(0.25f);
    CHECK(Host.AssetService.AssetEditorSession().RuntimeDirty);

    const auto SaveResult = Host.AssetService.SaveActiveAssetEditor();
    INFO("save error: " << (SaveResult ? std::string("ok") : SaveResult.error().Message));
    REQUIRE(SaveResult);

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "TypedEnemy.prefab"),
        SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Type == StaticTypeId<SourceAssetEditorNodeHost>());
    REQUIRE(SavedPrefab.Nodes.front().Components.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Components.front().Type == StaticTypeId<TransformComponent>());
    REQUIRE(SavedPrefab.Nodes.front().Children.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Children.front().Type == StaticTypeId<BaseNode>());
}

TEST_CASE("Typed asset refs enumerate source prefabs before they are opened in the asset editor", "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<PawnBase>()));
#if defined(SNAPI_GF_ENABLE_RENDERER)
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<WorldRenderSettings>()));
#endif

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    World PawnWorld("TypedAssetRefEnumerationPawnWorld");
    auto PawnHandleResult = PawnWorld.CreateNode(StaticTypeId<PawnBase>(), "UnitPawn");
    REQUIRE(PawnHandleResult);
    auto PawnAssetResult = CaptureNodeAsset(*PawnHandleResult->Borrowed());
    REQUIRE(PawnAssetResult);
    auto PawnJson = SerializeAuthoredAssetToJson(*PawnAssetResult);
    REQUIRE(PawnJson);
    WriteTextFile(Root.Path / "Gameplay" / "UnitPawn.prefab", *PawnJson);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    World RenderWorld("TypedAssetRefEnumerationRenderWorld");
    auto RenderHandleResult = RenderWorld.CreateNode(StaticTypeId<WorldRenderSettings>(), "UnitRenderSettings");
    REQUIRE(RenderHandleResult);
    auto RenderAssetResult = CaptureNodeAsset(*RenderHandleResult->Borrowed());
    REQUIRE(RenderAssetResult);
    auto RenderJson = SerializeAuthoredAssetToJson(*RenderAssetResult);
    REQUIRE(RenderJson);
    WriteTextFile(Root.Path / "Rendering" / "UnitRenderSettings.prefab", *RenderJson);
#endif

    REQUIRE(Host.AssetService.RefreshDiscovery());

    const auto PawnEntries = TAssetRef<PawnBase>::EnumerateCompatibleAssets();
    CHECK(std::any_of(PawnEntries.begin(), PawnEntries.end(), [](const TAssetRef<PawnBase>::TEntry& Entry) {
        return Entry.Name == "Gameplay/UnitPawn.prefab";
    }));

#if defined(SNAPI_GF_ENABLE_RENDERER)
    const auto RenderEntries = TAssetRef<WorldRenderSettings>::EnumerateCompatibleAssets();
    CHECK(std::any_of(RenderEntries.begin(), RenderEntries.end(), [](const TAssetRef<WorldRenderSettings>::TEntry& Entry) {
        return Entry.Name == "Rendering/UnitRenderSettings.prefab";
    }));
#endif

    (void)Context;
}

TEST_CASE("Editor asset service creates PawnBase prefabs with registered default components",
          "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<PawnBase>()));

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    const auto CreateResult = Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<PawnBase>(),
        "TypedPawn",
        "Gameplay");
    REQUIRE(CreateResult);

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/TypedPawn.prefab");
    REQUIRE(std::filesystem::exists(Root.Path / "Gameplay" / "TypedPawn.prefab"));

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "TypedPawn.prefab"),
        SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Type == StaticTypeId<PawnBase>());
    CHECK(std::any_of(
        SavedPrefab.Nodes.front().Components.begin(),
        SavedPrefab.Nodes.front().Components.end(),
        [](const NodeComponentAsset& Component) {
            return Component.Type == StaticTypeId<TransformComponent>();
        }));

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<PawnBase>());

    auto* PawnNode = static_cast<PawnBase*>(Session.TargetObject);
    REQUIRE(PawnNode != nullptr);
    CHECK(PawnNode->Component<TransformComponent>());
#if defined(SNAPI_GF_ENABLE_RENDERER)
    CHECK(PawnNode->Component<CameraComponent>());
    CHECK(PawnNode->Component<SprintArmComponent>());
#endif
}

TEST_CASE("Typed prefabs persist default components and saved component settings", "[Assets][Editor][Source]")
{
    EnsureSourceAssetEditorDefaultNodeRegistered();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<SourceAssetEditorDefaultNode>(),
        "DefaultNode",
        "Gameplay"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/DefaultNode.prefab");

    NodeAsset CreatedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "DefaultNode.prefab"),
        CreatedPrefab));
    REQUIRE(CreatedPrefab.Nodes.size() == 1);
    CHECK(CreatedPrefab.Nodes.front().Type == StaticTypeId<SourceAssetEditorDefaultNode>());
    CHECK(std::any_of(
        CreatedPrefab.Nodes.front().Components.begin(),
        CreatedPrefab.Nodes.front().Components.end(),
        [](const NodeComponentAsset& Component) {
            return Component.Type == StaticTypeId<TransformComponent>();
        }));

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<SourceAssetEditorDefaultNode>());

    auto* Node = static_cast<SourceAssetEditorDefaultNode*>(Session.TargetObject);
    REQUIRE(Node != nullptr);
    NodeHandle NodeHandleValue = Node->Handle();
    auto* Transform = static_cast<TransformComponent*>(
        Node->World()->BorrowedComponent(NodeHandleValue, StaticTypeId<TransformComponent>()));
    REQUIRE(Transform != nullptr);
    Transform->Position = Vec3(12.0, 34.0, 56.0);

    Host.AssetService.TickAssetEditorSession(0.25f);
    REQUIRE(Host.AssetService.SaveAssetByKey(CreatedKey));

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* ReopenedNode = static_cast<SourceAssetEditorDefaultNode*>(Session.TargetObject);
    REQUIRE(ReopenedNode != nullptr);
    NodeHandle ReopenedNodeHandle = ReopenedNode->Handle();
    auto* ReopenedTransform = static_cast<TransformComponent*>(
        ReopenedNode->World()->BorrowedComponent(ReopenedNodeHandle, StaticTypeId<TransformComponent>()));
    REQUIRE(ReopenedTransform != nullptr);
    CHECK(ReopenedTransform->Position.x() == Catch::Approx(12.0));
    CHECK(ReopenedTransform->Position.y() == Catch::Approx(34.0));
    CHECK(ReopenedTransform->Position.z() == Catch::Approx(56.0));
}

#if defined(SNAPI_GF_ENABLE_UI)

TEST_CASE("UI property panel edits on typed prefabs persist component settings through save and reopen",
          "[Assets][Editor][Source][UI]")
{
    EnsureSourceAssetEditorCameraNodeRegistered();

    auto Host = std::make_unique<TestEditorHost>();
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(*Host);

    REQUIRE(Host->AssetService.RefreshDiscovery());
    REQUIRE(Host->AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<SourceAssetEditorCameraNode>(),
        "UICameraNode",
        "Gameplay"));

    const auto* Created = Host->AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/UICameraNode.prefab");

    REQUIRE(Host->AssetService.OpenAssetEditorByKey(CreatedKey));
    const auto Session = Host->AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* RootNode = static_cast<BaseNode*>(Session.TargetObject);
    REQUIRE(RootNode != nullptr);
    NodeHandle RootNodeHandle = RootNode->Handle();

    for (int Index = 0; Index < 8; ++Index)
    {
        auto ExtraNodeResult = RootNode->World()->CreateNode(
            StaticTypeId<SourceAssetEditorCameraNode>(),
            "ExtraCameraNode" + std::to_string(Index));
        REQUIRE(ExtraNodeResult);
        NodeHandle ExtraNodeHandle = *ExtraNodeResult;
        REQUIRE(RootNode->World()->RequestNodeOnCreate(ExtraNodeHandle));
    }

    RootNode = RootNode->World()->BorrowedNode(RootNodeHandle);
    REQUIRE(RootNode != nullptr);

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindNode(RootNode));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Fov Degrees");
    REQUIRE(LabelId.has_value());

    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto NumberFields = FindNumberFieldsUnder(*UiContext, RowId);
    REQUIRE(NumberFields.size() == 1);

    NumberFields.front()->Value().Set(91.0);
    UiContext->Tick(0.016f);

    auto* EditedComponent = static_cast<CameraComponent*>(
        RootNode->World()->BorrowedComponent(
            RootNodeHandle,
            StaticTypeId<CameraComponent>()));
    REQUIRE(EditedComponent != nullptr);
    CHECK(EditedComponent->GetSettings().FovDegrees == Catch::Approx(91.0f));

    Host->AssetService.TickAssetEditorSession(0.0f);
    const bool RuntimeDirty = Host->AssetService.AssetEditorSession().RuntimeDirty;
    CHECK(RuntimeDirty);
    REQUIRE(Host->AssetService.SaveActiveAssetEditor());

    Host->AssetService.CloseAssetEditor();
    REQUIRE(Host->AssetService.OpenAssetEditorByKey(CreatedKey));

    const auto ReopenedSession = Host->AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);

    auto* ReopenedNode = static_cast<BaseNode*>(ReopenedSession.TargetObject);
    REQUIRE(ReopenedNode != nullptr);

    NodeHandle ReopenedCameraNodeHandle = ReopenedNode->Handle();
    auto* ReopenedComponent = static_cast<CameraComponent*>(
        ReopenedNode->World()->BorrowedComponent(
            ReopenedCameraNodeHandle,
            StaticTypeId<CameraComponent>()));
    REQUIRE(ReopenedComponent != nullptr);
    CHECK(ReopenedComponent->GetSettings().FovDegrees == Catch::Approx(91.0f));
}

#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)

TEST_CASE("World render settings prefab saves referenced fog params without deadlocking", "[Assets][Editor][Source][Renderer]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<HeightFogParamsNode>()));
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<WorldRenderSettings>()));

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<HeightFogParamsNode>(),
        "UnitFogParams",
        "Rendering"));

    const auto* CreatedFog = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedFog != nullptr);
    const std::string FogAssetKey = CreatedFog->Key;
    const std::string FogAssetId = CreatedFog->AssetId.ToString();
    REQUIRE(FogAssetKey == "Rendering/UnitFogParams.prefab");

    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<WorldRenderSettings>(),
        "UnitWorldRenderSettings",
        "Rendering"));

    const auto* CreatedRenderSettings = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedRenderSettings != nullptr);
    const std::string RenderSettingsKey = CreatedRenderSettings->Key;
    REQUIRE(RenderSettingsKey == "Rendering/UnitWorldRenderSettings.prefab");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(RenderSettingsKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* SettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(SettingsNode != nullptr);

    SettingsNode->EditHeightFogParams().EditAssetName() = FogAssetKey;
    SettingsNode->EditHeightFogParams().EditAssetId() = FogAssetId;
    SettingsNode->EditorOnPropertyChanged("HeightFogParams");
    SettingsNode->EditorOnPropertyChanged("HeightFogParams");

    std::size_t FogChildCount = 0;
    for (const NodeHandle& ChildRef : SettingsNode->Children())
    {
        NodeHandle ChildHandle = ChildRef;
        auto* ChildNode = SettingsNode->World()->BorrowedNode(ChildHandle);
        if (ChildNode != nullptr &&
            TypeRegistry::Instance().IsA(ChildNode->TypeKey(), StaticTypeId<HeightFogParamsNode>()))
        {
            ++FogChildCount;
            CHECK(ChildNode->EditorTransient());
        }
    }
    CHECK(FogChildCount == 1);

    Host.AssetService.TickAssetEditorSession(0.25f);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    const std::string SavedJson = ReadTextFile(Root.Path / "Rendering" / "UnitWorldRenderSettings.prefab");
    CHECK(SavedJson.find("\"HeightFogParams\"") != std::string::npos);
    CHECK(SavedJson.find(FogAssetKey) != std::string::npos);
    CHECK(SavedJson.find(FogAssetId) != std::string::npos);

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(SavedJson, SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Children.empty());

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey(RenderSettingsKey));

    Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* ReopenedSettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(ReopenedSettingsNode != nullptr);
    CHECK(ReopenedSettingsNode->GetHeightFogParams().GetAssetName() == FogAssetKey);
    CHECK(ReopenedSettingsNode->GetHeightFogParams().GetAssetId() == FogAssetId);
}

TEST_CASE("Project default render settings do not duplicate authored world render settings roots during PIE",
          "[Assets][Editor][Source][Renderer][PIE]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<HeightFogParamsNode>()));
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<WorldRenderSettings>()));

    TestEditorHost Host{};
    EditorPieService PieService{};
    EditorServiceContext Context(Host);
    REQUIRE(PieService.Initialize(Context));

    TempDir Root{};
    const std::filesystem::path ProjectRoot = Root.Path / "Project";
    const std::filesystem::path AssetRootPath = ProjectRoot / "Assets";
    std::filesystem::create_directories(AssetRootPath);
    ScopedAssetRoot AssetRoot(AssetRootPath);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<HeightFogParamsNode>(),
        "ProjectFogParams",
        "Rendering"));

    const auto* CreatedFog = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedFog != nullptr);
    const std::string FogAssetKey = CreatedFog->Key;
    const std::string FogAssetId = CreatedFog->AssetId.ToString();

    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<WorldRenderSettings>(),
        "ProjectDefaultRenderSettings",
        "Rendering"));

    const auto* CreatedRenderSettings = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedRenderSettings != nullptr);
    const std::string RenderSettingsKey = CreatedRenderSettings->Key;
    const std::string RenderSettingsAssetId = CreatedRenderSettings->AssetId.ToString();

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(RenderSettingsKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* SettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(SettingsNode != nullptr);
    SettingsNode->EditHeightFogParams().EditAssetName() = FogAssetKey;
    SettingsNode->EditHeightFogParams().EditAssetId() = FogAssetId;
    SettingsNode->EditorOnPropertyChanged("HeightFogParams");
    Host.AssetService.TickAssetEditorSession(0.25f);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());
    Host.AssetService.CloseAssetEditor();

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
    WriteTextFile(AssetRootPath / "Levels" / "Startup.level", *LevelJson);

    const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";
    const std::string ProjectConfig =
        std::string("{\n") +
        "  \"version\": 1,\n"
        "  \"name\": \"WorldRenderSettingsPieProject\",\n"
        "  \"assetRoot\": \"Assets\",\n"
        "  \"startupLevelAsset\": \"Levels/Startup.level\",\n"
        "  \"defaultRenderSettings\": \"" + RenderSettingsAssetId + "\"\n"
        "}\n";
    WriteTextFile(ProjectFilePath, ProjectConfig);

    REQUIRE(Host.AssetService.LoadProject(Context, ProjectFilePath.string()));
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    REQUIRE(PieService.Play(Context));
    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    REQUIRE(PieService.Stop(Context));
    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    PieService.Shutdown(Context);
}

TEST_CASE("PIE stop clears transient fog nodes created during play", "[Assets][Editor][Source][Renderer][PIE]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<HeightFogParamsNode>()));

    TestEditorHost Host{};
    EditorPieService PieService{};
    EditorServiceContext Context(Host);
    REQUIRE(PieService.Initialize(Context));

    auto& WorldRef = Host.Runtime.World();
    REQUIRE(PieService.Play(Context));

    auto FogNodeResult = WorldRef.CreateNode<HeightFogParamsNode>("PieFog");
    REQUIRE(FogNodeResult.has_value());
    auto* FogNode = static_cast<HeightFogParamsNode*>(FogNodeResult->Borrowed());
    REQUIRE(FogNode != nullptr);

    FogNode->EditDensity() = 0.37f;
    FogNode->EditStartDistance() = 42.0f;
    FogNode->EditorOnPropertyChanged("Density");
    WorldRef.Tick(0.0f);

    CHECK(CountNodesOfType(WorldRef, StaticTypeId<HeightFogParamsNode>()) == 1);

    REQUIRE(PieService.Stop(Context));
    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);

    CHECK(CountNodesOfType(WorldRef, StaticTypeId<HeightFogParamsNode>()) == 0);

    PieService.Shutdown(Context);
}

#endif

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
