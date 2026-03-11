#pragma once

#include "Editor/EditorExport.h"
#include "Conduit/Editor/Types.h"

#include <UIElementBase.h>
#include <UIDelegates.h>

#include <optional>

namespace SnAPI::UI
{
class UIContext;
struct UIPaintContext;
class RoutedEventContext;
} // namespace SnAPI::UI

namespace SnAPI::GameFramework::Conduit::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Custom UI element that renders the authored Conduit graph canvas.
 *
 * This is the first real graph surface for Conduit authoring. It owns no authoritative
 * document state; instead it renders a pushed `GraphCanvasView` snapshot and emits selection,
 * drag, pan, and zoom callbacks back to the editor service.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API UIConduitGraphCanvas final : public SnAPI::UI::UIElementBase
{
public:
    using NodeSelectionHandler = SnAPI::UI::TDelegate<void(const Uuid&)>;
    using NodeMovedHandler = SnAPI::UI::TDelegate<void(const Uuid&, float, float)>;
    using ViewportChangedHandler = SnAPI::UI::TDelegate<void(float, float, float)>;

    UIConduitGraphCanvas();
    ~UIConduitGraphCanvas() override = default;

    void Initialize(SnAPI::UI::UIContext* Context, SnAPI::UI::ElementId Id);

    void SetViewState(GraphCanvasView View);
    [[nodiscard]] const GraphCanvasView& ViewState() const { return m_view; }

    void SetNodeSelectionHandler(NodeSelectionHandler Handler);
    void SetNodeMovedHandler(NodeMovedHandler Handler);
    void SetViewportChangedHandler(ViewportChangedHandler Handler);

    void Measure(const SnAPI::UI::UIConstraints& Constraints, SnAPI::UI::UISize& OutDesired) override;
    void Paint(SnAPI::UI::UIPaintContext& Context) const override;
    void OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context) override;

private:
    struct NodeVisual
    {
        SnAPI::UI::UIRect Rect{};
        float GraphX = 0.0f;
        float GraphY = 0.0f;
    };

    struct PinVisual
    {
        SnAPI::UI::UIPoint Center{};
    };

    struct CommentVisual
    {
        SnAPI::UI::UIRect Rect{};
    };

    void Invalidate(SnAPI::UI::EInvalidation Flags) const;
    [[nodiscard]] float EffectiveZoom() const;
    [[nodiscard]] float DpiScale() const;
    [[nodiscard]] float NodeHeightGraphUnits(const CanvasNodeView& Node) const;
    [[nodiscard]] NodeVisual ComputeNodeVisual(const CanvasNodeView& Node) const;
    [[nodiscard]] PinVisual ComputePinVisual(const CanvasNodeView& Node, const NodeVisual& Visual, bool IsInput, std::size_t PinIndex) const;
    [[nodiscard]] CommentVisual ComputeCommentVisual(const CanvasCommentView& Comment) const;
    [[nodiscard]] SnAPI::UI::UIPoint ScreenToGraph(const SnAPI::UI::UIPoint& ScreenPosition) const;
    [[nodiscard]] std::optional<std::size_t> HitTestNode(const SnAPI::UI::UIPoint& ScreenPosition) const;
    [[nodiscard]] static SnAPI::UI::Color DecodeColor(std::uint32_t Rgba, std::uint8_t DefaultAlpha = 255);
    void SetSelectedNodeLocal(const Uuid& NodeId);
    void ClearInteractionState();

    GraphCanvasView m_view{};
    NodeSelectionHandler m_onNodeSelected{};
    NodeMovedHandler m_onNodeMoved{};
    ViewportChangedHandler m_onViewportChanged{};

    bool m_isPanning = false;
    bool m_isDraggingNode = false;
    Uuid m_dragNodeId{};
    SnAPI::UI::UIPoint m_dragStartPointer{};
    SnAPI::UI::UIPoint m_dragNodeOffsetGraph{};
    float m_dragStartPanX = 0.0f;
    float m_dragStartPanY = 0.0f;
};

} // namespace SnAPI::GameFramework::Conduit::Editor
