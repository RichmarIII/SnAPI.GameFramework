#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Conduit/Graph.h"

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Low-level builder that binds reflected operations into a `CompiledGraph`.
 *
 * `GraphBuilder` is the current compiler/binding surface for Conduit runtime graphs.
 * It is intentionally lower-level than the eventual authored graph asset compiler.
 *
 * Responsibilities:
 * - allocate typed frame slots
 * - bind reflected fields and methods against known owner types
 * - emit intrinsic and control-flow nodes
 * - assign labels and resolve jumps/branches
 * - produce a fully validated `CompiledGraph`
 *
 * Mental model:
 * - this is closer to an assembler than to an end-user node editor
 * - higher-level authored graph assets should compile down to this API
 * - all expensive lookup/validation happens here, not during hot execution
 */
class GraphBuilder
{
public:
    /**
     * @brief Construct a builder with a known self type.
     * @param SelfType Reflected type used by `AddSelf*` node helpers.
     */
    explicit GraphBuilder(const TypeInfo& SelfType);

    /** @brief Access the stable reflected self type bound into this builder. */
    [[nodiscard]] const TypeInfo& SelfType() const { return *m_selfType; }

    /**
     * @brief Add one slot using reflected type metadata.
     * @param Type Reflected slot type.
     * @param Kind Value vs handle interpretation.
     * @return Newly assigned slot id or an error.
     */
    TExpected<SlotId> AddSlot(const TypeInfo& Type, ESlotKind Kind = ESlotKind::Value);

    /**
     * @brief Add one slot by type id.
     * @param Type Reflected type id.
     * @param Kind Value vs handle interpretation.
     * @return Newly assigned slot id or an error.
     */
    TExpected<SlotId> AddSlot(const TypeId& Type, ESlotKind Kind = ESlotKind::Value);

    /**
     * @brief Allocate a new unresolved control-flow label.
     * @return New label id.
     */
    LabelId CreateLabel();

    /**
     * @brief Bind a label to the current node position.
     * @param Label Label previously created by `CreateLabel()`.
     * @return Success or error.
     */
    Result MarkLabel(LabelId Label);

    /**
     * @brief Register an authored graph entrypoint at the current node position.
     * @param Name Entrypoint name. For built-ins this may be empty and the canonical built-in name will be used.
     * @param Builtin Reserved lifecycle id, or `None` for a custom entrypoint.
     * @param DeltaSecondsSlot Optional float slot injected before tick-like built-in entrypoints execute.
     * @return Success or error.
     */
    Result AddEntryPoint(std::string_view Name,
                         EBuiltinEntryPoint Builtin = EBuiltinEntryPoint::None,
                         SlotId DeltaSecondsSlot = {});

    /**
     * @brief Emit a constant-copy node.
     * @param Output Destination slot.
     * @param Value Constant payload.
     * @return Success or error.
     */
    Result AddConstant(SlotId Output, Variant Value);
    /**
     * @brief Emit a constant node from a durable serialized value payload.
     * @param Output Destination slot.
     * @param Value Serialized constant payload.
     * @return Success or error.
     */
    Result AddSerializedConstant(SlotId Output, SerializedValue Value);

    /**
     * @brief Emit a slot-to-slot copy node.
     * @param Source Source slot.
     * @param Destination Destination slot.
     * @return Success or error.
     */
    Result AddCopy(SlotId Source, SlotId Destination);

    /**
     * @brief Emit a built-in unary intrinsic node.
     * @param Op Unary opcode.
     * @param Input Source slot.
     * @param Output Destination slot.
     * @return Success or error.
     */
    Result AddUnaryIntrinsic(EUnaryIntrinsicOp Op, SlotId Input, SlotId Output);

    /**
     * @brief Emit a built-in binary intrinsic node.
     * @param Op Binary opcode.
     * @param Left Left input slot.
     * @param Right Right input slot.
     * @param Output Destination slot.
     * @return Success or error.
     *
     * Equality and inequality support a generic reflected fallback when
     * `TypeRuntimeOps::Equals` is available.
     */
    Result AddBinaryIntrinsic(EBinaryIntrinsicOp Op, SlotId Left, SlotId Right, SlotId Output);

    /**
     * @brief Emit an unconditional jump.
     * @param Target Label to jump to.
     * @return Success or error.
     */
    Result AddJump(LabelId Target);

