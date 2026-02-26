#pragma once

#include "Expected.h"
#include "Handles.h"
#include "TypeRegistration.h"

#include <UIHandles.h>
#include <UIDelegates.h>
#include <UIContextMenu.h>
#include <UIProperties.h>

#include "UIBuilder.h"

#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SnAPI::UI
{
class Theme;
class UIContext;
class UIPanel;
class UIModal;
class UITabs;
class UIText;
class UITextInput;
class UIImage;
class UIBadge;
class UIBreadcrumbs;
class UIListView;
class UISwitch;
class UITreeView;
class ITreeItemSource;
class UIContextMenu;
template<typename TElement>
class TElementBuilder;
} // namespace SnAPI::UI

namespace SnAPI::GameFramework
{
class BaseNode;
class CameraComponent;
class GameRuntime;
class UIPropertyPanel;
class UIRenderViewport;
class World;
} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework::Editor
{
class EditorSelectionModel;

/**
 * @brief Builds and owns the editor shell widget tree inside the root UI context.
 */
class EditorLayout final
{
public:
    struct ContentAssetEntry
    {
        std::string Key{};
        std::string Name{};
        std::string Type{};
        std::string Variant{};
        bool IsRuntime = false;
        bool IsDirty = false;
    };

    struct ContentAssetDetails
    {
        std::string Name{};
        std::string Type{};
        std::string Variant{};
        std::string AssetId{};
        std::string Status{};
        bool IsRuntime = false;
        bool IsDirty = false;
        bool CanPlace = true;
        bool CanSave = true;
    };

    struct ContentAssetCreateRequest
    {
        TypeId Type{};
        std::string Name{};
        std::string FolderPath{};
    };

    struct ContentAssetInspectorState
    {
        struct NodeEntry
        {
            NodeHandle Handle{};
            int Depth = 0;
            std::string Label{};
        };

        bool Open = false;
        std::string AssetKey{};
        std::string Title{};
        std::string Status{};
        TypeId TargetType{};
        void* TargetObject = nullptr;
        std::vector<NodeEntry> Nodes{};
        NodeHandle SelectedNode{};
        bool CanEditHierarchy = false;
        bool IsDirty = false;
        bool CanSave = false;
    };

    enum class EHierarchyAction : std::uint8_t
    {
        AddNodeType,
        AddComponentType,
        RemoveComponentType,
        DeleteNode,
        CreatePrefab,
    };

    struct HierarchyActionRequest
    {
        EHierarchyAction Action = EHierarchyAction::AddNodeType;
        NodeHandle TargetNode{};
        bool TargetIsWorldRoot = false;
        TypeId Type{};
    };

    enum class EToolbarAction : std::uint8_t
    {
        Play,
        Pause,
        Stop,
        JoinLocalPlayer2,
    };

    Result Build(GameRuntime& Runtime,
                 SnAPI::UI::Theme& Theme,
                 CameraComponent* ActiveCamera,
                 EditorSelectionModel* SelectionModel);
    void Shutdown(GameRuntime* Runtime);

    void Sync(GameRuntime& Runtime, CameraComponent* ActiveCamera, EditorSelectionModel* SelectionModel, float DeltaSeconds);
    [[nodiscard]] bool IsBuilt() const { return m_built; }
    [[nodiscard]] UIRenderViewport* GameViewport() const;
    [[nodiscard]] int32_t GameViewportTabIndex() const;
    void SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler);
    void SetHierarchyActionHandler(SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler);
    void SetToolbarActionHandler(SnAPI::UI::TDelegate<void(EToolbarAction)> Handler);
    void SetContentAssets(std::vector<ContentAssetEntry> Assets);
    void SetContentAssetSelectionHandler(SnAPI::UI::TDelegate<void(const std::string&, bool)> Handler);
    void SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetDeleteHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetRenameHandler(SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> Handler);
    void SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()> Handler);
    void SetContentAssetCreateHandler(SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> Handler);
    void SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()> Handler);
    void SetContentAssetInspectorCloseHandler(SnAPI::UI::TDelegate<void()> Handler);
    void SetContentAssetInspectorNodeSelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler);
    void SetContentAssetInspectorHierarchyActionHandler(SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler);
    void SetContentAssetDetails(ContentAssetDetails Details);
    void SetContentAssetInspectorState(ContentAssetInspectorState State);

