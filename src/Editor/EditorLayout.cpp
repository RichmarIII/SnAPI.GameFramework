#include "Editor/EditorLayout.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorSelectionModel.h"
#include "GameRuntime.h"
#include "RendererSystem.h"
#include "StaticTypeId.h"
#include "UIPropertyPanel.h"
#include "UIRenderViewport.h"
#include "UISystem.h"
#include "World.h"

#include <UIContext.h>
#include <UIElementBase.h>
#include <UIPanel.h>
#include <UIRadio.h>
#include <UIScrollContainer.h>
#include <UISizing.h>
#include <UIText.h>
#include <UITextInput.h>
#include <UIRealtimeGraph.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>

#include "CameraBase.hpp"

namespace SnAPI::GameFramework::Editor
{

Result EditorLayout::Build(GameRuntime& Runtime,
                           SnAPI::UI::Theme& Theme,
                           CameraComponent* ActiveCamera,
                           EditorSelectionModel* SelectionModel)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    (void)Theme;
    (void)ActiveCamera;
    (void)SelectionModel;
    return std::unexpected(MakeError(EErrorCode::NotSupported, "Editor layout requires renderer and UI support"));
#else
    Shutdown(&Runtime);

    if (!RegisterExternalElements(Runtime))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to register external editor UI elements"));
    }

    m_context = RootContext(Runtime);
    if (!m_context)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Root UI context is not available"));
    }

    m_context->SetActiveTheme(&Theme);
    BuildShell(*m_context, Runtime, ActiveCamera, SelectionModel);
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);

    m_built = true;
    return Ok();
#endif
}

void EditorLayout::Shutdown(GameRuntime* Runtime)
{
    (void)Runtime;
    m_context = nullptr;
    m_gameViewport = {};
    m_inspectorPropertyPanel = {};
    m_hierarchyListHost = {};
    m_hierarchyRowsRoot = {};
    m_hierarchyRowsByNode.clear();
    m_hierarchySignature = 0;
    m_hierarchyNodeCount = 0;
    m_selection = nullptr;
    m_onHierarchyNodeChosen.Reset();
    m_boundInspectorObject = nullptr;
    m_boundInspectorType = {};
    m_overlayContextId = 0;
    m_overlayPanel = {};
    m_overlayGraph = {};
    m_overlayFrameTimeLabel = {};
    m_overlayFpsLabel = {};
    m_overlayFrameTimeSeries = std::numeric_limits<std::uint32_t>::max();
    m_overlayFpsSeries = std::numeric_limits<std::uint32_t>::max();
    m_built = false;
}

void EditorLayout::Sync(GameRuntime& Runtime,
                        CameraComponent* ActiveCamera,
                        EditorSelectionModel* SelectionModel,
                        const float DeltaSeconds)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    (void)ActiveCamera;
    (void)SelectionModel;
    (void)DeltaSeconds;
#else
    if (!m_built)
    {
        return;
    }

    if (!m_context)
    {
        m_context = RootContext(Runtime);
        if (!m_context)
        {
            m_built = false;
            return;
        }
    }

    m_selection = SelectionModel;
    SyncHierarchy(Runtime, ActiveCamera);
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);
    EnsureGameViewportOverlay(Runtime);
    UpdateGameViewportOverlay(Runtime, DeltaSeconds);
#endif
}

bool EditorLayout::RegisterExternalElements(GameRuntime& Runtime)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return false;
#else
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return false;
    }

    auto& UI = WorldPtr->UI();
    const Result RegisterViewport = UI.RegisterElementType<UIRenderViewport>(SnAPI::UI::TypeHash<SnAPI::UI::UIPanel>());
    const Result RegisterPropertyPanel =
        UI.RegisterElementType<UIPropertyPanel>(SnAPI::UI::TypeHash<SnAPI::UI::UIScrollContainer>());
    return static_cast<bool>(RegisterViewport) && static_cast<bool>(RegisterPropertyPanel);
#endif
}

SnAPI::UI::UIContext* EditorLayout::RootContext(GameRuntime& Runtime) const
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return nullptr;
#else
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return nullptr;
    }

    const std::uint64_t RootContextId = WorldPtr->UI().RootContextId();
    if (RootContextId == 0)
    {
        return nullptr;
    }

    return WorldPtr->UI().Context(RootContextId);
#endif
}

void EditorLayout::BuildShell(SnAPI::UI::UIContext& Context,
                              GameRuntime& Runtime,
                              CameraComponent* ActiveCamera,
                              EditorSelectionModel* SelectionModel)
{
    auto Root = Context.Root();
    ConfigureRoot(Context);
    m_selection = SelectionModel;

    (void)BuildMenuBar(Root);
    (void)BuildToolbar(Root);
    BuildWorkspace(Root, Runtime, ActiveCamera, SelectionModel);
    BuildContentBrowser(Root);
}

