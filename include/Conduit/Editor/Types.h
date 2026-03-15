#pragma once

#include "Editor/EditorExport.h"
#include "Conduit/Asset.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SnAPI::GameFramework::Conduit::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Active Conduit workspace document category.
 */
enum class EWorkspaceDocumentKind : std::uint8_t
{
    None = 0,
    Graph,
    Class,
};

/**
 * @defgroup SnAPI_GameFramework_Conduit_Editor Conduit.Editor
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Editor-facing authoring, document, schema, and compile-bridge APIs for Conduit.
 *
 * This layer sits on top of the low-level Conduit runtime. It is responsible for:
 * - durable authored graph state and editor metadata
 * - reflection-driven schema expansion for graph UX
 * - document lifetime inside the editor shell
 * - compilation and diagnostics bridging back to authored nodes
 *
 * The editor layer should author high-level graph intent and lower into runtime Conduit
 * primitives such as `GraphBuilder`, `CompiledGraph`, frame slots, and reflected bindings.
 */

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Severity level for one authored-graph compile diagnostic.
 */
enum class ECompileDiagnosticSeverity : std::uint8_t
{
    Info = 0,
    Warning,
    Error,
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One compile diagnostic attached to a graph or authored node.
 */
struct CompileDiagnostic
{
    ECompileDiagnosticSeverity Severity = ECompileDiagnosticSeverity::Error; /**< @brief Severity of the diagnostic. */
    Uuid NodeId{}; /**< @brief Optional authored node id. Empty means graph-level diagnostic. */
    std::string Message{}; /**< @brief Human-readable diagnostic message. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Result of compiling one authored Conduit graph document.
 */
struct CompileOutput
{
    std::optional<CompiledGraph> Graph{}; /**< @brief Compiled runtime graph on success. */
    std::vector<CompileDiagnostic> Diagnostics{}; /**< @brief Diagnostics emitted during compile. */

    /** @brief Query whether any diagnostic is an error. */
    [[nodiscard]] bool HasErrors() const
    {
        for (const CompileDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Severity == ECompileDiagnosticSeverity::Error)
            {
                return true;
            }
        }
        return false;
    }

