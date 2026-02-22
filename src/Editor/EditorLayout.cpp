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
#include <UIColorPicker.h>
#include <UIDatePicker.h>
#include <UIDockZone.h>
#include <UIElementBase.h>
#include <UIImage.h>
#include <UIListView.h>
#include <UIMenuBar.h>
#include <UINumberField.h>
#include <UIPanel.h>
#include <UIPagination.h>
#include <UIScrollContainer.h>
#include <UISizing.h>
#include <UITable.h>
#include <UITabs.h>
#include <UITokenField.h>
#include <UIToolbar.h>
#include <UIText.h>
#include <UITextInput.h>
#include <UITreeView.h>
#include <UIBadge.h>
#include <UIBreadcrumbs.h>
#include <UIButton.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

#include "CameraBase.hpp"

namespace SnAPI::GameFramework::Editor
{
namespace
{
constexpr std::string_view kBrandIconUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/png/action/dashboard/materialicons/24dp/2x/baseline_dashboard_black_24dp.png";
constexpr std::string_view kHierarchyIconUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/png/action/list/materialicons/24dp/2x/baseline_list_black_24dp.png";
constexpr std::string_view kSearchIconUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/png/action/search/materialicons/24dp/2x/baseline_search_black_24dp.png";
constexpr std::string_view kGameViewIconUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/png/image/photo_camera/materialicons/24dp/2x/baseline_photo_camera_black_24dp.png";
constexpr std::string_view kInspectorIconUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/png/action/find_in_page/materialicons/24dp/2x/baseline_find_in_page_black_24dp.png";
constexpr std::array<std::string_view, 6> kMenuItems{
    "File", "Edit", "Assets", "Tools", "Window", "Help"};
constexpr std::array<std::string_view, 6> kToolbarActions{
    "Play", "Pause", "Step", "Move", "Rotate", "Scale"};
constexpr std::array<std::string_view, 3> kViewportModes{
    "Perspective", "Lit", "Shaded"};

constexpr std::array<std::string_view, 8> kAssetPreviewUrls{{
    "https://picsum.photos/seed/snapi_environment/512/320",
    "https://picsum.photos/seed/snapi_props/512/320",
    "https://picsum.photos/seed/snapi_character/512/320",
    "https://picsum.photos/seed/snapi_fx/512/320",
    "https://picsum.photos/seed/snapi_textures/512/320",
    "https://picsum.photos/seed/snapi_prefab/512/320",
    "https://picsum.photos/seed/snapi_floor/512/320",
    "https://picsum.photos/seed/snapi_crate/512/320",
}};

constexpr float kMainAreaSplitRatio = 0.68f;
constexpr float kWorkspaceLeftSplitRatio = 0.23f;
constexpr float kWorkspaceCenterSplitRatio = 0.74f;

void ConfigureSplitZone(SnAPI::UI::UIDockZone& Zone,
                        const SnAPI::UI::EDockSplit Direction,
                        const float SplitRatio,
                        const float MinPrimarySize,
                        const float MinSecondarySize)
{
    Zone.SplitDirection().Set(Direction);
    Zone.SplitRatio().Set(SplitRatio);
    Zone.MinPrimarySize().Set(MinPrimarySize);
    Zone.MinSecondarySize().Set(MinSecondarySize);
    Zone.Width().Set(SnAPI::UI::Sizing::Fill());
    Zone.Height().Set(SnAPI::UI::Sizing::Fill());
    Zone.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

void ConfigureHostPanel(SnAPI::UI::UIPanel& Panel)
{
    Panel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());
    Panel.Padding().Set(0.0f);
    Panel.Gap().Set(0.0f);
    Panel.UseGradient().Set(false);
    Panel.Background().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderThickness().Set(0.0f);
    Panel.CornerRadius().Set(0.0f);
    Panel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

[[nodiscard]] std::string ToLower(const std::string_view Text)
{
    std::string Out(Text);
    std::transform(Out.begin(), Out.end(), Out.begin(), [](const unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });
    return Out;
}

[[nodiscard]] bool LabelMatchesFilter(const std::string_view Label, const std::string& FilterLower)
{
    if (FilterLower.empty())
    {
        return true;
    }

    const std::string LabelLower = ToLower(Label);
    return LabelLower.find(FilterLower) != std::string::npos;
}
} // namespace

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

    m_runtime = &Runtime;
    m_invalidationDebugOverlayEnabled = QueryInvalidationDebugOverlayEnabled();
    m_context->SetActiveTheme(&Theme);
    BuildShell(*m_context, Runtime, ActiveCamera, SelectionModel);
    SyncInvalidationDebugOverlay();
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
    m_runtime = nullptr;
    m_gameViewTabs = {};
    m_gameViewport = {};
    m_inspectorPropertyPanel = {};
    m_hierarchyTree = {};
    m_invalidationDebugToggleLabel = {};
    m_hierarchyVisibleNodes.clear();
    m_hierarchySignature = 0;
    m_hierarchyNodeCount = 0;
    m_hierarchyVisualSelection = {};
    m_hierarchyFilterText.clear();
    m_selection = nullptr;
    m_onHierarchyNodeChosen.Reset();
    m_boundInspectorObject = nullptr;
    m_boundInspectorType = {};
    m_invalidationDebugOverlayEnabled = false;
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
    SyncInvalidationDebugOverlay();
    SyncHierarchy(Runtime, ActiveCamera);
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);
    (void)DeltaSeconds;
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

    BuildMenuBar(Root);
    BuildToolbar(Root);

    auto MainAreaSplit = Root.Add(SnAPI::UI::UIDockZone{});
    auto& MainAreaSplitElement = MainAreaSplit.Element();
    ConfigureSplitZone(MainAreaSplitElement, SnAPI::UI::EDockSplit::Vertical, kMainAreaSplitRatio, 220.0f, 140.0f);