void EditorLayout::ConfigureRoot(SnAPI::UI::UIContext& Context)
{
    auto Root = Context.Root();
    auto& RootPanel = Root.Element();
    RootPanel.ElementStyle().Apply("editor.root");
    RootPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    RootPanel.Padding().Set(0.0f);
    RootPanel.Gap().Set(0.0f);
    RootPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    RootPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    RootPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

EditorLayout::PanelBuilder EditorLayout::BuildMenuBar(PanelBuilder& Root)
{
    auto MenuBar = Root.Add(SnAPI::UI::UIPanel("Editor.MenuBar"));
    auto& MenuPanel = MenuBar.Element();
    MenuPanel.ElementStyle().Apply("editor.menu_bar");
    MenuPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    MenuPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    MenuPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Brand = MenuBar.Add(SnAPI::UI::UIText("SnAPI"));
    Brand.Element().ElementStyle().Apply("editor.panel_title");
    Brand.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 6.0f, 0.0f});

    auto Product = MenuBar.Add(SnAPI::UI::UIText("GameFramework"));
    Product.Element().ElementStyle().Apply("editor.panel_subtitle");
    Product.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 18.0f, 0.0f});

    constexpr std::array<std::string_view, 6> kMenuItems{"File", "Edit", "Assets", "Tools", "Window", "Help"};
    for (const std::string_view Label : kMenuItems)
    {
        auto Item = MenuBar.Add(SnAPI::UI::UIText(Label));
        Item.Element().ElementStyle().Apply("editor.menu_item");
        Item.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 10.0f, 0.0f});
    }

    return MenuBar;
}

EditorLayout::PanelBuilder EditorLayout::BuildToolbar(PanelBuilder& Root)
{
    auto Toolbar = Root.Add(SnAPI::UI::UIPanel("Editor.Toolbar"));
    auto& ToolbarPanel = Toolbar.Element();
    ToolbarPanel.ElementStyle().Apply("editor.toolbar");
    ToolbarPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ToolbarPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ToolbarPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto AddChip = [&](const std::string_view Label) {
        auto Chip = Toolbar.Add(SnAPI::UI::UIPanel("Editor.ToolbarChip"));
        auto& ChipPanel = Chip.Element();
        ChipPanel.ElementStyle().Apply("editor.toolbar_chip");
        ChipPanel.Width().Set(SnAPI::UI::Sizing::Auto());
        ChipPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        ChipPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 2.0f, 0.0f});

        auto ChipLabel = Chip.Add(SnAPI::UI::UIText(Label));
        ChipLabel.Element().ElementStyle().Apply("editor.menu_item");
        ChipLabel.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    };

    AddChip("Play");
    AddChip("Pause");
    AddChip("Step");
    AddChip("Perspective");
    AddChip("Lit");

    return Toolbar;
}

