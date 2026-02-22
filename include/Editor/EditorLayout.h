#pragma once

#include "Expected.h"
#include "Handles.h"
#include "TypeRegistration.h"

#include <UIHandles.h>
#include <UIDelegates.h>

#include "UIBuilder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SnAPI::UI
{
class Theme;
class UIContext;
class UIPanel;
class UITabs;
class UIText;
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
    std::vector<NodeHandle> m_hierarchyVisibleNodes{};
    std::uint64_t m_hierarchySignature = 0;
    std::size_t m_hierarchyNodeCount = 0;
    NodeHandle m_hierarchyVisualSelection{};
    std::string m_hierarchyFilterText{};
    EditorSelectionModel* m_selection = nullptr;
    SnAPI::UI::TDelegate<void(NodeHandle)> m_onHierarchyNodeChosen{};
    void* m_boundInspectorObject = nullptr;
    TypeId m_boundInspectorType{};
    bool m_invalidationDebugOverlayEnabled = false;
    bool m_built = false;
};

} // namespace SnAPI::GameFramework::Editor