    auto WorkspaceHost = MainAreaSplit.Add(SnAPI::UI::UIPanel("Editor.WorkspaceHost"));
    ConfigureHostPanel(WorkspaceHost.Element());
    BuildWorkspace(WorkspaceHost, Runtime, ActiveCamera, SelectionModel);

    auto BrowserHost = MainAreaSplit.Add(SnAPI::UI::UIPanel("Editor.ContentBrowserHost"));
    ConfigureHostPanel(BrowserHost.Element());
    BuildContentBrowser(BrowserHost);
}

void EditorLayout::ConfigureRoot(SnAPI::UI::UIContext& Context)
{
    auto Root = Context.Root();
    auto& RootPanel = Root.Element();
    RootPanel.ElementStyle().Apply("editor.root");
    // Force an opaque fullscreen root so uncovered regions never reveal desktop composition.
    RootPanel.UseGradient().Set(false);
    RootPanel.Background().Set(SnAPI::UI::Color{12, 13, 16, 255});
    RootPanel.BorderColor().Set(SnAPI::UI::Color{12, 13, 16, 255});
    RootPanel.BorderThickness().Set(0.0f);
    RootPanel.CornerRadius().Set(0.0f);
    RootPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    RootPanel.Padding().Set(0.0f);
    RootPanel.Gap().Set(0.0f);
    RootPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    RootPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    RootPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

void EditorLayout::BuildMenuBar(PanelBuilder& Root)
{
    auto MenuBar = Root.Add(SnAPI::UI::UIMenuBar{});
    auto& MenuBarElement = MenuBar.Element();
    MenuBarElement.ElementStyle().Apply("editor.menu_bar");
    MenuBarElement.Height().Set(SnAPI::UI::Sizing::Auto());
    MenuBarElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto BrandIcon = MenuBar.Add(SnAPI::UI::UIImage(kBrandIconUrl));
    auto& BrandIconImage = BrandIcon.Element();
    BrandIconImage.Width().Set(SnAPI::UI::Sizing::Fixed(16.0f));
    BrandIconImage.Height().Set(SnAPI::UI::Sizing::Fixed(16.0f));
    BrandIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    BrandIconImage.LazyLoad().Set(true);
    BrandIconImage.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 6.0f, 0.0f});

    auto Brand = MenuBar.Add(SnAPI::UI::UIText("SnAPI"));
    Brand.Element().ElementStyle().Apply("editor.brand_title");
    Brand.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 3.0f, 0.0f});

    auto Product = MenuBar.Add(SnAPI::UI::UIText("GameFramework"));
    Product.Element().ElementStyle().Apply("editor.brand_subtitle");
    Product.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 14.0f, 0.0f});

    for (std::size_t Index = 0; Index < kMenuItems.size(); ++Index)
    {
        auto Item = MenuBar.Add(SnAPI::UI::UIText(kMenuItems[Index]));
        auto& ItemText = Item.Element();
        ItemText.ElementStyle().Apply("editor.menu_item");
        ItemText.TextColor().Set(SnAPI::UI::Color{224, 228, 235, 255});
        ItemText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        ItemText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 10.0f, 0.0f});
    }

    auto Spacer = MenuBar.Add(SnAPI::UI::UIPanel("Editor.MenuSpacer"));
    auto& SpacerPanel = Spacer.Element();
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SpacerPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});

    auto InvalidationToggle = MenuBar.Add(SnAPI::UI::UIButton{});
    auto& InvalidationToggleButton = InvalidationToggle.Element();
    InvalidationToggleButton.ElementStyle().Apply("editor.menu_button");
    InvalidationToggleButton.Width().Set(SnAPI::UI::Sizing::Auto());
    InvalidationToggleButton.Height().Set(SnAPI::UI::Sizing::Auto());
    InvalidationToggleButton.ElementPadding().Set(SnAPI::UI::Padding{7.0f, 3.0f, 7.0f, 3.0f});
    InvalidationToggleButton.ElementMargin().Set(SnAPI::UI::Margin{8.0f, 0.0f, 0.0f, 0.0f});
    InvalidationToggleButton.OnClick([this]() { ToggleInvalidationDebugOverlay(); });

    auto InvalidationToggleLabel = InvalidationToggle.Add(SnAPI::UI::UIText{});
    auto& InvalidationToggleLabelText = InvalidationToggleLabel.Element();
    InvalidationToggleLabelText.ElementStyle().Apply("editor.menu_button_text");
    InvalidationToggleLabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
    InvalidationToggleLabelText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    m_invalidationDebugToggleLabel = InvalidationToggleLabel.Handle();
    UpdateInvalidationDebugToggleLabel();
}