void EditorLayout::BuildWorkspace(PanelBuilder& Root,
                                  GameRuntime& Runtime,
                                  CameraComponent* ActiveCamera,
                                  EditorSelectionModel* SelectionModel)
{
    auto Workspace = Root.Add(SnAPI::UI::UIPanel("Editor.Workspace"));
    auto& WorkspacePanel = Workspace.Element();
    WorkspacePanel.ElementStyle().Apply("editor.workspace");
    WorkspacePanel.Width().Set(SnAPI::UI::Sizing::Fill());
    WorkspacePanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    WorkspacePanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    BuildHierarchyPane(Workspace, Runtime, ActiveCamera, SelectionModel);
    BuildGamePane(Workspace, Runtime, ActiveCamera);
    BuildInspectorPane(Workspace, ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
}

void EditorLayout::BuildContentBrowser(PanelBuilder& Root)
{
    auto ContentBrowser = Root.Add(SnAPI::UI::UIPanel("Editor.ContentBrowser"));
    auto& ContentPanel = ContentBrowser.Element();
    ContentPanel.ElementStyle().Apply("editor.content_browser");
    ContentPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ContentPanel.Height().Set(SnAPI::UI::Sizing::Fixed(224.0f));
    ContentPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto HeaderRow = ContentBrowser.Add(SnAPI::UI::UIPanel("Editor.ContentHeader"));
    auto& HeaderPanel = HeaderRow.Element();
    HeaderPanel.ElementStyle().Apply("editor.toolbar");
    HeaderPanel.Padding().Set(4.0f);
    HeaderPanel.Gap().Set(8.0f);
    HeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    constexpr std::array<std::string_view, 4> kContentTabs{"Content Browser", "Assets", "Prefabs", "Materials"};
    for (const std::string_view TabLabel : kContentTabs)
    {
        auto Tab = HeaderRow.Add(SnAPI::UI::UIText(TabLabel));
        Tab.Element().ElementStyle().Apply("editor.menu_item");
        Tab.Element().ElementMargin().Set(SnAPI::UI::Margin{2.0f, 0.0f, 8.0f, 0.0f});
    }

    auto CardScroll = ContentBrowser.Add(SnAPI::UI::UIScrollContainer{});
    auto& CardScrollElement = CardScroll.Element();
    CardScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    CardScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    CardScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    CardScrollElement.ShowHorizontalScrollbar().Set(true);
    CardScrollElement.ShowVerticalScrollbar().Set(false);
    CardScrollElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Cards = CardScroll.Add(SnAPI::UI::UIPanel("Editor.AssetCards"));
    auto& CardsPanel = Cards.Element();
    CardsPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    CardsPanel.Padding().Set(4.0f);
    CardsPanel.Gap().Set(10.0f);
    CardsPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    CardsPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    constexpr std::array<std::string_view, 8> kAssets{
        "Environment", "Props", "Character", "FX", "Textures", "MyPrefab", "SciFi_Floor", "Crate_Model"};
    for (const std::string_view AssetName : kAssets)
    {
        auto Card = Cards.Add(SnAPI::UI::UIPanel("Editor.AssetCard"));
        auto& CardPanel = Card.Element();
        CardPanel.ElementStyle().Apply("editor.asset_card");
        CardPanel.Width().Set(SnAPI::UI::Sizing::Fixed(140.0f));
        CardPanel.Height().Set(SnAPI::UI::Sizing::Fixed(160.0f));
        CardPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

        auto Preview = Card.Add(SnAPI::UI::UIPanel("Editor.AssetPreview"));
        auto& PreviewPanel = Preview.Element();
        PreviewPanel.ElementStyle().Apply("editor.asset_preview");
        PreviewPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        PreviewPanel.Height().Set(SnAPI::UI::Sizing::Fixed(104.0f));
        PreviewPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

        auto Label = Card.Add(SnAPI::UI::UIText(AssetName));
        Label.Element().ElementStyle().Apply("editor.menu_item");
        Label.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        Label.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 2.0f, 0.0f, 0.0f});
    }
}

void EditorLayout::BuildHierarchyPane(PanelBuilder& Workspace,
                                      GameRuntime& Runtime,
                                      CameraComponent* ActiveCamera,
                                      EditorSelectionModel* SelectionModel)
{
    auto Hierarchy = Workspace.Add(SnAPI::UI::UIPanel("Editor.Hierarchy"));
    auto& HierarchyPanel = Hierarchy.Element();
    HierarchyPanel.ElementStyle().Apply("editor.sidebar");
    HierarchyPanel.Width().Set(SnAPI::UI::Sizing::Ratio(0.38f));
    HierarchyPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    HierarchyPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Title = Hierarchy.Add(SnAPI::UI::UIText("Scene Hierarchy"));
    Title.Element().ElementStyle().Apply("editor.panel_title");
    Title.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Search = Hierarchy.Add(SnAPI::UI::UITextInput{});
    auto& SearchInput = Search.Element();
    SearchInput.ElementStyle().Apply("editor.search");
    SearchInput.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchInput.Height().Set(SnAPI::UI::Sizing::Fixed(30.0f));
    SearchInput.Placeholder().Set(std::string("Search..."));
    SearchInput.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto TreeScroll = Hierarchy.Add(SnAPI::UI::UIScrollContainer{});
    auto& TreeScrollElement = TreeScroll.Element();
    TreeScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TreeScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TreeScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TreeScrollElement.ShowVerticalScrollbar().Set(true);
    TreeScrollElement.ShowHorizontalScrollbar().Set(false);
    TreeScrollElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto TreePanel = TreeScroll.Add(SnAPI::UI::UIPanel("Editor.HierarchyTreeHost"));
    auto& TreePanelElement = TreePanel.Element();
    TreePanelElement.Padding().Set(4.0f);
    TreePanelElement.Gap().Set(3.0f);
    TreePanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TreePanelElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TreePanelElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    m_hierarchyListHost = TreePanel.Handle().Id;
    m_selection = SelectionModel;
    EnsureDefaultSelection(ActiveCamera);
    SyncHierarchy(Runtime, ActiveCamera);
}

