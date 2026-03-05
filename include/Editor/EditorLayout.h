#pragma once

#include "Expected.h"
#include "Editor/EditorImportSettings.h"
#include "Handles.h"
#include "IAssetImportSettings.h"
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
#include <unordered_map>
#include <utility>
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
class UIFilesystemPicker;
class UIImage;
class UIBadge;
class UIBreadcrumbs;
class UIListView;
class UISwitch;
class UITreeView;
class ITreeItemSource;
class UIContextMenu;
class UIButton;
class UIComboBox;
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
        std::string IconSource{};
        std::uint32_t IconTextureId = 0;
        std::uint32_t IconWidth = 0;
        std::uint32_t IconHeight = 0;
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

    struct ContentAssetImportRequest
    {
        std::string SourcePath{};
        std::string FolderPath{};
        std::unordered_map<std::string, std::string> BuildOptions{};
        ::SnAPI::AssetPipeline::AssetImportSettingsPtr ImportSettings{};
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
        TypeId ImportSettingsType{};
        void* ImportSettingsObject = nullptr;
        std::vector<NodeEntry> Nodes{};
        NodeHandle SelectedNode{};
        bool CanEditHierarchy = false;
        bool HasImportSettings = false;
        bool RuntimeDirty = false;
        bool ImportSettingsDirty = false;
        bool IsDirty = false;
        bool CanSave = false;
        bool CanReimport = false;
        std::string PreviewIconSource{};
        std::uint32_t PreviewTextureId = 0;
        std::uint32_t PreviewWidth = 0;
        std::uint32_t PreviewHeight = 0;
        std::string PreviewStatsPrimary{};
        std::string PreviewStatsSecondary{};
        std::uint64_t SessionRevision = 0;
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

    enum class EGizmoSpace : std::uint8_t
    {
        World = 0,
        Object,
        Camera
    };

    enum class ESnapMode : std::uint8_t
    {
        Off = 0,
        On
    };

    enum class EProjectAction : std::uint8_t
    {
        CreateNew,
        OpenExisting,
        SaveSettings,
    };

    struct ProjectActionRequest
    {
        EProjectAction Action = EProjectAction::CreateNew;
        std::string ProjectName{};
        std::string ProjectDirectory{};
        std::string ProjectFilePath{};
        std::string StartupLevelPack{};
        std::string DefaultRenderSettingsAssetId{};
    };

    struct ProjectState
    {
        bool IsLoaded = false;
        std::string Name{};
        std::string ProjectFilePath{};
        std::string ProjectRootDirectory{};
        std::string AssetRootDirectory{};
        std::string StartupLevelPack{};
        std::string DefaultRenderSettingsAssetId{};
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
    [[nodiscard]] SnAPI::UI::UIContext* Context() const { return m_context; }
    [[nodiscard]] EGizmoSpace GizmoSpace() const { return m_gizmoSpace; }
    [[nodiscard]] bool GizmoSnappingEnabled() const { return m_snapMode == ESnapMode::On; }
    [[nodiscard]] double MoveSnapStep() const { return m_moveSnapStep; }
    [[nodiscard]] double RotateSnapStepDegrees() const { return m_rotateSnapStepDegrees; }
    [[nodiscard]] double ScaleSnapStep() const { return m_scaleSnapStep; }
    void SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler);
    void SetHierarchyActionHandler(SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler);
    void SetToolbarActionHandler(SnAPI::UI::TDelegate<void(EToolbarAction)> Handler);
    void SetProjectActionHandler(SnAPI::UI::TDelegate<void(const ProjectActionRequest&)> Handler);
    void SetProjectState(ProjectState State);
    void SetProjectSelectionRequired(bool Required);
    void SetContentAssets(std::vector<ContentAssetEntry> Assets);
    void SetContentAssetSelectionHandler(SnAPI::UI::TDelegate<void(const std::string&, bool)> Handler);
    void SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetDeleteHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetRenameHandler(SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> Handler);
    void SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()> Handler);
    void SetContentAssetCreateHandler(SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> Handler);
    void SetContentAssetImportHandler(SnAPI::UI::TDelegate<void(const ContentAssetImportRequest&)> Handler);
    void SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()> Handler);
    void SetContentAssetInspectorReimportHandler(SnAPI::UI::TDelegate<void()> Handler);
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

    enum class EImportProfile : std::uint8_t
    {
        Unknown = 0,
        AssimpModel,
        Texture,
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
    void EnsureContextMenuOverlay();
    void EnsureContentAssetCreateModalOverlay();
    void DestroyContentAssetCreateModalOverlay();
    void EnsureContentAssetImportModalOverlay();
    void DestroyContentAssetImportModalOverlay();
    void EnsureContentAssetInspectorModalOverlay();
    void DestroyContentAssetInspectorModalOverlay();
    void EnsureProjectModalOverlay();
    void DestroyProjectModalOverlay();
    void EnsureProjectSettingsModalOverlay();
    void DestroyProjectSettingsModalOverlay();

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
    void OpenFileMenu();
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
    void OpenContentAssetImportModal();
    void CloseContentAssetImportModal();
    void ConfirmContentAssetImport();
    void RefreshContentAssetImportModalVisibility();
    void RefreshContentAssetImportProfile();
    void RefreshContentAssetImportSettingsPanel();
    void RefreshContentAssetImportSummary();
    void RefreshContentAssetImportOkButtonState();
    void OpenProjectWelcomeModal();
    void OpenProjectCreateModal();
    void OpenProjectOpenModal();
    void OpenProjectSettingsModal();
    void CloseProjectSettingsModal();
    void ConfirmProjectSettingsModal();
    void CloseProjectModal(bool ForceClose = false);
    void ConfirmProjectModal();
    void RefreshProjectModalVisibility();
    void RefreshProjectModalOkButtonState();
    void RefreshProjectSettingsModalVisibility();
    void RefreshProjectSettingsModalSaveButtonState();
    void RememberRecentProject(const ProjectActionRequest& Request);
    void RememberRecentProjectFile(std::string ProjectFilePath, std::string ProjectName = {});
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
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_shellRoot{};
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
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentImportModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_contentImportSourcePicker{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentImportSummaryText{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentImportSettingsPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentImportOkButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentInspectorModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorStatusText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorHierarchyTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorPreviewStatsText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIImage> m_contentInspectorPreviewImage{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_contentInspectorHierarchyTree{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentInspectorPropertyPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorImportSettingsTitleText{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentInspectorImportSettingsPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentInspectorSaveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentInspectorReimportButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_menuFileButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_projectModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_projectNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_projectDirectoryInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_projectFilePathInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_projectModalOkButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_projectSettingsModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_projectSettingsNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_projectSettingsStartupPackInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_projectSettingsDefaultRenderSettingsCombo{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_projectSettingsSaveButton{};

    struct ContentAssetCardWidgets
    {
        SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> Button{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> Card{};
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
    struct RecentProjectEntry
    {
        std::string Name{};
        std::string ProjectFilePath{};
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
    bool m_contentImportModalOpen = false;
    std::string m_contentImportSourcePath{};
    EImportProfile m_contentImportProfile = EImportProfile::Unknown;
    AssimpImportSettings m_contentImportAssimpSettings{};
    TextureImportSettings m_contentImportTextureSettings{};
    ContentAssetInspectorState m_contentAssetInspectorState{};
    bool m_projectModalOpen = false;
    bool m_projectModalRequired = false;
    bool m_projectModalShowWelcome = false;
    bool m_projectSettingsModalOpen = false;
    EProjectAction m_projectModalAction = EProjectAction::CreateNew;
    std::string m_projectNameText{};
    std::string m_projectDirectoryText{};
    std::string m_projectFilePathText{};
    std::string m_projectSettingsNameText{};
    std::string m_projectSettingsStartupPackText{};
    std::string m_projectSettingsDefaultRenderSettingsAssetId{};
    std::vector<std::pair<std::string, std::string>> m_projectSettingsRenderSettingsOptions{};
    ProjectState m_projectState{};
    std::vector<RecentProjectEntry> m_recentProjects{};
    std::vector<NodeHandle> m_contentInspectorVisibleNodes{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_contentInspectorHierarchySource{};
    bool m_contentInspectorTargetBound = false;
    void* m_contentInspectorBoundObject = nullptr;
    TypeId m_contentInspectorBoundType{};
    bool m_contentInspectorImportTargetBound = false;
    void* m_contentInspectorImportBoundObject = nullptr;
    TypeId m_contentInspectorImportBoundType{};
    SnAPI::UI::TDelegate<void(const std::string&, bool)> m_onContentAssetSelected{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetPlaceRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetSaveRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetDeleteRequested{};
    SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> m_onContentAssetRenameRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetRefreshRequested{};
    SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> m_onContentAssetCreateRequested{};
    SnAPI::UI::TDelegate<void(const ContentAssetImportRequest&)> m_onContentAssetImportRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorSaveRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorReimportRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorCloseRequested{};
    SnAPI::UI::TDelegate<void(const NodeHandle&)> m_onContentAssetInspectorNodeSelected{};
    SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> m_onContentAssetInspectorHierarchyActionRequested{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_hierarchyItemSource{};
    enum class EContextMenuScope : std::uint8_t
    {
        None,
        MenuBar,
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
    SnAPI::UI::TDelegate<void(const ProjectActionRequest&)> m_onProjectActionRequested{};
    void* m_boundInspectorObject = nullptr;
    TypeId m_boundInspectorType{};
    std::size_t m_boundInspectorComponentSignature = 0;
    EGizmoSpace m_gizmoSpace = EGizmoSpace::World;
    ESnapMode m_snapMode = ESnapMode::Off;
    double m_moveSnapStep = 1.0;
    double m_rotateSnapStepDegrees = 15.0;
    double m_scaleSnapStep = 0.5;
    bool m_invalidationDebugOverlayEnabled = false;
    SnAPI::UI::PropertyMap m_viewModel{};
    bool m_built = false;
};

} // namespace SnAPI::GameFramework::Editor