    /**
     * @brief Emit a conditional branch on a bool slot.
     * @param Condition Bool slot controlling the branch.
     * @param TrueTarget Label when condition is true.
     * @param FalseTarget Label when condition is false.
     * @return Success or error.
     */
    Result AddBranch(SlotId Condition, LabelId TrueTarget, LabelId FalseTarget);

    /**
     * @brief Emit one runtime default-construction step for a reflected slot.
     * @param Output Destination slot.
     * @return Success or error.
     */
    Result AddDefaultConstruct(SlotId Output);

    /**
     * @brief Emit a reflected self-field read.
     * @param FieldName Reflected field name on the self type.
     * @param Output Destination slot.
     * @return Success or error.
     */
    Result AddSelfFieldRead(std::string_view FieldName, SlotId Output);

    /**
     * @brief Emit a reflected self-field write.
     * @param FieldName Reflected field name on the self type.
     * @param Input Source slot.
     * @return Success or error.
     */
    Result AddSelfFieldWrite(std::string_view FieldName, SlotId Input);

    /**
     * @brief Emit a reflected self-method call.
     * @param Name Reflected method name.
     * @param Inputs Ordered argument slots.
     * @param Output Optional return slot for non-void methods.
     * @return Success or error.
     */
    Result AddSelfMethodCall(std::string_view Name, std::span<const SlotId> Inputs, std::optional<SlotId> Output = std::nullopt);

    /**
     * @brief Emit a reflected field read against a handle-resolved instance.
     * @param OwnerType Reflected owner type expected by the node.
     * @param Instance Handle slot identifying the target object.
     * @param FieldName Reflected field name.
     * @param Output Destination slot.
     * @return Success or error.
     */
    Result AddFieldRead(const TypeInfo& OwnerType, SlotId Instance, std::string_view FieldName, SlotId Output);

    /**
     * @brief Overload of `AddFieldRead` using a type id.
     */
    Result AddFieldRead(const TypeId& OwnerType, SlotId Instance, std::string_view FieldName, SlotId Output);

    /**
     * @brief Emit a reflected field write against a handle-resolved instance.
     * @param OwnerType Reflected owner type expected by the node.
     * @param Instance Handle slot identifying the target object.
     * @param FieldName Reflected field name.
     * @param Input Source slot.
     * @return Success or error.
     */
    Result AddFieldWrite(const TypeInfo& OwnerType, SlotId Instance, std::string_view FieldName, SlotId Input);

    /**
     * @brief Overload of `AddFieldWrite` using a type id.
     */
    Result AddFieldWrite(const TypeId& OwnerType, SlotId Instance, std::string_view FieldName, SlotId Input);

    /**
     * @brief Emit a reflected method call against a handle-resolved instance.
     * @param OwnerType Reflected owner type expected by the node.
     * @param Instance Handle slot identifying the target object.
     * @param Name Reflected method name.
     * @param Inputs Ordered argument slots.
     * @param Output Optional return slot for non-void methods.
     * @return Success or error.
     */
    Result AddMethodCall(const TypeInfo& OwnerType,
                         SlotId Instance,
                         std::string_view Name,
                         std::span<const SlotId> Inputs,
                         std::optional<SlotId> Output = std::nullopt);

    /**
     * @brief Overload of `AddMethodCall` using a type id.
     */
    Result AddMethodCall(const TypeId& OwnerType,
                         SlotId Instance,
                         std::string_view Name,
                         std::span<const SlotId> Inputs,
                         std::optional<SlotId> Output = std::nullopt);

    /**
     * @brief Finalize the builder into a compiled graph.
     * @return Compiled graph or an error.
     *
     * Finalization performs remaining fixups and validation such as:
     * - unresolved label detection
     * - jump/branch target patching
     * - preservation of max scratch-argument requirements
     */
    TExpected<CompiledGraph> Build() &&;

private:
    /**
     * @brief Internal fixup target kind for one unresolved label reference.
     */
    enum class ELabelFixupKind : std::uint8_t
    {
        JumpTarget,       /**< @brief Patch `JumpNodeData::TargetNode`. */
        BranchTrueTarget, /**< @brief Patch `BranchNodeData::TrueTarget`. */
        BranchFalseTarget /**< @brief Patch `BranchNodeData::FalseTarget`. */
    };