void EditorLayout::EnsureDefaultSelection(CameraComponent* ActiveCamera)
{
    if (!m_selection || !m_selection->SelectedNode().IsNull())
    {
        return;
    }

    if (!ActiveCamera || ActiveCamera->Owner().IsNull())
    {
        return;
    }

    (void)m_selection->SelectNode(ActiveCamera->Owner());
}

void EditorLayout::SyncHierarchy(GameRuntime& Runtime, CameraComponent* ActiveCamera)
{
    if (!m_context || m_hierarchyListHost.Value == 0)
    {
        return;
    }

    EnsureDefaultSelection(ActiveCamera);

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return;
    }

    std::vector<HierarchyEntry> Entries{};
    if (!CollectHierarchyEntries(*WorldPtr, Entries))
    {
        return;
    }

    const std::uint64_t Signature = ComputeHierarchySignature(Entries);
    if (Signature != m_hierarchySignature || Entries.size() != m_hierarchyNodeCount || m_hierarchyRowsRoot.Value == 0)
    {
        RebuildHierarchyRows(Entries);
        m_hierarchySignature = Signature;
        m_hierarchyNodeCount = Entries.size();
    }

    SyncHierarchySelectionVisual();
}

bool EditorLayout::CollectHierarchyEntries(World& WorldRef, std::vector<HierarchyEntry>& OutEntries) const
{
    auto& Pool = WorldRef.NodePool();
    std::vector<NodeHandle> RootHandles{};
    Pool.ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Parent().IsNull())
        {
            RootHandles.push_back(Handle);
        }
    });

    const std::function<void(NodeHandle, int)> Visit = [&](const NodeHandle Handle, const int Depth) {
        auto* Node = Pool.Borrowed(Handle);
        if (!Node)
        {
            return;
        }

        OutEntries.push_back(HierarchyEntry{Handle, Depth, Node->Name()});

        for (const NodeHandle ChildHandle : Node->Children())
        {
            if (ChildHandle.IsNull())
            {
                continue;
            }

            if (!Pool.Borrowed(ChildHandle))
            {
                continue;
            }

            Visit(ChildHandle, Depth + 1);
        }
    };

    for (const NodeHandle RootHandle : RootHandles)
    {
        Visit(RootHandle, 0);
    }

    return true;
}

std::uint64_t EditorLayout::ComputeHierarchySignature(const std::vector<HierarchyEntry>& Entries) const
{
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;

    std::uint64_t Hash = kFnvOffset;
    const auto Mix = [&](const std::uint64_t Value) {
        Hash ^= Value;
        Hash *= kFnvPrime;
    };

    Mix(static_cast<std::uint64_t>(Entries.size()));
    for (const HierarchyEntry& Entry : Entries)
    {
        Mix(static_cast<std::uint64_t>(UuidHash{}(Entry.Handle.Id)));
        Mix(static_cast<std::uint64_t>(Entry.Depth));
        Mix(static_cast<std::uint64_t>(std::hash<std::string>{}(Entry.Label)));
    }

    return Hash;
}