void EditorLayout::BuildToolbar(PanelBuilder& Root)
{
    auto Toolbar = Root.Add(SnAPI::UI::UIToolbar{});
    auto& ToolbarElement = Toolbar.Element();
    ToolbarElement.ElementStyle().Apply("editor.toolbar");
    ToolbarElement.Height().Set(SnAPI::UI::Sizing::Auto());
    ToolbarElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    for (std::size_t Index = 0; Index < kToolbarActions.size(); ++Index)
    {
        auto Button = Toolbar.Add(SnAPI::UI::UIButton{});
        auto& ButtonElement = Button.Element();
        ButtonElement.ElementStyle().Apply("editor.toolbar_button");
        ButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
        ButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
        ButtonElement.ElementPadding().Set(SnAPI::UI::Padding{7.0f, 4.0f, 7.0f, 4.0f});
        ButtonElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 2.0f, 0.0f});

        auto Label = Button.Add(SnAPI::UI::UIText(kToolbarActions[Index]));
        auto& LabelText = Label.Element();
        LabelText.ElementStyle().Apply("editor.toolbar_button_text");
        LabelText.TextColor().Set(SnAPI::UI::Color{218, 223, 232, 255});
        LabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        LabelText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    }

    auto Spacer = Toolbar.Add(SnAPI::UI::UIPanel("Editor.ToolbarSpacer"));
    auto& SpacerPanel = Spacer.Element();
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SpacerPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});

    auto ModeBreadcrumbs = Toolbar.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& ModeBreadcrumbsElement = ModeBreadcrumbs.Element();
    ModeBreadcrumbsElement.ElementStyle().Apply("editor.modes_breadcrumb");
    ModeBreadcrumbsElement.SetCrumbs({std::string(kViewportModes[0]), std::string(kViewportModes[1])});
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
    WorkspacePanel.Height().Set(SnAPI::UI::Sizing::Fill());
    WorkspacePanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto WorkspaceSplit = Workspace.Add(SnAPI::UI::UIDockZone{});
    auto& WorkspaceSplitElement = WorkspaceSplit.Element();
    ConfigureSplitZone(WorkspaceSplitElement, SnAPI::UI::EDockSplit::Horizontal, kWorkspaceLeftSplitRatio, 210.0f, 360.0f);

    auto HierarchyHost = WorkspaceSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.HierarchyHost"));
    ConfigureHostPanel(HierarchyHost.Element());
    BuildHierarchyPane(HierarchyHost, Runtime, ActiveCamera, SelectionModel);

    auto CenterRightHost = WorkspaceSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.CenterRightHost"));
    ConfigureHostPanel(CenterRightHost.Element());

    auto CenterRightSplit = CenterRightHost.Add(SnAPI::UI::UIDockZone{});
    auto& CenterRightSplitElement = CenterRightSplit.Element();
    ConfigureSplitZone(CenterRightSplitElement, SnAPI::UI::EDockSplit::Horizontal, kWorkspaceCenterSplitRatio, 340.0f, 220.0f);

    auto GameHost = CenterRightSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.GameHost"));
    ConfigureHostPanel(GameHost.Element());
    BuildGamePane(GameHost, Runtime, ActiveCamera);

    auto InspectorHost = CenterRightSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.InspectorHost"));
    ConfigureHostPanel(InspectorHost.Element());
    BuildInspectorPane(InspectorHost, ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
}

