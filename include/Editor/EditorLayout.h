#pragma once

#include "Expected.h"
#include "Handles.h"
#include "TypeRegistration.h"

#include <UIHandles.h>
#include <UIDelegates.h>

#include "UIBuilder.h"

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

namespace SnAPI::UI
{
class Theme;
class UIContext;
class UIPanel;
class UITabs;
class UIText;
class UIListView;
class UITreeView;
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
    };

    struct ContentAssetDetails
    {
        std::string Name{};
        std::string Type{};
        std::string Variant{};
        std::string AssetId{};
        std::string Status{};
        bool CanPlace = true;
        bool CanSave = true;
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
    void SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(NodeHandle)> Handler);
    void SetContentAssets(std::vector<ContentAssetEntry> Assets);
    void SetContentAssetSelectionHandler(SnAPI::UI::TDelegate<void(const std::string&, bool)> Handler);
    void SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    void SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()> Handler);
    void SetContentAssetDetails(ContentAssetDetails Details);

private:
    using PanelBuilder = SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel>;

    struct HierarchyEntry
    {
        NodeHandle Handle{};
        int Depth = 0;
        std::string Label{};
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

    void BuildHierarchyPane(PanelBuilder& Workspace,
                            GameRuntime& Runtime,
                            CameraComponent* ActiveCamera,
                            EditorSelectionModel* SelectionModel);
    void BuildGamePane(PanelBuilder& Workspace, GameRuntime& Runtime, CameraComponent* ActiveCamera);
    void BuildInspectorPane(PanelBuilder& Workspace, BaseNode* SelectedNode, CameraComponent* ActiveCamera);
    void BuildContentDetailsPane(PanelBuilder& DetailsTab);

    void EnsureDefaultSelection(CameraComponent* ActiveCamera);
    void SyncHierarchy(GameRuntime& Runtime, CameraComponent* ActiveCamera);
    void RebuildHierarchyTree(const std::vector<HierarchyEntry>& Entries, NodeHandle SelectedNode);
    [[nodiscard]] bool CollectHierarchyEntries(World& WorldRef, std::vector<HierarchyEntry>& OutEntries) const;
    [[nodiscard]] std::uint64_t ComputeHierarchySignature(const std::vector<HierarchyEntry>& Entries) const;
    void OnHierarchyNodeChosen(NodeHandle Handle);
    [[nodiscard]] BaseNode* ResolveSelectedNode(GameRuntime& Runtime, CameraComponent* ActiveCamera) const;
    [[nodiscard]] bool QueryInvalidationDebugOverlayEnabled() const;
    void SetInvalidationDebugOverlayEnabled(bool Enabled);
    void ToggleInvalidationDebugOverlay();
    void SyncInvalidationDebugOverlay();
    void UpdateInvalidationDebugToggleLabel();
    void HandleContentAssetCardClicked(std::size_t AssetIndex);
    void EnsureContentAssetCardCapacity();
    void UpdateContentAssetCardWidgets();
    void ApplyContentAssetFilter();
    void RefreshContentAssetCardSelectionStyles();
    void UpdateContentAssetDetailsWidgets();
    [[nodiscard]] std::size_t ResolveSelectedContentAssetIndex() const;

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
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_invalidationDebugToggleLabel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentSearchInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetNameValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetTypeValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetVariantValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetIdValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetStatusValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentPlaceButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentSaveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIListView> m_contentAssetsList{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetsEmptyHint{};

    struct ContentAssetCardWidgets
    {
        SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> Button{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Type{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Name{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Variant{};
    };

    std::vector<ContentAssetCardWidgets> m_contentAssetCards{};
    std::vector<SnAPI::UI::ElementHandle<SnAPI::UI::UIButton>> m_contentAssetCardButtons{};
    std::vector<std::size_t> m_contentAssetCardIndices{};
    std::vector<ContentAssetEntry> m_contentAssets{};
    ContentAssetDetails m_contentAssetDetails{};
    std::string m_contentAssetFilterText{};
    std::string m_selectedContentAssetKey{};
    std::string m_lastContentAssetClickKey{};
    std::chrono::steady_clock::time_point m_lastContentAssetClickTime{};
    SnAPI::UI::TDelegate<void(const std::string&, bool)> m_onContentAssetSelected{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetPlaceRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetSaveRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetRefreshRequested{};
    std::vector<NodeHandle> m_hierarchyVisibleNodes{};
    std::uint64_t m_hierarchySignature = 0;
    std::size_t m_hierarchyNodeCount = 0;
    NodeHandle m_hierarchyVisualSelection{};
    std::string m_hierarchyFilterText{};
    EditorSelectionModel* m_selection = nullptr;
    SnAPI::UI::TDelegate<void(NodeHandle)> m_onHierarchyNodeChosen{};
    void* m_boundInspectorObject = nullptr;
    TypeId m_boundInspectorType{};
    std::size_t m_boundInspectorComponentSignature = 0;
    bool m_invalidationDebugOverlayEnabled = false;
    bool m_built = false;
};

} // namespace SnAPI::GameFramework::Editor