void EditorLayout::RebuildHierarchyRows(const std::vector<HierarchyEntry>& Entries)
{
    if (!m_context || m_hierarchyListHost.Value == 0)
    {
        return;
    }

    if (m_hierarchyRowsRoot.Value != 0)
    {
        if (auto* ExistingRoot = dynamic_cast<SnAPI::UI::UIElementBase*>(&m_context->GetElement(m_hierarchyRowsRoot)))
        {
            ExistingRoot->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
        }
    }

    const auto RowsRootHandle = m_context->CreateElement<SnAPI::UI::UIPanel>("Editor.HierarchyRows");
    if (RowsRootHandle.Id.Value == 0)
    {
        return;
    }

    m_context->AddChild(m_hierarchyListHost, RowsRootHandle.Id);
    m_hierarchyRowsRoot = RowsRootHandle.Id;
    m_hierarchyRowsByNode.clear();

    if (auto* RowsRoot = dynamic_cast<SnAPI::UI::UIPanel*>(&m_context->GetElement(RowsRootHandle.Id)))
    {
        RowsRoot->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        RowsRoot->Padding().Set(0.0f);
        RowsRoot->Gap().Set(3.0f);
        RowsRoot->Width().Set(SnAPI::UI::Sizing::Fill());
    }

    const NodeHandle SelectedNode = (m_selection != nullptr) ? m_selection->SelectedNode() : NodeHandle{};

    for (const HierarchyEntry& Entry : Entries)
    {
        std::string Label(static_cast<std::size_t>(Entry.Depth) * 2u, ' ');
        if (Entry.Label.empty())
        {
            Label += "<unnamed>";
        }
        else
        {
            Label += Entry.Label;
        }

        const auto RowHandle = m_context->CreateElement<SnAPI::UI::UIRadio>(Label);
        if (RowHandle.Id.Value == 0)
        {
            continue;
        }

        m_context->AddChild(RowsRootHandle.Id, RowHandle.Id);
        m_hierarchyRowsByNode[Entry.Handle.Id] = RowHandle.Id;

        if (auto* Row = dynamic_cast<SnAPI::UI::UIRadio*>(&m_context->GetElement(RowHandle.Id)))
        {
            Row->ElementStyle().Apply("editor.hierarchy_item");
            Row->GroupId().Set(kHierarchyRadioGroup);
            Row->Width().Set(SnAPI::UI::Sizing::Fill());
            Row->Height().Set(SnAPI::UI::Sizing::Auto());
            Row->Selected().Set(Entry.Handle == SelectedNode);
            Row->OnSelected([this, Handle = Entry.Handle](const bool Selected) {
                if (Selected)
                {
                    OnHierarchyNodeChosen(Handle);
                }
            });
        }
    }

    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::SyncHierarchySelectionVisual()
{
    if (!m_context)
    {
        return;
    }

    const NodeHandle SelectedNode = (m_selection != nullptr) ? m_selection->SelectedNode() : NodeHandle{};

    for (const auto& [NodeId, RowId] : m_hierarchyRowsByNode)
    {
        auto* Row = dynamic_cast<SnAPI::UI::UIRadio*>(&m_context->GetElement(RowId));
        if (!Row)
        {
            continue;
        }

        const bool IsSelected = (!SelectedNode.IsNull() && SelectedNode.Id == NodeId);
        Row->Selected().Set(IsSelected);
    }
}

void EditorLayout::OnHierarchyNodeChosen(const NodeHandle Handle)
{
    if (m_onHierarchyNodeChosen)
    {
        m_onHierarchyNodeChosen(Handle);
        return;
    }

    if (!m_selection)
    {
        return;
    }

    (void)m_selection->SelectNode(Handle);
}

void EditorLayout::SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(NodeHandle)> Handler)
{
    m_onHierarchyNodeChosen = std::move(Handler);
}

BaseNode* EditorLayout::ResolveSelectedNode(GameRuntime& Runtime, CameraComponent* ActiveCamera) const
{
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return nullptr;
    }

    if (m_selection)
    {
        if (auto* SelectedNode = m_selection->ResolveSelectedNode(*WorldPtr))
        {
            return SelectedNode;
        }
    }

    if (ActiveCamera && !ActiveCamera->Owner().IsNull())
    {
        return WorldPtr->NodePool().Borrowed(ActiveCamera->Owner());
    }

    return nullptr;
}

void EditorLayout::BuildGamePane(PanelBuilder& Workspace, GameRuntime& Runtime, CameraComponent* ActiveCamera)
{
    auto GamePane = Workspace.Add(SnAPI::UI::UIPanel("Editor.GamePane"));
    auto& GamePaneElement = GamePane.Element();
    GamePaneElement.ElementStyle().Apply("editor.center");
    GamePaneElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    GamePaneElement.Height().Set(SnAPI::UI::Sizing::Fill());
    GamePaneElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Header = GamePane.Add(SnAPI::UI::UIPanel("Editor.GameHeader"));
    auto& HeaderPanel = Header.Element();
    HeaderPanel.ElementStyle().Apply("editor.toolbar");
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto HeaderTitle = Header.Add(SnAPI::UI::UIText("Game View"));
    HeaderTitle.Element().ElementStyle().Apply("editor.panel_title");
    HeaderTitle.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 14.0f, 0.0f});

    auto HeaderMode = Header.Add(SnAPI::UI::UIText("Perspective"));
    HeaderMode.Element().ElementStyle().Apply("editor.menu_item");
    HeaderMode.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 8.0f, 0.0f});

    auto HeaderLit = Header.Add(SnAPI::UI::UIText("Lit"));
    HeaderLit.Element().ElementStyle().Apply("editor.menu_item");
    HeaderLit.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Viewport = GamePane.Add(UIRenderViewport{});
    auto& ViewportElement = Viewport.Element();
    ViewportElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ViewportElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ViewportElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    ViewportElement.ViewportName().Set(std::string("Editor.GameViewport"));
    ViewportElement.PassGraphPreset().Set(ERenderViewportPassGraphPreset::DefaultWorld);
    ViewportElement.AutoRegisterPassGraph().Set(true);
    ViewportElement.RenderScale().Set(1.0f);
    ViewportElement.Enabled().Set(true);
    ViewportElement.SetGameRuntime(&Runtime);
    if (ActiveCamera && ActiveCamera->Camera())
    {
        ViewportElement.SetViewportCamera(ActiveCamera->Camera());
    }

    m_gameViewport = Viewport.Handle();
}