void EditorLayout::BuildContentBrowser(PanelBuilder& Root)
{
    auto ContentBrowser = Root.Add(SnAPI::UI::UIPanel("Editor.ContentBrowser"));
    auto& ContentPanel = ContentBrowser.Element();
    ContentPanel.ElementStyle().Apply("editor.content_browser");
    ContentPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ContentPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ContentPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto HeaderRow = ContentBrowser.Add(SnAPI::UI::UIPanel("Editor.ContentHeader"));
    auto& HeaderPanel = HeaderRow.Element();
    HeaderPanel.ElementStyle().Apply("editor.content_header");
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto Path = HeaderRow.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& PathElement = Path.Element();
    PathElement.ElementStyle().Apply("editor.browser_path");
    PathElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PathElement.SetCrumbs({"Content", "Assets", "Environment"});

    auto HeaderSearch = HeaderRow.Add(SnAPI::UI::UITextInput{});
    auto& HeaderSearchInput = HeaderSearch.Element();
    HeaderSearchInput.ElementStyle().Apply("editor.search");
    HeaderSearchInput.Width().Set(SnAPI::UI::Sizing::Ratio(0.45f));
    HeaderSearchInput.Placeholder().Set(std::string("Search assets..."));

    auto BrowserTabs = ContentBrowser.Add(SnAPI::UI::UITabs{});
    auto& BrowserTabsElement = BrowserTabs.Element();
    BrowserTabsElement.ElementStyle().Apply("editor.browser_tabs");
    BrowserTabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    BrowserTabsElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    BrowserTabsElement.HeaderHeight().Set(28.0f);

    auto AssetsTab = BrowserTabs.Add(SnAPI::UI::UIPanel("Editor.ContentTab.Assets"));
    auto& AssetsTabPanel = AssetsTab.Element();
    AssetsTabPanel.ElementStyle().Apply("editor.section_card");
    AssetsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    AssetsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    AssetsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    AssetsTabPanel.Padding().Set(6.0f);
    AssetsTabPanel.Gap().Set(6.0f);

    auto AssetsList = AssetsTab.Add(SnAPI::UI::UIListView{});
    auto& AssetsListElement = AssetsList.Element();
    AssetsListElement.Orientation().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    AssetsListElement.ItemExtent().Set(152.0f);
    AssetsListElement.ItemGap().Set(10.0f);
    AssetsListElement.Virtualized().Set(false);
    AssetsListElement.Width().Set(SnAPI::UI::Sizing::Fill());
    AssetsListElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    AssetsListElement.ElementStyle().Apply("editor.browser_list");

    constexpr std::array<std::string_view, 8> kAssets{
        "Environment", "Props", "Character", "FX", "Textures", "MyPrefab", "SciFi_Floor", "Crate_Model"};
    for (std::size_t AssetIndex = 0; AssetIndex < kAssets.size(); ++AssetIndex)
    {
        auto Card = AssetsList.Add(SnAPI::UI::UIPanel("Editor.AssetCard"));
        auto& CardPanel = Card.Element();
        CardPanel.ElementStyle().Apply("editor.asset_card");
        CardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        CardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        CardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
        CardPanel.Padding().Set(6.0f);
        CardPanel.Gap().Set(4.0f);

        auto Preview = Card.Add(SnAPI::UI::UIPanel("Editor.AssetPreview"));
        auto& PreviewPanel = Preview.Element();
        PreviewPanel.ElementStyle().Apply("editor.asset_preview");
        PreviewPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        PreviewPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));

        auto PreviewImage = Preview.Add(SnAPI::UI::UIImage(kAssetPreviewUrls[AssetIndex]));
        auto& PreviewImageElement = PreviewImage.Element();
        PreviewImageElement.Width().Set(SnAPI::UI::Sizing::Fill());
        PreviewImageElement.Height().Set(SnAPI::UI::Sizing::Fill());
        PreviewImageElement.Mode().Set(SnAPI::UI::EImageMode::AspectFill);
        PreviewImageElement.LazyLoad().Set(true);

        auto Label = Card.Add(SnAPI::UI::UIText(kAssets[AssetIndex]));
        Label.Element().ElementStyle().Apply("editor.menu_item");
        Label.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    }

    auto BrowserPagination = AssetsTab.Add(SnAPI::UI::UIPagination{});
    auto& BrowserPaginationElement = BrowserPagination.Element();
    BrowserPaginationElement.ElementStyle().Apply("editor.browser_pagination");
    BrowserPaginationElement.PageCount().Set(6);
    BrowserPaginationElement.VisibleButtonCount().Set(6);
    BrowserPaginationElement.Width().Set(SnAPI::UI::Sizing::Fill());

    auto DetailsTab = BrowserTabs.Add(SnAPI::UI::UIPanel("Editor.ContentTab.Details"));
    auto& DetailsTabPanel = DetailsTab.Element();
    DetailsTabPanel.ElementStyle().Apply("editor.section_card");
    DetailsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    DetailsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DetailsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    DetailsTabPanel.Padding().Set(6.0f);
    DetailsTabPanel.Gap().Set(6.0f);

    auto DetailsTable = DetailsTab.Add(SnAPI::UI::UITable{});
    auto& DetailsTableElement = DetailsTable.Element();
    DetailsTableElement.ElementStyle().Apply("editor.browser_table");
    DetailsTableElement.ColumnCount().Set(2u);
    DetailsTableElement.RowHeight().Set(28.0f);
    DetailsTableElement.HeaderHeight().Set(28.0f);
    DetailsTableElement.Width().Set(SnAPI::UI::Sizing::Fill());
    DetailsTableElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    DetailsTableElement.SetColumnHeaders({"Field", "Value"});

    constexpr std::array<std::pair<std::string_view, std::string_view>, 5> kAssetMeta{{
        {"Name", "SciFi_Floor"},
        {"Type", "StaticMesh"},
        {"Triangles", "8,432"},
        {"Materials", "2"},
        {"Modified", "Today"},
    }};
    for (const auto& [FieldName, FieldValue] : kAssetMeta)
    {
        auto FieldCell = DetailsTable.Add(SnAPI::UI::UIText(FieldName));
        FieldCell.Element().ElementStyle().Apply("editor.menu_item");
        auto ValueCell = DetailsTable.Add(SnAPI::UI::UIText(FieldValue));
        ValueCell.Element().ElementStyle().Apply("editor.panel_title");
    }

    auto CollectionsTab = BrowserTabs.Add(SnAPI::UI::UIPanel("Editor.ContentTab.Collections"));
    auto& CollectionsTabPanel = CollectionsTab.Element();
    CollectionsTabPanel.ElementStyle().Apply("editor.section_card");
    CollectionsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    CollectionsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    CollectionsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    CollectionsTabPanel.Padding().Set(6.0f);
    CollectionsTabPanel.Gap().Set(6.0f);

    auto Tags = CollectionsTab.Add(SnAPI::UI::UITokenField{});
    auto& TagsElement = Tags.Element();
    TagsElement.ElementStyle().Apply("editor.token_field");
    TagsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TagsElement.AddToken("Environment", false);
    TagsElement.AddToken("Gameplay", false);
    TagsElement.AddToken("Favorites", false);

    auto Palette = CollectionsTab.Add(SnAPI::UI::UIColorPicker{});
    auto& PaletteElement = Palette.Element();
    PaletteElement.ElementStyle().Apply("editor.color_picker");
    PaletteElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PaletteElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    BrowserTabsElement.SetTabLabel(0, "Assets");
    BrowserTabsElement.SetTabLabel(1, "Details");
    BrowserTabsElement.SetTabLabel(2, "Collections");
}