    /**
     * @brief One pending label patch to apply during `Build()`.
     */
    struct LabelFixup
    {
        std::uint32_t NodeIndex = 0; /**< @brief Node whose payload needs patching. */
        LabelId Label{}; /**< @brief Target label to resolve. */
        ELabelFixupKind Kind = ELabelFixupKind::JumpTarget; /**< @brief Which payload field to patch. */
    };

    /**
     * @brief One authored entrypoint captured before final compiled-range resolution.
     */
    struct EntryPointDef
    {
        std::string Name{}; /**< @brief Stable entrypoint name. */
        EBuiltinEntryPoint Builtin = EBuiltinEntryPoint::None; /**< @brief Reserved lifecycle id, when applicable. */
        std::uint32_t StartNodeIndex = 0; /**< @brief First node emitted after the entrypoint marker. */
        SlotId DeltaSecondsSlot{}; /**< @brief Optional float slot used for tick-like lifecycle entrypoints. */
    };

    /**
     * @brief Internal slot lookup helper.
     * @param Id Slot id.
     * @return Slot metadata or null.
     */
    [[nodiscard]] const SlotDesc* FindSlot(SlotId Id) const;
    /**
     * @brief Resolve a reflected type id to stable metadata.
     * @param Type Reflected type id.
     * @return Type metadata or an error.
     */
    TExpected<const TypeInfo*> ResolveType(const TypeId& Type) const;
    /**
     * @brief Bind one reflected field by name.
     * @param OwnerType Reflected owner type.
     * @param FieldName Field name.
     * @return Field metadata or an error.
     */
    TExpected<const FieldInfo*> BindField(const TypeInfo& OwnerType, std::string_view FieldName) const;
    /**
     * @brief Bind one reflected method by name/signature.
     * @param OwnerType Reflected owner type.
     * @param Name Method name.
     * @param Inputs Ordered input slots used to validate argument types.
     * @return Method metadata or an error.
     */
    TExpected<const MethodInfo*> BindMethod(const TypeInfo& OwnerType, std::string_view Name, std::span<const SlotId> Inputs) const;
    /**
     * @brief Validate that a slot is suitable for handle-based instance resolution.
     * @param Slot Slot metadata.
     * @return Success or error.
     */
    Result ValidateInstanceSlot(const SlotDesc& Slot, const TypeInfo& OwnerType, bool RequireMutable) const;
    /**
     * @brief Validate that a slot is a bool value slot suitable for branching.
     * @param Condition Slot id.
     * @return Success or error.
     */
    Result ValidateConditionSlot(SlotId Condition) const;
    /**
     * @brief Validate that a slot is a normal value slot.
     * @param Slot Slot id.
     * @param ContextName Human-readable error context.
     * @return Success or error.
     */
    Result ValidateValueSlot(SlotId Slot, std::string_view ContextName) const;
    /**
     * @brief Validate that a label belongs to this builder.
     * @param Label Label id.
     * @return Success or error.
     */
    Result ValidateLabel(LabelId Label) const;
    Result ValidateEntryPointName(std::string_view Name, EBuiltinEntryPoint Builtin) const;
    /**
     * @brief Resolve every pending label fixup into concrete node indices.
     * @return Success or error.
     */
    Result ApplyLabelFixups();

    const TypeInfo* m_selfType = nullptr; /**< @brief Stable reflected self type used by `AddSelf*` helpers. */
    FrameLayout m_layout{}; /**< @brief Accumulated frame layout. */
    std::vector<BoundNode> m_nodes; /**< @brief Ordered emitted nodes. */
    std::vector<std::optional<std::uint32_t>> m_labels; /**< @brief Builder-local label table. */
    std::vector<EntryPointDef> m_entryPoints; /**< @brief Authored entrypoints captured in program order. */
    std::vector<LabelFixup> m_labelFixups; /**< @brief Pending jump/branch patches. */
    std::size_t m_maxScratchArgs = 0; /**< @brief Maximum reflected method argument count emitted so far. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Backward-compatible alias retained during the Conduit builder rename.
 *
 * New code should prefer `GraphBuilder`.
 */
using SelfGraphBuilder = GraphBuilder;

} // namespace SnAPI::GameFramework::Conduit