void EditorLayout::BuildInspectorPane(PanelBuilder& Workspace, BaseNode* SelectedNode, CameraComponent* ActiveCamera)
{
    auto Inspector = Workspace.Add(SnAPI::UI::UIPanel("Editor.Inspector"));
    auto& InspectorPanel = Inspector.Element();
    InspectorPanel.ElementStyle().Apply("editor.sidebar");
    InspectorPanel.Width().Set(SnAPI::UI::Sizing::Ratio(0.48f));
    InspectorPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    InspectorPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Title = Inspector.Add(SnAPI::UI::UIText("Inspector"));
    Title.Element().ElementStyle().Apply("editor.panel_title");
    Title.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Subtitle = Inspector.Add(SnAPI::UI::UIText("Selection"));
    Subtitle.Element().ElementStyle().Apply("editor.panel_subtitle");
    Subtitle.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto PropertyPanelBuilder = Inspector.Add(UIPropertyPanel{});
    auto& PropertyPanel = PropertyPanelBuilder.Element();
    PropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PropertyPanel.Padding().Set(6.0f);
    PropertyPanel.Gap().Set(4.0f);
    PropertyPanel.ShowHorizontalScrollbar().Set(true);
    PropertyPanel.ShowVerticalScrollbar().Set(true);
    PropertyPanel.Smooth().Set(true);
    PropertyPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    m_inspectorPropertyPanel = PropertyPanelBuilder.Handle();
    BindInspectorTarget(SelectedNode, ActiveCamera);
}

void EditorLayout::BindInspectorTarget(BaseNode* SelectedNode, CameraComponent* ActiveCamera)
{
    if (!m_context)
    {
        return;
    }

    auto* PropertyPanel = ResolveInspectorPanel();
    if (!PropertyPanel)
    {
        return;
    }

    void* TargetObject = nullptr;
    TypeId TargetType{};

    if (SelectedNode)
    {
        if (SelectedNode->Has<CameraComponent>())
        {
            auto CameraResult = SelectedNode->Component<CameraComponent>();
            if (CameraResult)
            {
                TargetObject = &*CameraResult;
                TargetType = StaticTypeId<CameraComponent>();
            }
        }

        if (!TargetObject)
        {
            TargetObject = SelectedNode;
            TargetType = SelectedNode->TypeKey();
        }
    }
    else if (ActiveCamera)
    {
        TargetObject = ActiveCamera;
        TargetType = StaticTypeId<CameraComponent>();
    }

    if (!TargetObject)
    {
        PropertyPanel->ClearObject();
        m_boundInspectorObject = nullptr;
        m_boundInspectorType = {};
        return;
    }

    if (m_boundInspectorObject == TargetObject && m_boundInspectorType == TargetType)
    {
        return;
    }

    (void)PropertyPanel->BindObject(TargetType, TargetObject);
    m_boundInspectorObject = TargetObject;
    m_boundInspectorType = TargetType;
}

void EditorLayout::SyncGameViewportCamera(GameRuntime& Runtime, CameraComponent* ActiveCamera)
{
    auto* Viewport = ResolveGameViewport();
    if (!Viewport)
    {
        return;
    }

    Viewport->SetGameRuntime(&Runtime);

    SnAPI::Graphics::ICamera* RenderCamera = nullptr;
    if (ActiveCamera)
    {
        RenderCamera = ActiveCamera->Camera();
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (!RenderCamera)
    {
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            RenderCamera = WorldPtr->Renderer().ActiveCamera();
        }
    }
#endif

    if (ActiveCamera && Viewport)
    {
        const SnAPI::UI::UIRect ViewRect = Viewport->LayoutRect();
        if (ViewRect.W > 0.0f && ViewRect.H > 0.0f)
        {
            auto& CameraSettings = ActiveCamera->EditSettings();
            CameraSettings.Aspect = ViewRect.W / ViewRect.H;
        }
    }

    Viewport->SetViewportCamera(RenderCamera);
}