void EditorLayout::BuildHierarchyPane(PanelBuilder& Workspace,
                                      GameRuntime& Runtime,
                                      CameraComponent* ActiveCamera,
                                      EditorSelectionModel* SelectionModel)
{
    auto Hierarchy = Workspace.Add(SnAPI::UI::UIPanel("Editor.Hierarchy"));
    auto& HierarchyPanel = Hierarchy.Element();
    HierarchyPanel.ElementStyle().Apply("editor.sidebar");
    HierarchyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HierarchyPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    HierarchyPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    HierarchyPanel.Padding().Set(6.0f);
    HierarchyPanel.Gap().Set(6.0f);

    auto TitleRow = Hierarchy.Add(SnAPI::UI::UIPanel("Editor.HierarchyTitleRow"));
    auto& TitleRowPanel = TitleRow.Element();
    TitleRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    TitleRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TitleRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    TitleRowPanel.Gap().Set(4.0f);
    TitleRowPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    TitleRowPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto TitleIcon = TitleRow.Add(SnAPI::UI::UIImage(kHierarchyIconUrl));
    auto& TitleIconImage = TitleIcon.Element();
    TitleIconImage.Width().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    TitleIconImage.Height().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    TitleIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    TitleIconImage.LazyLoad().Set(true);

    auto Title = TitleRow.Add(SnAPI::UI::UIText("Scene Hierarchy"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.panel_title");
    TitleText.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    TitleText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 4.0f, 0.0f});

    auto CountBadge = TitleRow.Add(SnAPI::UI::UIBadge("0"));
    CountBadge.Element().ElementStyle().Apply("editor.status_badge");
    CountBadge.Element().HorizontalPadding().Set(5.0f);
    CountBadge.Element().VerticalPadding().Set(2.0f);

    auto SearchRow = Hierarchy.Add(SnAPI::UI::UIPanel("Editor.HierarchySearchRow"));
    auto& SearchRowPanel = SearchRow.Element();
    SearchRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    SearchRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SearchRowPanel.Gap().Set(6.0f);
    SearchRowPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    SearchRowPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto SearchIcon = SearchRow.Add(SnAPI::UI::UIImage(kSearchIconUrl));
    auto& SearchIconImage = SearchIcon.Element();
    SearchIconImage.Width().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    SearchIconImage.Height().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    SearchIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    SearchIconImage.LazyLoad().Set(true);
    SearchIconImage.ElementMargin().Set(SnAPI::UI::Margin{2.0f, 0.0f, 0.0f, 0.0f});

    auto Search = SearchRow.Add(SnAPI::UI::UITextInput{});
    auto& SearchInput = Search.Element();
    SearchInput.ElementStyle().Apply("editor.search");
    SearchInput.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchInput.Height().Set(SnAPI::UI::Sizing::Auto());
    SearchInput.Placeholder().Set(std::string("Search..."));
    SearchInput.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    SearchInput.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_hierarchyFilterText = ToLower(Value);
        m_hierarchySignature = 0;
        m_hierarchyNodeCount = 0;
    }));

    auto Tree = Hierarchy.Add(SnAPI::UI::UITreeView{});
    auto& TreeElement = Tree.Element();
    TreeElement.ElementStyle().Apply("editor.tree");
    TreeElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TreeElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TreeElement.RowHeight().Set(24.0f);
    TreeElement.PaddingX().Set(6.0f);
    TreeElement.PaddingY().Set(4.0f);
    TreeElement.OnSelectionChanged(SnAPI::UI::TDelegate<void(int32_t)>::Bind([this](const int32_t VisibleIndex) {
        if (VisibleIndex < 0 || static_cast<std::size_t>(VisibleIndex) >= m_hierarchyVisibleNodes.size())
        {
            return;
        }

        OnHierarchyNodeChosen(m_hierarchyVisibleNodes[static_cast<std::size_t>(VisibleIndex)]);
    }));
    m_hierarchyTree = Tree.Handle();

    auto HierarchyPager = Hierarchy.Add(SnAPI::UI::UIPagination{});
    auto& HierarchyPagerElement = HierarchyPager.Element();
    HierarchyPagerElement.ElementStyle().Apply("editor.browser_pagination");
    HierarchyPagerElement.PageCount().Set(1);
    HierarchyPagerElement.VisibleButtonCount().Set(1);
    HierarchyPagerElement.ButtonWidth().Set(26.0f);
    HierarchyPagerElement.Width().Set(SnAPI::UI::Sizing::Fill());

    m_selection = SelectionModel;
    EnsureDefaultSelection(ActiveCamera);
    SyncHierarchy(Runtime, ActiveCamera);

    if (m_selection)
    {
        std::size_t NodeCount = 0;
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            WorldPtr->NodePool().ForEach([&](const NodeHandle&, BaseNode&) { ++NodeCount; });
        }
        CountBadge.Element().Text().Set(std::to_string(NodeCount));
    }
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
    if (!m_context || m_hierarchyTree.Id.Value == 0)
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

    if (!m_hierarchyFilterText.empty())
    {
        Entries.erase(
            std::remove_if(Entries.begin(), Entries.end(), [this](const HierarchyEntry& Entry) {
                return !LabelMatchesFilter(Entry.Label, m_hierarchyFilterText);
            }),
            Entries.end());
    }

    const NodeHandle SelectedNode = (m_selection != nullptr) ? m_selection->SelectedNode() : NodeHandle{};
    const std::uint64_t Signature = ComputeHierarchySignature(Entries);
    if (Signature != m_hierarchySignature ||
        Entries.size() != m_hierarchyNodeCount ||
        SelectedNode != m_hierarchyVisualSelection)
    {
        RebuildHierarchyTree(Entries, SelectedNode);
        m_hierarchySignature = Signature;
        m_hierarchyNodeCount = Entries.size();
        m_hierarchyVisualSelection = SelectedNode;
    }
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

    std::vector<std::pair<NodeHandle, int>> Stack{};
    Stack.reserve(RootHandles.size());
    for (auto It = RootHandles.rbegin(); It != RootHandles.rend(); ++It)
    {
        Stack.emplace_back(*It, 0);
    }

    while (!Stack.empty())
    {
        const auto [Handle, Depth] = Stack.back();
        Stack.pop_back();

        auto* Node = Pool.Borrowed(Handle);
        if (!Node)
        {
            continue;
        }

        OutEntries.push_back(HierarchyEntry{Handle, Depth, Node->Name()});

        const auto& Children = Node->Children();
        for (auto ChildIt = Children.rbegin(); ChildIt != Children.rend(); ++ChildIt)
        {
            const NodeHandle ChildHandle = *ChildIt;
            if (ChildHandle.IsNull() || !Pool.Borrowed(ChildHandle))
            {
                continue;
            }

            Stack.emplace_back(ChildHandle, Depth + 1);
        }
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

void EditorLayout::RebuildHierarchyTree(const std::vector<HierarchyEntry>& Entries, const NodeHandle SelectedNode)
{
    if (!m_context || m_hierarchyTree.Id.Value == 0)
    {
        return;
    }

    auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_hierarchyTree.Id));
    if (!Tree)
    {
        return;
    }

    std::vector<SnAPI::UI::UITreeItem> TreeItems{};
    TreeItems.reserve(Entries.size());
    m_hierarchyVisibleNodes.clear();
    m_hierarchyVisibleNodes.reserve(Entries.size());

    for (std::size_t Index = 0; Index < Entries.size(); ++Index)
    {
        const HierarchyEntry& Entry = Entries[Index];
        const std::string LabelBase = Entry.Label.empty() ? std::string("<unnamed>") : Entry.Label;
        std::string Label = LabelBase;
        if (!SelectedNode.IsNull() && Entry.Handle == SelectedNode)
        {
            Label = "\u2022 " + LabelBase;
        }

        bool HasChildren = false;
        if ((Index + 1u) < Entries.size())
        {
            HasChildren = Entries[Index + 1u].Depth > Entry.Depth;
        }

        TreeItems.push_back(SnAPI::UI::UITreeItem{
            .Label = std::move(Label),
            .Depth = static_cast<uint32_t>(std::max(0, Entry.Depth)),
            .HasChildren = HasChildren,
            .Expanded = true,
        });
        m_hierarchyVisibleNodes.push_back(Entry.Handle);
    }

    Tree->SetItems(std::move(TreeItems));
    m_context->MarkLayoutDirty();
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

