#pragma once

#include "Expected.h"
#include "Handles.h"
#include "TypeRegistration.h"

#include <UIHandles.h>
#include <UIDelegates.h>

#include "UIBuilder.h"

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace SnAPI::UI
{
class Theme;
class UIContext;
class UIPanel;
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
    void SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(NodeHandle)> Handler);

private:
    using PanelBuilder = SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel>;
    static constexpr std::uint32_t kHierarchyRadioGroup = 0xED170001u;

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

    PanelBuilder BuildMenuBar(PanelBuilder& Root);
    PanelBuilder BuildToolbar(PanelBuilder& Root);
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
    void RebuildHierarchyRows(const std::vector<HierarchyEntry>& Entries);
    [[nodiscard]] bool CollectHierarchyEntries(World& WorldRef, std::vector<HierarchyEntry>& OutEntries) const;
    [[nodiscard]] std::uint64_t ComputeHierarchySignature(const std::vector<HierarchyEntry>& Entries) const;
    void SyncHierarchySelectionVisual();
    void OnHierarchyNodeChosen(NodeHandle Handle);
    [[nodiscard]] BaseNode* ResolveSelectedNode(GameRuntime& Runtime, CameraComponent* ActiveCamera) const;

    void BindInspectorTarget(BaseNode* SelectedNode, CameraComponent* ActiveCamera);
    void SyncGameViewportCamera(GameRuntime& Runtime, CameraComponent* ActiveCamera);
    void EnsureGameViewportOverlay(GameRuntime& Runtime);
    void UpdateGameViewportOverlay(GameRuntime& Runtime, float DeltaSeconds);

    [[nodiscard]] UIRenderViewport* ResolveGameViewport() const;
    [[nodiscard]] UIPropertyPanel* ResolveInspectorPanel() const;

    SnAPI::UI::UIContext* m_context = nullptr;
    SnAPI::UI::ElementHandle<UIRenderViewport> m_gameViewport{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_inspectorPropertyPanel{};
    SnAPI::UI::ElementId m_hierarchyListHost{};
    SnAPI::UI::ElementId m_hierarchyRowsRoot{};
    std::unordered_map<Uuid, SnAPI::UI::ElementId, UuidHash> m_hierarchyRowsByNode{};
    std::uint64_t m_hierarchySignature = 0;
    std::size_t m_hierarchyNodeCount = 0;
    EditorSelectionModel* m_selection = nullptr;
    SnAPI::UI::TDelegate<void(NodeHandle)> m_onHierarchyNodeChosen{};
    void* m_boundInspectorObject = nullptr;
    TypeId m_boundInspectorType{};
    std::uint64_t m_overlayContextId = 0;
    SnAPI::UI::ElementId m_overlayPanel{};
    SnAPI::UI::ElementId m_overlayGraph{};
    SnAPI::UI::ElementId m_overlayFrameTimeLabel{};
    SnAPI::UI::ElementId m_overlayFpsLabel{};
    std::uint32_t m_overlayFrameTimeSeries = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_overlayFpsSeries = std::numeric_limits<std::uint32_t>::max();
    bool m_built = false;
};

} // namespace SnAPI::GameFramework::Editor