void EditorLayout::EnsureGameViewportOverlay(GameRuntime& Runtime)
{
    auto* Viewport = ResolveGameViewport();
    auto* WorldPtr = Runtime.WorldPtr();
    if (!Viewport || !WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        m_overlayContextId = 0;
        m_overlayPanel = {};
        m_overlayGraph = {};
        m_overlayFrameTimeLabel = {};
        m_overlayFpsLabel = {};
        m_overlayFrameTimeSeries = std::numeric_limits<std::uint32_t>::max();
        m_overlayFpsSeries = std::numeric_limits<std::uint32_t>::max();
        return;
    }

    const std::uint64_t OverlayContextId = Viewport->OwnedContextId();
    if (OverlayContextId == 0)
    {
        m_overlayContextId = 0;
        m_overlayPanel = {};
        m_overlayGraph = {};
        m_overlayFrameTimeLabel = {};
        m_overlayFpsLabel = {};
        m_overlayFrameTimeSeries = std::numeric_limits<std::uint32_t>::max();
        m_overlayFpsSeries = std::numeric_limits<std::uint32_t>::max();
        return;
    }

    if (m_overlayContextId != OverlayContextId)
    {
        m_overlayContextId = OverlayContextId;
        m_overlayPanel = {};
        m_overlayGraph = {};
        m_overlayFrameTimeLabel = {};
        m_overlayFpsLabel = {};
        m_overlayFrameTimeSeries = std::numeric_limits<std::uint32_t>::max();
        m_overlayFpsSeries = std::numeric_limits<std::uint32_t>::max();
    }

    auto* OverlayContext = WorldPtr->UI().Context(m_overlayContextId);
    if (!OverlayContext)
    {
        return;
    }

    if (m_overlayGraph.Value != 0 && m_overlayFrameTimeLabel.Value != 0 && m_overlayFpsLabel.Value != 0)
    {
        auto* ExistingGraph = dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&OverlayContext->GetElement(m_overlayGraph));
        auto* ExistingFrameLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext->GetElement(m_overlayFrameTimeLabel));
        auto* ExistingFpsLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext->GetElement(m_overlayFpsLabel));
        if (ExistingGraph && ExistingFrameLabel && ExistingFpsLabel)
        {
            return;
        }
    }

    auto OverlayPanelBuilder = OverlayContext->Root().Add(SnAPI::UI::UIPanel("Editor.GameViewportOverlay"));
    auto& OverlayPanel = OverlayPanelBuilder.Element();
    OverlayPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Fixed(278.0f));
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Fixed(178.0f));
    OverlayPanel.HAlign().Set(SnAPI::UI::EAlignment::End);
    OverlayPanel.VAlign().Set(SnAPI::UI::EAlignment::Start);
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{12.0f, 12.0f, 12.0f, 12.0f});
    OverlayPanel.Padding().Set(8.0f);
    OverlayPanel.Gap().Set(4.0f);
    OverlayPanel.Background().Set(SnAPI::UI::Color{8, 14, 24, 214});
    OverlayPanel.BorderColor().Set(SnAPI::UI::Color{79, 118, 166, 214});
    OverlayPanel.BorderThickness().Set(1.0f);
    OverlayPanel.CornerRadius().Set(6.0f);
    OverlayPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto TitleBuilder = OverlayPanelBuilder.Add(SnAPI::UI::UIText("Realtime FrameGraph"));
    auto& Title = TitleBuilder.Element();
    Title.TextColor().Set(SnAPI::UI::Color{182, 220, 255, 255});
    Title.HAlign().Set(SnAPI::UI::EAlignment::Start);
    Title.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto StatsBuilder = OverlayPanelBuilder.Add(SnAPI::UI::UIPanel("Editor.GameViewportOverlay.Stats"));
    auto& StatsPanel = StatsBuilder.Element();
    StatsPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    StatsPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    StatsPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    StatsPanel.Gap().Set(12.0f);
    StatsPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    StatsPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto FrameTimeLabelBuilder = StatsBuilder.Add(SnAPI::UI::UIText("Frame: -- ms"));
    auto& FrameTimeLabel = FrameTimeLabelBuilder.Element();
    FrameTimeLabel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    FrameTimeLabel.TextColor().Set(SnAPI::UI::Color{136, 216, 255, 255});
    FrameTimeLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    FrameTimeLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    FrameTimeLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto FpsLabelBuilder = StatsBuilder.Add(SnAPI::UI::UIText("FPS: --"));
    auto& FpsLabel = FpsLabelBuilder.Element();
    FpsLabel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    FpsLabel.TextColor().Set(SnAPI::UI::Color{255, 206, 120, 255});
    FpsLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    FpsLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    FpsLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto GraphBuilder = OverlayPanelBuilder.Add(SnAPI::UI::UIRealtimeGraph("Frame Time / FPS"));
    auto& Graph = GraphBuilder.Element();
    Graph.Width().Set(SnAPI::UI::Sizing::Fill());
    Graph.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    Graph.SampleCapacity().Set(220u);
    Graph.AutoRange().Set(true);
    Graph.ShowLegend().Set(false);
    Graph.GridLinesX().Set(8u);
    Graph.GridLinesY().Set(4u);
    Graph.ContentPadding().Set(6.0f);
    Graph.LineThickness().Set(1.6f);
    Graph.ValuePrecision().Set(1u);
    Graph.BackgroundColor().Set(SnAPI::UI::Color{12, 20, 32, 220});
    Graph.PlotBackgroundColor().Set(SnAPI::UI::Color{17, 29, 46, 230});
    Graph.BorderColor().Set(SnAPI::UI::Color{89, 131, 184, 214});
    Graph.GridColor().Set(SnAPI::UI::Color{84, 119, 168, 88});
    Graph.AxisColor().Set(SnAPI::UI::Color{128, 172, 222, 176});
    Graph.TitleColor().Set(SnAPI::UI::Color{226, 236, 248, 255});
    Graph.LegendTextColor().Set(SnAPI::UI::Color{190, 208, 232, 255});
    Graph.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    const std::uint32_t FrameSeries = Graph.AddSeries("Frame ms", SnAPI::UI::Color{104, 198, 255, 255});
    const std::uint32_t FpsSeries = Graph.AddSeries("FPS", SnAPI::UI::Color{255, 182, 92, 255});
    if (FrameSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)Graph.SetSeriesRange(FrameSeries, 0.0f, 33.34f);
    }
    if (FpsSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)Graph.SetSeriesRange(FpsSeries, 0.0f, 240.0f);
    }

    m_overlayContextId = OverlayContextId;
    m_overlayPanel = OverlayPanelBuilder.Handle().Id;
    m_overlayGraph = GraphBuilder.Handle().Id;
    m_overlayFrameTimeLabel = FrameTimeLabelBuilder.Handle().Id;
    m_overlayFpsLabel = FpsLabelBuilder.Handle().Id;
    m_overlayFrameTimeSeries = FrameSeries;
    m_overlayFpsSeries = FpsSeries;
}

