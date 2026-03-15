#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "AssetRef.h"
#include "Conduit/Compiler.h"
#include "IAsset.h"
#include "TypeName.h"

namespace SnAPI::AssetPipeline
{
class AssetManager;
}

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Authored-node opcode classification for persistent Conduit graph assets.
 *
 * This is intentionally close to the current low-level runtime surface so the first
 * authored asset layer can compile directly into `GraphBuilder` without a second semantic
 * model in between.
 */
enum class EGraphAssetNodeKind : std::uint8_t
{
    EntryPoint,
    Label,
    Constant,
    VariableGet,
    VariableSet,
    UnaryIntrinsic,
    BinaryIntrinsic,
    Jump,
    Branch,
    SelfFieldRead,
    SelfFieldWrite,
    SelfMethodCall,
    InstanceFieldRead,
    InstanceFieldWrite,
    InstanceMethodCall,
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Persisted viewport state for the Conduit graph canvas.
 */
struct GraphViewportAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphViewportAsset";
    float PanX = 0.0f; /**< @brief Horizontal canvas pan offset in authored graph space. */
    float PanY = 0.0f; /**< @brief Vertical canvas pan offset in authored graph space. */
    float Zoom = 1.0f; /**< @brief Graph-canvas zoom factor. */

    bool operator==(const GraphViewportAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Persisted visual state for one authored graph node.
 */
struct GraphNodeEditorAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphNodeEditorAsset";
    Uuid NodeId{}; /**< @brief Stable authored node id this layout entry belongs to. */
    float X = 0.0f; /**< @brief Left position in graph-canvas space. */
    float Y = 0.0f; /**< @brief Top position in graph-canvas space. */
    float Width = 240.0f; /**< @brief Preferred node width for authoring UI. */
    bool IsCollapsed = false; /**< @brief True when the node is collapsed in the editor. */

    bool operator==(const GraphNodeEditorAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Persisted authored comment box / group region.
 */
struct GraphCommentAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphCommentAsset";
    Uuid Id{}; /**< @brief Stable authored comment-box id. */
    std::string Title{}; /**< @brief User-facing comment-box title. */
    float X = 0.0f; /**< @brief Left position in graph-canvas space. */
    float Y = 0.0f; /**< @brief Top position in graph-canvas space. */
    float Width = 480.0f; /**< @brief Width in graph-canvas space. */
    float Height = 320.0f; /**< @brief Height in graph-canvas space. */
    std::uint32_t ColorRgba = 0x334455FFu; /**< @brief Editor tint encoded as 0xRRGGBBAA. */
    std::vector<Uuid> NodeIds{}; /**< @brief Authored nodes visually grouped by this comment box. */

    bool operator==(const GraphCommentAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Persisted named camera/bookmark location for graph authoring.
 */
struct GraphBookmarkAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphBookmarkAsset";
    Uuid Id{}; /**< @brief Stable authored bookmark id. */
    std::string Name{}; /**< @brief User-facing bookmark label. */
    float PanX = 0.0f; /**< @brief Viewport horizontal pan. */
    float PanY = 0.0f; /**< @brief Viewport vertical pan. */
    float Zoom = 1.0f; /**< @brief Viewport zoom. */

    bool operator==(const GraphBookmarkAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Persisted editor-only metadata that travels with a `GraphAsset`.
 */
struct GraphEditorAssetState
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphEditorAssetState";
    GraphViewportAsset Viewport{}; /**< @brief Last authored viewport pan and zoom. */
    std::vector<GraphNodeEditorAsset> Nodes{}; /**< @brief Per-node layout records keyed by authored node id. */
    std::vector<GraphCommentAsset> Comments{}; /**< @brief Authored comment/group regions. */
    std::vector<GraphBookmarkAsset> Bookmarks{}; /**< @brief Authored viewport bookmarks. */

    bool operator==(const GraphEditorAssetState&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief One authored slot declaration inside a `GraphAsset`.
 */
struct GraphSlotAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphSlotAsset";
    std::string Name{}; /**< @brief Optional authored/debug slot name. */
    TypeId Type{}; /**< @brief Reflected value type stored in the slot. */
    ESlotKind Kind = ESlotKind::Value; /**< @brief Value vs handle interpretation. */

    bool operator==(const GraphSlotAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief One authored persistent graph variable.
 */
struct GraphVariableAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphVariableAsset";
    Uuid Id{}; /**< @brief Stable authored variable id used by get/set nodes and rename-safe tooling. */
    std::string Name{}; /**< @brief User-facing variable name. */
    TypeId Type{}; /**< @brief Reflected stored type. */
    SerializedValue DefaultValue{}; /**< @brief Optional initial value. Empty type means default-construct at runtime. */

    bool operator==(const GraphVariableAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief One authored node record inside a `GraphAsset`.
 *
 * This is a discriminated data record keyed by `Kind`. Only the fields relevant to the
 * chosen kind are interpreted by `CompileGraphAsset(...)`.
 */
struct GraphNodeAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphNodeAsset";
    Uuid Id{}; /**< @brief Stable authored node id used by editor metadata, diagnostics, and tooling. */
    EGraphAssetNodeKind Kind = EGraphAssetNodeKind::Label; /**< @brief Opcode tag for the authored node. */

    EBuiltinEntryPoint BuiltinEntryPoint = EBuiltinEntryPoint::None; /**< @brief Built-in lifecycle id for `EntryPoint` nodes. `None` means custom named entrypoint. */
    std::string EntryPointName{}; /**< @brief Custom entrypoint name for `EntryPoint` nodes. */
    Uuid VariableId{}; /**< @brief Stable authored variable id used by `VariableGet` / `VariableSet` nodes. */

    std::string LabelName{}; /**< @brief Label name for `Label` nodes and jump/branch targets. */
    std::string FalseLabelName{}; /**< @brief False-target label used by `Branch`. */
    std::string MemberName{}; /**< @brief Reflected field or method name used by member-access nodes. */
    Uuid ExecTargetNodeId{}; /**< @brief Explicit exec target for `Out`/`True` flow pins. Empty means unconnected or legacy-label-only. */
    Uuid FalseExecTargetNodeId{}; /**< @brief Explicit false-branch exec target. Used only by `Branch`. */

    SerializedValue ConstantValue{}; /**< @brief Constant payload for `Constant` nodes. */

    EUnaryIntrinsicOp UnaryOp = EUnaryIntrinsicOp::LogicalNot; /**< @brief Unary opcode for `UnaryIntrinsic`. */
    EBinaryIntrinsicOp BinaryOp = EBinaryIntrinsicOp::Add; /**< @brief Binary opcode for `BinaryIntrinsic`. */

    SlotId Input{}; /**< @brief Generic single input slot (unary intrinsic / field write / variable set). */
    SlotId Left{}; /**< @brief Left input slot for binary intrinsics. */
    SlotId Right{}; /**< @brief Right input slot for binary intrinsics. */
    SlotId Output{}; /**< @brief Output slot for constants, reads, intrinsics, variable get, and lifecycle entrypoint delta injection. */
    SlotId Condition{}; /**< @brief Branch condition slot. */
    SlotId Instance{}; /**< @brief Handle slot for instance-bound nodes. */
    SlotId ReturnSlot{}; /**< @brief Optional return/output slot for method calls. Invalid means void/no output. */

    TypeId OwnerType{}; /**< @brief Reflected owner type for instance-bound nodes. */
    std::vector<SlotId> Inputs{}; /**< @brief Ordered argument slots for method calls. */

    bool operator==(const GraphNodeAsset&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Persistent authored graph payload that compiles into a `CompiledGraph`.
 */
struct GraphAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::GraphAsset";
    static constexpr std::uint32_t kSchemaVersion = 5;

    std::string Name{}; /**< @brief Optional authored graph name. */
    TypeId SelfType{}; /**< @brief Reflected self type used by `Self*` nodes. `TypeId{}` defaults to `void`. */
    std::vector<GraphSlotAsset> Slots{}; /**< @brief Authored slot declarations. */
    std::vector<GraphVariableAsset> Variables{}; /**< @brief Authored persistent graph-owned instance variables. */
    std::vector<GraphNodeAsset> Nodes{}; /**< @brief Authored node list in program order. */
    GraphEditorAssetState EditorState{}; /**< @brief Persisted editor metadata such as layout, comments, and viewport state. */

    /**
     * @brief Compile this asset into the runtime graph form.
     * @return Compiled graph or an error.
     */
    [[nodiscard]] TExpected<CompiledGraph> Compile() const;

    [[nodiscard]] std::string_view DisplayName() const override { return "Conduit Graph"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".conduitgraph"; }
    [[nodiscard]] std::string_view Category() const override { return "Conduit"; }
    [[nodiscard]] EAssetEditorMode EditorMode() const override { return EAssetEditorMode::ConduitGraph; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindConduitGraph(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadConduitGraph(); }
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Spawnable Conduit-authored class asset that binds one host node type to one graph asset.
 *
 * `ClassAsset` is the first concrete step toward graph-backed gameplay classes. It does not attempt
 * to generate a native C++ subclass. Instead, it declares the reflected host node type that will act
 * as `self` for the referenced graph. Built-in lifecycle behavior comes from authored graph
 * entrypoint nodes rather than class-wide phase flags.
 */
struct ClassAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::ClassAsset";
    static constexpr std::uint32_t kSchemaVersion = 3;

    std::string Name{}; /**< @brief Optional authored class name. */
    TypeId HostType{}; /**< @brief Reflected node type spawned for this class. Must derive from `BaseNode`. */
    TAssetRef<GraphAsset> Graph{}; /**< @brief Referenced Conduit graph asset that provides the logic body. */

    [[nodiscard]] std::string_view DisplayName() const override { return "Conduit Class"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".conduitclass"; }
    [[nodiscard]] std::string_view Category() const override { return "Conduit"; }
    [[nodiscard]] EAssetEditorMode EditorMode() const override { return EAssetEditorMode::ConduitClass; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindConduitClass(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadConduitClass(); }
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Bound runtime form of one `ClassAsset`.
 *
 * `RuntimeGraph` carries the compiled entrypoint table used both for automatic lifecycle dispatch
 * and for external execution of custom named entries.
 */
struct CompiledClass
{
    std::string Name{}; /**< @brief Authored class name. */
    TypeId HostType{}; /**< @brief Reflected host node type. */
    TypeId EffectiveSelfType{}; /**< @brief Self type used when compiling the resolved graph. */
    TAssetRef<GraphAsset> Graph{}; /**< @brief Source graph asset reference. */
    GraphAsset SourceGraph{}; /**< @brief Resolved graph asset after any self-type injection. */
    CompiledGraph RuntimeGraph{}; /**< @brief Compiled runtime graph bound to the class. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Compile an authored Conduit asset into a runtime `CompiledGraph`.
 * @param Asset Authored asset payload.
 * @return Compiled graph or an error.
 */
TExpected<CompiledGraph> CompileGraphAsset(const GraphAsset& Asset);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Compile one authored Conduit class asset into a bound runtime class.
 * @param Asset Authored class asset.
 * @param AssetManager Borrowed asset manager used to resolve the referenced graph asset.
 * @return Compiled class or an error.
 */
TExpected<CompiledClass> CompileClassAsset(const ClassAsset& Asset, ::SnAPI::AssetPipeline::AssetManager& AssetManager);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Serialize a `GraphAsset` into raw payload bytes.
 * @param Asset Asset to encode.
 * @param OutBytes Destination byte vector.
 * @return Success or error.
 */
TExpected<void> SerializeGraphAsset(const GraphAsset& Asset, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Deserialize a `GraphAsset` from raw payload bytes.
 * @param Bytes Source buffer. Must not be null when @p Size is non-zero.
 * @param Size Byte count.
 * @return Decoded asset or an error.
 */
TExpected<GraphAsset> DeserializeGraphAsset(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Serialize a `ClassAsset` into raw payload bytes.
 * @param Asset Asset to encode.
 * @param OutBytes Destination byte vector.
 * @return Success or error.
 */
TExpected<void> SerializeClassAsset(const ClassAsset& Asset, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Deserialize a `ClassAsset` from raw payload bytes.
 * @param Bytes Source buffer. Must not be null when @p Size is non-zero.
 * @param Size Byte count.
 * @return Decoded asset or an error.
 */
TExpected<ClassAsset> DeserializeClassAsset(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework::Conduit

namespace SnAPI::GameFramework
{

SNAPI_DEFINE_TYPE_NAME(Conduit::EGraphAssetNodeKind, "SnAPI::GameFramework::Conduit::EGraphAssetNodeKind")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::SlotId>, "std::vector<SnAPI::GameFramework::Conduit::SlotId>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::GraphNodeEditorAsset>, "std::vector<SnAPI::GameFramework::Conduit::GraphNodeEditorAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::GraphCommentAsset>, "std::vector<SnAPI::GameFramework::Conduit::GraphCommentAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::GraphBookmarkAsset>, "std::vector<SnAPI::GameFramework::Conduit::GraphBookmarkAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::GraphSlotAsset>, "std::vector<SnAPI::GameFramework::Conduit::GraphSlotAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::GraphVariableAsset>, "std::vector<SnAPI::GameFramework::Conduit::GraphVariableAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Conduit::GraphNodeAsset>, "std::vector<SnAPI::GameFramework::Conduit::GraphNodeAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<Uuid>, "std::vector<SnAPI::GameFramework::Uuid>")

} // namespace SnAPI::GameFramework