private:
    using PanelBuilder = SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel>;

    enum class EHierarchyEntryKind : std::uint8_t
    {
        World,
        Level,
        Node,
    };

    struct HierarchyEntry
    {
        NodeHandle Handle{};
        int Depth = 0;
        std::string Label{};
        EHierarchyEntryKind Kind = EHierarchyEntryKind::Node;
    };

    [[nodiscard]] bool RegisterExternalElements(GameRuntime& Runtime);
    [[nodiscard]] SnAPI::UI::UIContext* RootContext(GameRuntime& Runtime) const;

    void BuildShell(SnAPI::UI::UIContext& Context,
                    GameRuntime& Runtime,
                    CameraComponent* ActiveCamera,
                    EditorSelectionModel* SelectionModel);
    void ConfigureRoot(SnAPI::UI::UIContext& Context);

    void BuildMenuBar(PanelBuilder& Root);
    void BuildToolbar(PanelBuilder& Root);
    void BuildWorkspace(PanelBuilder& Root,
                        GameRuntime& Runtime,
                        CameraComponent* ActiveCamera,
                        EditorSelectionModel* SelectionModel);
    void BuildContentBrowser(PanelBuilder& Root);
    void BuildContextMenuOverlay(PanelBuilder& Root);
    void BuildCreateAssetModalOverlay(PanelBuilder& Root);
    void BuildAssetInspectorModalOverlay(PanelBuilder& Root);

    void BuildHierarchyPane(PanelBuilder& Workspace,
                            GameRuntime& Runtime,
                            CameraComponent* ActiveCamera,
                            EditorSelectionModel* SelectionModel);
    void BuildGamePane(PanelBuilder& Workspace, GameRuntime& Runtime, CameraComponent* ActiveCamera);
    void BuildInspectorPane(PanelBuilder& Workspace, BaseNode* SelectedNode, CameraComponent* ActiveCamera);
    void BuildContentDetailsPane(PanelBuilder& DetailsTab);

    void EnsureDefaultSelection(CameraComponent* ActiveCamera);
    void SyncHierarchy(GameRuntime& Runtime, CameraComponent* ActiveCamera);
    void RebuildHierarchyTree(const std::vector<HierarchyEntry>& Entries, const NodeHandle& SelectedNode);
    void SyncHierarchySelection(const NodeHandle& SelectedNode);
    [[nodiscard]] bool CollectHierarchyEntries(World& WorldRef, std::vector<HierarchyEntry>& OutEntries) const;
    [[nodiscard]] std::uint64_t ComputeHierarchySignature(const std::vector<HierarchyEntry>& Entries) const;
    void OnHierarchyNodeChosen(const NodeHandle& Handle);
    [[nodiscard]] BaseNode* ResolveSelectedNode(GameRuntime& Runtime, CameraComponent* ActiveCamera) const;
    [[nodiscard]] bool QueryInvalidationDebugOverlayEnabled() const;
    void SetInvalidationDebugOverlayEnabled(bool Enabled);
    void ToggleInvalidationDebugOverlay();
    void SyncInvalidationDebugOverlay();
    void PublishInvalidationDebugState();
    void HandleContentAssetCardClicked(std::size_t AssetIndex);
    void SelectContentAsset(std::size_t AssetIndex, bool NotifySelection, bool IsDoubleClick);
    void OpenHierarchyContextMenu(std::size_t ItemIndex, const SnAPI::UI::PointerEvent& Event);
    void OpenHierarchyAddTypeMenu(bool AddComponents);
    void OpenContentAssetContextMenu(std::size_t AssetIndex, const SnAPI::UI::PointerEvent& Event);
    void OpenInspectorComponentContextMenu(const NodeHandle& OwnerNode,
                                           const TypeId& ComponentType,
                                           const SnAPI::UI::PointerEvent& Event);
    void OpenContentBrowserContextMenu(const SnAPI::UI::PointerEvent& Event);
    void OpenContextMenu(const SnAPI::UI::UIPoint& ScreenPosition, std::vector<SnAPI::UI::UIContextMenuItem> Items);
    void CloseContextMenu();
    void OnContextMenuItemInvoked(const SnAPI::UI::UIContextMenuItem& Item);
    void EnsureContentAssetCardCapacity();
    void UpdateContentAssetCardWidgets();
    void ApplyContentAssetFilter();
    void OpenContentAssetCreateModal();
    void CloseContentAssetCreateModal();
    void ConfirmContentAssetCreate();
    void RefreshContentAssetCreateModalVisibility();
    void RebuildContentAssetCreateTypeTree();
    void RefreshContentAssetCreateOkButtonState();
    void CloseContentAssetInspectorModal(bool NotifyHandler);
    void RefreshContentAssetInspectorModalVisibility();
    void RefreshContentAssetInspectorModalState();
    void RebuildContentAssetInspectorHierarchyTree();
    void OpenContentAssetInspectorHierarchyContextMenu(std::size_t ItemIndex, const SnAPI::UI::PointerEvent& Event);
    void OpenContentAssetInspectorComponentContextMenu(const NodeHandle& OwnerNode,
                                                       const TypeId& ComponentType,
                                                       const SnAPI::UI::PointerEvent& Event);
    void RebuildContentBrowserEntries();
    void RefreshContentBrowserPath();
    void RefreshContentAssetCardSelectionStyles();
    void RefreshContentAssetDetailsViewModel();
    [[nodiscard]] std::size_t ResolveSelectedContentAssetIndex() const;
    void InitializeViewModel();

    template<typename TValue>
    SnAPI::UI::TPropertyRef<TValue> ViewModelProperty(const SnAPI::UI::PropertyKey Key)
    {
        return SnAPI::UI::TPropertyRef<TValue>(&m_viewModel, Key);
    }

    void BindInspectorTarget(BaseNode* SelectedNode, CameraComponent* ActiveCamera);
    void SyncGameViewportCamera(GameRuntime& Runtime, CameraComponent* ActiveCamera);

    [[nodiscard]] UIRenderViewport* ResolveGameViewport() const;
    [[nodiscard]] SnAPI::UI::UITabs* ResolveGameViewTabs() const;
    [[nodiscard]] UIPropertyPanel* ResolveInspectorPanel() const;

    SnAPI::UI::UIContext* m_context = nullptr;
    GameRuntime* m_runtime = nullptr;
    SnAPI::UI::ElementHandle<SnAPI::UI::UITabs> m_gameViewTabs{};
    SnAPI::UI::ElementHandle<UIRenderViewport> m_gameViewport{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_inspectorPropertyPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_hierarchyTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIContextMenu> m_contextMenu{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIBadge> m_hierarchyCountBadge{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UISwitch> m_invalidationDebugToggleSwitch{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_invalidationDebugToggleLabel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentSearchInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIBreadcrumbs> m_contentPathBreadcrumbs{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentAssetNameValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetTypeValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetVariantValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetIdValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetStatusValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentPlaceButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentSaveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIListView> m_contentAssetsList{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetsEmptyHint{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentCreateModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_contentCreateTypeTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentCreateSearchInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentCreateNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentCreateOkButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentInspectorModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorStatusText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_contentInspectorHierarchyTree{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentInspectorPropertyPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentInspectorSaveButton{};

    struct ContentAssetCardWidgets
    {
        SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> Button{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIImage> Icon{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Type{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Name{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Variant{};
    };
    struct ContentBrowserEntry
    {
        bool IsFolder = false;
        std::size_t AssetIndex = 0;
        std::string FolderPath{};
        std::string DisplayName{};
    };

    std::vector<ContentAssetCardWidgets> m_contentAssetCards{};
    std::vector<SnAPI::UI::ElementHandle<SnAPI::UI::UIButton>> m_contentAssetCardButtons{};
    std::vector<std::size_t> m_contentAssetCardIndices{};
    std::vector<ContentBrowserEntry> m_contentBrowserEntries{};
    std::vector<ContentAssetEntry> m_contentAssets{};
    ContentAssetDetails m_contentAssetDetails{};
    std::string m_contentAssetFilterText{};
    std::string m_contentCurrentFolder{};
    std::string m_selectedContentAssetKey{};
    std::string m_selectedContentFolderPath{};
    std::string m_lastContentAssetClickKey{};
    std::chrono::steady_clock::time_point m_lastContentAssetClickTime{};
    bool m_contentCreateModalOpen = false;
    std::string m_contentCreateTypeFilterText{};
    std::string m_contentCreateNameText{};
    TypeId m_contentCreateSelectedType{};
    std::vector<TypeId> m_contentCreateVisibleTypes{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_contentCreateTypeSource{};
    ContentAssetInspectorState m_contentAssetInspectorState{};
    std::vector<NodeHandle> m_contentInspectorVisibleNodes{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_contentInspectorHierarchySource{};
    bool m_contentInspectorTargetBound = false;
    void* m_contentInspectorBoundObject = nullptr;
    TypeId m_contentInspectorBoundType{};
    SnAPI::UI::TDelegate<void(const std::string&, bool)> m_onContentAssetSelected{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetPlaceRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetSaveRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetDeleteRequested{};
    SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> m_onContentAssetRenameRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetRefreshRequested{};
    SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> m_onContentAssetCreateRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorSaveRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorCloseRequested{};
    SnAPI::UI::TDelegate<void(const NodeHandle&)> m_onContentAssetInspectorNodeSelected{};
    SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> m_onContentAssetInspectorHierarchyActionRequested{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_hierarchyItemSource{};
    enum class EContextMenuScope : std::uint8_t
    {
        None,
        HierarchyItem,
        InspectorComponent,
        ContentAssetItem,
        ContentBrowser,
        ContentInspectorHierarchyItem,
        ContentInspectorComponent,
    };
    enum class EPendingHierarchyMenu : std::uint8_t
    {
        None,
        Root,
        AddNodeTypes,
        AddComponentTypes,
    };
    EContextMenuScope m_contextMenuScope = EContextMenuScope::None;
    EPendingHierarchyMenu m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    std::optional<std::size_t> m_pendingHierarchyMenuIndex{};
    SnAPI::UI::UIPoint m_pendingHierarchyMenuOpenPosition{};
    std::optional<std::size_t> m_contextMenuHierarchyIndex{};
    std::optional<std::size_t> m_contextMenuAssetIndex{};
    NodeHandle m_contextMenuContentInspectorNode{};
    std::optional<NodeHandle> m_contextMenuComponentOwner{};
    TypeId m_contextMenuComponentType{};
    std::vector<TypeId> m_contextMenuNodeTypes{};
    std::vector<TypeId> m_contextMenuComponentTypes{};
    SnAPI::UI::UIPoint m_contextMenuOpenPosition{};
    std::vector<NodeHandle> m_hierarchyVisibleNodes{};
    std::uint64_t m_hierarchySignature = 0;
    std::size_t m_hierarchyNodeCount = 0;
    NodeHandle m_hierarchyVisualSelection{};
    std::string m_hierarchyFilterText{};
    EditorSelectionModel* m_selection = nullptr;
    SnAPI::UI::TDelegate<void(const NodeHandle&)> m_onHierarchyNodeChosen{};
    SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> m_onHierarchyActionRequested{};
    SnAPI::UI::TDelegate<void(EToolbarAction)> m_onToolbarActionRequested{};
    void* m_boundInspectorObject = nullptr;
    TypeId m_boundInspectorType{};
    std::size_t m_boundInspectorComponentSignature = 0;
    bool m_invalidationDebugOverlayEnabled = false;
    SnAPI::UI::PropertyMap m_viewModel{};
    bool m_built = false;
};

} // namespace SnAPI::GameFramework::Editor