bool EditorLayout::QueryInvalidationDebugOverlayEnabled() const
{
#if !defined(SNAPI_GF_ENABLE_UI)
    return false;
#else
    if (!m_runtime)
    {
        return false;
    }

    auto* WorldPtr = m_runtime->WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return false;
    }

    auto& UI = WorldPtr->UI();
    const std::uint64_t RootContextId = UI.RootContextId();
    if (RootContextId == 0)
    {
        return false;
    }

    const auto* RootContext = UI.Context(RootContextId);
    return RootContext != nullptr && RootContext->IsInvalidationDebugOverlayEnabled();
#endif
}

void EditorLayout::SetInvalidationDebugOverlayEnabled(const bool Enabled)
{
    m_invalidationDebugOverlayEnabled = Enabled;

#if defined(SNAPI_GF_ENABLE_UI)
    if (m_runtime)
    {
        if (auto* WorldPtr = m_runtime->WorldPtr(); WorldPtr && WorldPtr->UI().IsInitialized())
        {
            auto& UI = WorldPtr->UI();
            const auto ContextIds = UI.ContextIds();
            for (const std::uint64_t ContextId : ContextIds)
            {
                if (auto* Context = UI.Context(ContextId))
                {
                    Context->SetInvalidationDebugOverlayEnabled(Enabled);
                }
            }
        }
    }
#endif

    UpdateInvalidationDebugToggleLabel();
}

void EditorLayout::ToggleInvalidationDebugOverlay()
{
    SetInvalidationDebugOverlayEnabled(!m_invalidationDebugOverlayEnabled);
}

void EditorLayout::SyncInvalidationDebugOverlay()
{
#if !defined(SNAPI_GF_ENABLE_UI)
    return;
#else
    if (!m_runtime)
    {
        return;
    }

    auto* WorldPtr = m_runtime->WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return;
    }

    auto& UI = WorldPtr->UI();
    const auto ContextIds = UI.ContextIds();
    for (const std::uint64_t ContextId : ContextIds)
    {
        if (auto* Context = UI.Context(ContextId))
        {
            if (Context->IsInvalidationDebugOverlayEnabled() != m_invalidationDebugOverlayEnabled)
            {
                Context->SetInvalidationDebugOverlayEnabled(m_invalidationDebugOverlayEnabled);
            }
        }
    }
#endif
}

void EditorLayout::UpdateInvalidationDebugToggleLabel()
{
    if (!m_context || m_invalidationDebugToggleLabel.Id.Value == 0)
    {
        return;
    }

    auto* Label = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_invalidationDebugToggleLabel.Id));
    if (!Label)
    {
        return;
    }

    Label->Text().Set(m_invalidationDebugOverlayEnabled ? std::string("InvDbg: ON") : std::string("InvDbg: OFF"));
    Label->TextColor().Set(m_invalidationDebugOverlayEnabled
                               ? SnAPI::UI::Color{184, 238, 198, 255}
                               : SnAPI::UI::Color{224, 228, 235, 255});
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
    GamePaneElement.Width().Set(SnAPI::UI::Sizing::Fill());
    GamePaneElement.Height().Set(SnAPI::UI::Sizing::Fill());
    GamePaneElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto ViewTabs = GamePane.Add(SnAPI::UI::UITabs{});
    auto& ViewTabsElement = ViewTabs.Element();
    ViewTabsElement.ElementStyle().Apply("editor.viewport_tabs");
    ViewTabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ViewTabsElement.Height().Set(SnAPI::UI::Sizing::Fill());
    ViewTabsElement.HeaderHeight().Set(30.0f);
    m_gameViewTabs = ViewTabs.Handle();

    auto GameViewTab = ViewTabs.Add(SnAPI::UI::UIPanel("Editor.GameViewTab"));
    auto& GameViewTabPanel = GameViewTab.Element();
    GameViewTabPanel.ElementStyle().Apply("editor.section_card");
    GameViewTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    GameViewTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    GameViewTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    GameViewTabPanel.Padding().Set(4.0f);
    GameViewTabPanel.Gap().Set(4.0f);

    auto Header = GameViewTab.Add(SnAPI::UI::UIPanel("Editor.GameHeader"));
    auto& HeaderPanel = Header.Element();
    HeaderPanel.ElementStyle().Apply("editor.toolbar");
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto HeaderIcon = Header.Add(SnAPI::UI::UIImage(kGameViewIconUrl));
    auto& HeaderIconImage = HeaderIcon.Element();
    HeaderIconImage.Width().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    HeaderIconImage.Height().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    HeaderIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    HeaderIconImage.LazyLoad().Set(true);
    HeaderIconImage.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 6.0f, 0.0f});

    auto Breadcrumbs = Header.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& BreadcrumbsElement = Breadcrumbs.Element();
    BreadcrumbsElement.ElementStyle().Apply("editor.viewport_breadcrumb");
    BreadcrumbsElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    BreadcrumbsElement.SetCrumbs({"Game View", "Perspective", "Lit"});

    auto Viewport = GameViewTab.Add(UIRenderViewport{});
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

    auto ProfilerTab = ViewTabs.Add(SnAPI::UI::UIPanel("Editor.GameProfilerTab"));
    auto& ProfilerTabPanel = ProfilerTab.Element();
    ProfilerTabPanel.ElementStyle().Apply("editor.section_card");
    ProfilerTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ProfilerTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ProfilerTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ProfilerTabPanel.Padding().Set(8.0f);
    ProfilerTabPanel.Gap().Set(6.0f);

    auto ProfilerHint = ProfilerTab.Add(SnAPI::UI::UIText("Profiler is rendered as a game-viewport overlay."));
    ProfilerHint.Element().ElementStyle().Apply("editor.menu_item");

    ViewTabsElement.SetTabLabel(0, "Game View");
    ViewTabsElement.SetTabLabel(1, "Profiler");

    m_gameViewport = Viewport.Handle();
}