    /** @brief Query whether compilation produced a runtime graph and no errors. */
    [[nodiscard]] bool Succeeded() const
    {
        return Graph.has_value() && !HasErrors();
    }
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Editor-facing widget strategy for one selected graph-variable default value.
 */
enum class EVariableDefaultEditorKind : std::uint8_t
{
    None = 0,
    Bool,
    Text,
    Enum,
    Complex,
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Lightweight list-row payload for one authored graph variable.
 */
struct VariableEntryView
{
    Uuid Id{}; /**< @brief Stable authored variable id. */
    std::string Name{}; /**< @brief User-facing variable name. */
    std::string TypeLabel{}; /**< @brief Human-readable reflected type label. */
    bool HasDefault = false; /**< @brief `true` when the variable has an authored default override. */
    bool Selected = false; /**< @brief `true` when this row is the active selected variable. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One type option surfaced by the variable-type picker.
 */
struct VariableTypeOption
{
    TypeId Type{}; /**< @brief Reflected type id. */
    std::string Label{}; /**< @brief Human-readable picker label. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One self-type option surfaced by the graph self-type picker.
 */
struct GraphSelfTypeOption
{
    TypeId Type{}; /**< @brief Reflected self type id. Empty means no self type is authored. */
    std::string Label{}; /**< @brief Human-readable picker label. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One host-node type option surfaced by the Conduit class editor.
 */
struct ClassHostTypeOption
{
    TypeId Type{}; /**< @brief Reflected host-node type id. */
    std::string Label{}; /**< @brief Human-readable picker label. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One Conduit graph asset option surfaced by the Conduit class editor.
 */
struct ClassGraphOption
{
    std::string AssetKey{}; /**< @brief Logical source asset key of the graph asset. */
    std::string Label{}; /**< @brief Human-readable picker label. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Lightweight palette-row payload for one schema node template.
 */
struct PaletteEntryView
{
    std::string StableId{}; /**< @brief Stable schema/template id used to spawn the node. */
    std::string DisplayName{}; /**< @brief UI-facing node name. */
    std::string Category{}; /**< @brief Palette/search category path. */
    std::string Tooltip{}; /**< @brief Short explanatory tooltip. */
    bool RequiresSpecialization = false; /**< @brief `true` when more authored configuration is needed after spawn. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Request payload emitted when the graph canvas wants a spawn context menu.
 */
struct GraphSpawnMenuRequest
{
    float ScreenX = 0.0f; /**< @brief Screen-space X position for the menu anchor. */
    float ScreenY = 0.0f; /**< @brief Screen-space Y position for the menu anchor. */
    float GraphX = 0.0f; /**< @brief Graph-space X position where the spawned node should be placed. */
    float GraphY = 0.0f; /**< @brief Graph-space Y position where the spawned node should be placed. */
    Uuid SourceNodeId{}; /**< @brief Optional source node id when the menu originated from a dragged output pin. */
    std::string SourcePin{}; /**< @brief Optional source output pin name when the menu originated from a dragged wire. */
    bool FromPinDrag = false; /**< @brief `true` when the request came from releasing a dragged output pin on empty canvas. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One context-menu spawn option for the graph canvas.
 */
struct SpawnMenuEntryView
{
    std::string StableId{}; /**< @brief Stable schema/template id used to spawn the node. */
    std::string DisplayName{}; /**< @brief UI-facing node name. */
    std::string Category{}; /**< @brief Category path for grouping and disambiguation. */
    std::string Tooltip{}; /**< @brief Short explanatory tooltip. */
    std::string TargetPin{}; /**< @brief Input pin name to auto-connect when spawned from a dragged source pin. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Lightweight outline-row payload for one authored node already present in the document.
 */
struct NodeEntryView
{
    Uuid Id{}; /**< @brief Stable authored node id. */
    std::string Title{}; /**< @brief UI-facing node title. */
    std::string Detail{}; /**< @brief Secondary summary such as category or bound member. */
    bool Selected = false; /**< @brief `true` when this row is the active selected node. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Lightweight visual payload for one rendered node pin on the graph canvas.
 */
struct CanvasPinView
{
    std::string Name{}; /**< @brief UI-facing pin label. */
    std::string TypeLabel{}; /**< @brief Secondary type summary shown for value and handle pins. */
    ESlotKind Kind = ESlotKind::Value; /**< @brief Value vs handle semantics for non-exec pins. */
    bool IsInput = true; /**< @brief `true` when this pin is rendered on the left side of the node. */
    bool IsExec = false; /**< @brief `true` when this pin carries control flow rather than data. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Lightweight visual payload for one rendered graph wire.
 *
 * This is currently used only for authored control-flow routing that the graph asset can
 * already represent explicitly, such as jump and branch label targets.
 */
struct CanvasWireView
{
    Uuid SourceNodeId{}; /**< @brief Authored source node id. */
    std::string SourcePin{}; /**< @brief Source pin label. */
    Uuid TargetNodeId{}; /**< @brief Authored destination node id. */
    std::string TargetPin{}; /**< @brief Destination pin label. */
    ESlotKind Kind = ESlotKind::Value; /**< @brief Value vs handle semantics for non-exec wires. */
    bool IsExec = false; /**< @brief `true` when this wire represents control flow. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Visual layout payload for one authored node on the graph canvas.
 */
struct CanvasNodeView
{
    Uuid Id{}; /**< @brief Stable authored node id. */
    std::string Title{}; /**< @brief UI-facing node title. */
    std::string Detail{}; /**< @brief Secondary node subtitle. */
    float X = 0.0f; /**< @brief Authored graph-space left position. */
    float Y = 0.0f; /**< @brief Authored graph-space top position. */
    float Width = 240.0f; /**< @brief Preferred graph-space node width. */
    bool IsCollapsed = false; /**< @brief `true` when the node is visually collapsed. */
    bool Selected = false; /**< @brief `true` when the node is currently selected. */
    std::vector<CanvasPinView> InputPins{}; /**< @brief Input pins shown on the left side. */
    std::vector<CanvasPinView> OutputPins{}; /**< @brief Output pins shown on the right side. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Visual layout payload for one authored comment/group box on the graph canvas.
 */
struct CanvasCommentView
{
    Uuid Id{}; /**< @brief Stable authored comment id. */
    std::string Title{}; /**< @brief User-facing comment title. */
    float X = 0.0f; /**< @brief Authored graph-space left position. */
    float Y = 0.0f; /**< @brief Authored graph-space top position. */
    float Width = 480.0f; /**< @brief Graph-space width. */
    float Height = 320.0f; /**< @brief Graph-space height. */
    std::uint32_t ColorRgba = 0x334455FFu; /**< @brief Encoded editor tint. */
    bool Selected = false; /**< @brief `true` when the comment box is selected. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Full graph-canvas view model for the active authored document.
 */
struct GraphCanvasView
{
    GraphViewportAsset Viewport{}; /**< @brief Current authored graph-space viewport pan and zoom. */
    std::vector<CanvasNodeView> Nodes{}; /**< @brief Visible authored nodes. */
    std::vector<CanvasCommentView> Comments{}; /**< @brief Visible authored comment/group boxes. */
    std::vector<CanvasWireView> Wires{}; /**< @brief Visible authored graph wires. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Full detail-pane payload for the currently selected graph variable.
 *
 * `ComplexObject` is a borrowed pointer to service-owned scratch storage used by the property panel.
 * It remains valid until the next variable-selection or variable-default update pushed by the service.
 */
struct VariableInspectorView
{
    bool HasSelection = false; /**< @brief `true` when one graph variable is selected. */
    Uuid VariableId{}; /**< @brief Stable authored variable id. */
    std::string Name{}; /**< @brief User-facing variable name. */
    TypeId Type{}; /**< @brief Reflected variable type id. */
    std::string TypeLabel{}; /**< @brief Human-readable reflected type label. */
    bool HasDefault = false; /**< @brief `true` when the variable currently has an authored default override. */
    EVariableDefaultEditorKind DefaultEditorKind = EVariableDefaultEditorKind::None; /**< @brief UI strategy for default editing. */
    bool BoolValue = false; /**< @brief Current bool default value when `DefaultEditorKind == Bool`. */
    std::string TextValue{}; /**< @brief Current string/textual default value when `DefaultEditorKind == Text`. */
    std::vector<std::string> EnumOptions{}; /**< @brief Available enum entry labels when `DefaultEditorKind == Enum`. */
    int32_t SelectedEnumIndex = -1; /**< @brief Selected enum option index, or `-1`. */
    void* ComplexObject = nullptr; /**< @brief Borrowed complex-default scratch object for property-panel editing. */
    TypeId ComplexType{}; /**< @brief Reflected type id of `ComplexObject`. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Full detail-pane payload for the currently selected authored node.
 */
struct NodeInspectorView
{
    bool HasSelection = false; /**< @brief `true` when one authored node is selected. */
    Uuid NodeId{}; /**< @brief Stable authored node id. */
    EGraphAssetNodeKind Kind = EGraphAssetNodeKind::Label; /**< @brief Authored opcode tag for the selected node. */
    std::string Title{}; /**< @brief UI-facing node title. */
    std::string Detail{}; /**< @brief Secondary summary line. */
    bool CanEditPrimaryText = false; /**< @brief `true` when the primary text field is editable. */
    std::string PrimaryTextLabel{}; /**< @brief Caption shown beside the primary text field. */
    std::string PrimaryTextValue{}; /**< @brief Current primary text value. */
    bool CanEditSecondaryText = false; /**< @brief `true` when the secondary text field is editable. */
    std::string SecondaryTextLabel{}; /**< @brief Caption shown beside the secondary text field. */
    std::string SecondaryTextValue{}; /**< @brief Current secondary text value. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Full detail-pane payload for the active Conduit class document.
 */
struct ClassInspectorView
{
    bool HasSelection = false; /**< @brief `true` when a Conduit class document is active. */
    std::string Name{}; /**< @brief User-facing class name. */
    TypeId HostType{}; /**< @brief Reflected host node type. */
    std::string HostTypeLabel{}; /**< @brief Human-readable host type label. */
    std::string GraphAssetKey{}; /**< @brief Logical graph asset reference key. */
    std::string GraphAssetLabel{}; /**< @brief Human-readable graph asset label. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Current multi-selection state for one authored graph document.
 */
struct GraphSelection
{
    std::vector<Uuid> NodeIds{}; /**< @brief Selected authored node ids. */
    std::vector<Uuid> CommentIds{}; /**< @brief Selected authored comment-box ids. */
    std::vector<Uuid> VariableIds{}; /**< @brief Selected authored graph-variable ids. */

    /** @brief Clear all current selection ids. */
    void Clear()
    {
        NodeIds.clear();
        CommentIds.clear();
        VariableIds.clear();
    }

    /** @brief Query whether the document currently has no selected authored object. */
    [[nodiscard]] bool Empty() const
    {
        return NodeIds.empty() && CommentIds.empty() && VariableIds.empty();
    }
};

} // namespace SnAPI::GameFramework::Conduit::Editor