void EditorLayout::UpdateGameViewportOverlay(GameRuntime& Runtime, const float DeltaSeconds)
{
    if (!std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return;
    }

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized() || m_overlayContextId == 0 || m_overlayGraph.Value == 0)
    {
        return;
    }

    auto* OverlayContext = WorldPtr->UI().Context(m_overlayContextId);
    if (!OverlayContext)
    {
        return;
    }

    auto* Graph = dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&OverlayContext->GetElement(m_overlayGraph));
    if (!Graph || m_overlayFrameTimeSeries == std::numeric_limits<std::uint32_t>::max())
    {
        return;
    }

    const float FrameMs = std::clamp(DeltaSeconds * 1000.0f, 0.0f, 500.0f);
    const float FramesPerSecond = std::clamp(1.0f / DeltaSeconds, 0.0f, 2000.0f);
    (void)Graph->PushSample(m_overlayFrameTimeSeries, FrameMs);
    if (m_overlayFpsSeries != std::numeric_limits<std::uint32_t>::max())
    {
        (void)Graph->PushSample(m_overlayFpsSeries, FramesPerSecond);
    }

    if (m_overlayFrameTimeLabel.Value != 0)
    {
        if (auto* FrameLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext->GetElement(m_overlayFrameTimeLabel)))
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "Frame: %.2f ms", FrameMs);
            FrameLabel->Text().Set(std::string(Buffer));
        }
    }

    if (m_overlayFpsLabel.Value != 0)
    {
        if (auto* FpsLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext->GetElement(m_overlayFpsLabel)))
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "FPS: %.1f", FramesPerSecond);
            FpsLabel->Text().Set(std::string(Buffer));
        }
    }
}

UIRenderViewport* EditorLayout::GameViewport() const
{
    return ResolveGameViewport();
}

UIRenderViewport* EditorLayout::ResolveGameViewport() const
{
    if (!m_context || m_gameViewport.Id.Value == 0)
    {
        return nullptr;
    }

    return dynamic_cast<UIRenderViewport*>(&m_context->GetElement(m_gameViewport.Id));
}

UIPropertyPanel* EditorLayout::ResolveInspectorPanel() const
{
    if (!m_context || m_inspectorPropertyPanel.Id.Value == 0)
    {
        return nullptr;
    }

    return dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_inspectorPropertyPanel.Id));
}

} // namespace SnAPI::GameFramework::Editor