void EditorLayout::BuildInspectorPane(PanelBuilder& Workspace, BaseNode* SelectedNode, CameraComponent* ActiveCamera)
{
    auto Inspector = Workspace.Add(SnAPI::UI::UIPanel("Editor.Inspector"));
    auto& InspectorPanel = Inspector.Element();
    InspectorPanel.ElementStyle().Apply("editor.sidebar");
    InspectorPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    InspectorPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    InspectorPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    InspectorPanel.Padding().Set(6.0f);
    InspectorPanel.Gap().Set(6.0f);

    auto TitleRow = Inspector.Add(SnAPI::UI::UIPanel("Editor.InspectorTitleRow"));
    auto& TitleRowPanel = TitleRow.Element();
    TitleRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    TitleRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TitleRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    TitleRowPanel.Gap().Set(4.0f);
    TitleRowPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    TitleRowPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto TitleIcon = TitleRow.Add(SnAPI::UI::UIImage(kInspectorIconUrl));
    auto& TitleIconImage = TitleIcon.Element();
    TitleIconImage.Width().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    TitleIconImage.Height().Set(SnAPI::UI::Sizing::Fixed(14.0f));
    TitleIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    TitleIconImage.LazyLoad().Set(true);

    auto Title = TitleRow.Add(SnAPI::UI::UIText("Inspector"));
    Title.Element().ElementStyle().Apply("editor.panel_title");
    Title.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto InspectorTabs = Inspector.Add(SnAPI::UI::UITabs{});
    auto& InspectorTabsElement = InspectorTabs.Element();
    InspectorTabsElement.ElementStyle().Apply("editor.viewport_tabs");
    InspectorTabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    InspectorTabsElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    InspectorTabsElement.HeaderHeight().Set(30.0f);

    auto PropertiesTab = InspectorTabs.Add(SnAPI::UI::UIPanel("Editor.Inspector.Properties"));
    auto& PropertiesTabPanel = PropertiesTab.Element();
    PropertiesTabPanel.ElementStyle().Apply("editor.section_card");
    PropertiesTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PropertiesTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertiesTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    PropertiesTabPanel.Padding().Set(4.0f);
    PropertiesTabPanel.Gap().Set(4.0f);

    auto Subtitle = PropertiesTab.Add(SnAPI::UI::UIText("Selection"));
    Subtitle.Element().ElementStyle().Apply("editor.panel_subtitle");
    Subtitle.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto PropertyPanelBuilder = PropertiesTab.Add(UIPropertyPanel{});
    auto& PropertyPanel = PropertyPanelBuilder.Element();
    PropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PropertyPanel.Padding().Set(3.0f);
    PropertyPanel.Gap().Set(3.0f);
    PropertyPanel.ShowHorizontalScrollbar().Set(false);
    PropertyPanel.ShowVerticalScrollbar().Set(true);
    PropertyPanel.Smooth().Set(true);
    PropertyPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto ToolsTab = InspectorTabs.Add(SnAPI::UI::UIPanel("Editor.Inspector.Tools"));
    auto& ToolsTabPanel = ToolsTab.Element();
    ToolsTabPanel.ElementStyle().Apply("editor.section_card");
    ToolsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ToolsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ToolsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ToolsTabPanel.Padding().Set(4.0f);
    ToolsTabPanel.Gap().Set(6.0f);

    auto ToolsScroll = ToolsTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& ToolsScrollElement = ToolsScroll.Element();
    ToolsScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ToolsScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    ToolsScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ToolsScrollElement.ShowHorizontalScrollbar().Set(false);
    ToolsScrollElement.ShowVerticalScrollbar().Set(true);
    ToolsScrollElement.Smooth().Set(true);
    ToolsScrollElement.Padding().Set(2.0f);
    ToolsScrollElement.Gap().Set(6.0f);

    auto SnapCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Snap"));
    auto& SnapCardPanel = SnapCard.Element();
    SnapCardPanel.ElementStyle().Apply("editor.section_card");
    SnapCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    SnapCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    SnapCardPanel.Gap().Set(4.0f);

    auto SnapTitle = SnapCard.Add(SnAPI::UI::UIText("Snapping"));
    SnapTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto MoveSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& MoveSnapField = MoveSnap.Element();
    MoveSnapField.ElementStyle().Apply("editor.number_field");
    MoveSnapField.Step().Set(0.1);
    MoveSnapField.Value().Set(1.0);
    MoveSnapField.Precision().Set(2u);
    MoveSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    MoveSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    MoveSnapField.Padding().Set(5.0f);

    auto RotateSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& RotateSnapField = RotateSnap.Element();
    RotateSnapField.ElementStyle().Apply("editor.number_field");
    RotateSnapField.Step().Set(1.0);
    RotateSnapField.Value().Set(15.0);
    RotateSnapField.Precision().Set(1u);
    RotateSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    RotateSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    RotateSnapField.Padding().Set(5.0f);

    auto ScaleSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& ScaleSnapField = ScaleSnap.Element();
    ScaleSnapField.ElementStyle().Apply("editor.number_field");
    ScaleSnapField.Step().Set(0.05);
    ScaleSnapField.Value().Set(0.5);
    ScaleSnapField.Precision().Set(2u);
    ScaleSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    ScaleSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    ScaleSnapField.Padding().Set(5.0f);

    auto DateCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Date"));
    auto& DateCardPanel = DateCard.Element();
    DateCardPanel.ElementStyle().Apply("editor.section_card");
    DateCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    DateCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DateCardPanel.Gap().Set(4.0f);

    auto DateTitle = DateCard.Add(SnAPI::UI::UIText("Build Date"));
    DateTitle.Element().ElementStyle().Apply("editor.panel_title");
    auto DatePicker = DateCard.Add(SnAPI::UI::UIDatePicker{});
    DatePicker.Element().ElementStyle().Apply("editor.date_picker");
    DatePicker.Element().Width().Set(SnAPI::UI::Sizing::Fill());
    DatePicker.Element().ShowWeekday().Set(false);

    auto ColorCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Color"));
    auto& ColorCardPanel = ColorCard.Element();
    ColorCardPanel.ElementStyle().Apply("editor.section_card");
    ColorCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ColorCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ColorCardPanel.Gap().Set(4.0f);

    auto ColorTitle = ColorCard.Add(SnAPI::UI::UIText("Gizmo Palette"));
    ColorTitle.Element().ElementStyle().Apply("editor.panel_title");
    auto ColorPicker = ColorCard.Add(SnAPI::UI::UIColorPicker{});
    ColorPicker.Element().ElementStyle().Apply("editor.color_picker");
    ColorPicker.Element().Width().Set(SnAPI::UI::Sizing::Fill());
    ColorPicker.Element().Height().Set(SnAPI::UI::Sizing::Fixed(136.0f));

    auto TagsCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Tags"));
    auto& TagsCardPanel = TagsCard.Element();
    TagsCardPanel.ElementStyle().Apply("editor.section_card");
    TagsCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TagsCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TagsCardPanel.Gap().Set(4.0f);

    auto TagsTitle = TagsCard.Add(SnAPI::UI::UIText("Selection Tags"));
    TagsTitle.Element().ElementStyle().Apply("editor.panel_title");
    auto TagsField = TagsCard.Add(SnAPI::UI::UITokenField{});
    TagsField.Element().ElementStyle().Apply("editor.token_field");
    TagsField.Element().Width().Set(SnAPI::UI::Sizing::Fill());
    TagsField.Element().AddToken("Gameplay", false);
    TagsField.Element().AddToken("Dynamic", false);

    auto ShortcutsTable = ToolsScroll.Add(SnAPI::UI::UITable{});
    auto& ShortcutsTableElement = ShortcutsTable.Element();
    ShortcutsTableElement.ElementStyle().Apply("editor.tools_table");
    ShortcutsTableElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ShortcutsTableElement.ColumnCount().Set(2u);
    ShortcutsTableElement.RowHeight().Set(26.0f);
    ShortcutsTableElement.HeaderHeight().Set(26.0f);
    ShortcutsTableElement.SetColumnHeaders({"Action", "Hotkey"});
    constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kShortcuts{{
        {"Focus Selection", "F"},
        {"Duplicate", "Ctrl+D"},
        {"Frame All", "Shift+F"},
        {"Delete", "Del"},
    }};
    for (const auto& [Action, Hotkey] : kShortcuts)
    {
        auto ActionCell = ShortcutsTable.Add(SnAPI::UI::UIText(Action));
        ActionCell.Element().ElementStyle().Apply("editor.menu_item");
        auto KeyCell = ShortcutsTable.Add(SnAPI::UI::UIText(Hotkey));
        KeyCell.Element().ElementStyle().Apply("editor.panel_title");
    }

    InspectorTabsElement.SetTabLabel(0, "Selection");
    InspectorTabsElement.SetTabLabel(1, "Tools");

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
        if (m_boundInspectorObject != SelectedNode || m_boundInspectorType != SelectedNode->TypeKey())
        {
            PropertyPanel->ClearObject();
            if (PropertyPanel->BindNode(SelectedNode))
            {
                m_boundInspectorObject = SelectedNode;
                m_boundInspectorType = SelectedNode->TypeKey();
            }
            else
            {
                m_boundInspectorObject = nullptr;
                m_boundInspectorType = {};
            }
        }
        else
        {
            PropertyPanel->RefreshFromModel();
        }
        return;
    }

    if (ActiveCamera)
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
        PropertyPanel->RefreshFromModel();
        return;
    }

    PropertyPanel->ClearObject();
    if (PropertyPanel->BindObject(TargetType, TargetObject))
    {
        m_boundInspectorObject = TargetObject;
        m_boundInspectorType = TargetType;
    }
    else
    {
        m_boundInspectorObject = nullptr;
        m_boundInspectorType = {};
    }
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

UIRenderViewport* EditorLayout::GameViewport() const
{
    return ResolveGameViewport();
}

int32_t EditorLayout::GameViewportTabIndex() const
{
    if (auto* Tabs = ResolveGameViewTabs())
    {
        return Tabs->ActiveIndex().Get();
    }
    return 0;
}

UIRenderViewport* EditorLayout::ResolveGameViewport() const
{
    if (!m_context || m_gameViewport.Id.Value == 0)
    {
        return nullptr;
    }

    return dynamic_cast<UIRenderViewport*>(&m_context->GetElement(m_gameViewport.Id));
}

SnAPI::UI::UITabs* EditorLayout::ResolveGameViewTabs() const
{
    if (!m_context || m_gameViewTabs.Id.Value == 0)
    {
        return nullptr;
    }
    return dynamic_cast<SnAPI::UI::UITabs*>(&m_context->GetElement(m_gameViewTabs.Id));
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
