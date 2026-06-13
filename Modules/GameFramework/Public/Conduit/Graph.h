#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Conduit/Frame.h"
#include "Conduit/Value.h"

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for copying a baked constant into a frame slot.
 *
 * Constants are stored as durable serialized reflected values so compiled graphs can safely
 * carry non-trivial types such as strings and reflected structs.
 */
struct ConstantNodeData
{
    SlotId Output{}; /**< @brief Destination slot. */
    const TypeInfo* Type = nullptr; /**< @brief Reflected constant type. */
    SerializedValue Value{}; /**< @brief Baked constant payload. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for copying one live slot value into another slot.
 */
struct SlotCopyNodeData
{
    SlotId Source{}; /**< @brief Source slot. */
    SlotId Destination{}; /**< @brief Destination slot. */
    const TypeInfo* Type = nullptr; /**< @brief Shared reflected type for both slots. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for default-constructing one value slot at runtime.
 */
struct DefaultConstructNodeData
{
    SlotId Output{}; /**< @brief Destination slot. */
    const TypeInfo* Type = nullptr; /**< @brief Reflected slot type. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Erased callable for one unary intrinsic implementation.
 *
 * The function reads one input object and writes one output object.
 * Type compatibility is validated by the builder before the node is baked.
 */
using UnaryIntrinsicFn = Result (*)(const void* Input, void* Output);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Erased callable for one binary intrinsic implementation.
 *
 * The function reads two input objects and writes one output object.
 */
using BinaryIntrinsicFn = Result (*)(const void* Left, const void* Right, void* Output);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for one built-in unary operation.
 */
struct UnaryIntrinsicNodeData
{
    SlotId Input{}; /**< @brief Input slot. */
    SlotId Output{}; /**< @brief Output slot. */
    EUnaryIntrinsicOp Op = EUnaryIntrinsicOp::LogicalNot; /**< @brief Opcode for debugging/inspection. */
    const TypeInfo* InputType = nullptr; /**< @brief Reflected input type. */
    const TypeInfo* OutputType = nullptr; /**< @brief Reflected output type. */
    UnaryIntrinsicFn ExecuteIntrinsic = nullptr; /**< @brief Bound intrinsic implementation. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for one built-in binary operation.
 *
 * For equality, `ExecuteIntrinsic` may be null when the builder selected the generic
 * reflected `RuntimeOps->Equals` fallback path.
 */
struct BinaryIntrinsicNodeData
{
    SlotId Left{}; /**< @brief Left input slot. */
    SlotId Right{}; /**< @brief Right input slot. */
    SlotId Output{}; /**< @brief Output slot. */
    EBinaryIntrinsicOp Op = EBinaryIntrinsicOp::Add; /**< @brief Opcode for debugging/inspection. */
    const TypeInfo* ValueType = nullptr; /**< @brief Reflected operand type. */
    const TypeInfo* OutputType = nullptr; /**< @brief Reflected result type. */
    BinaryIntrinsicFn ExecuteIntrinsic = nullptr; /**< @brief Bound intrinsic implementation, when specialized dispatch exists. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for an unconditional jump.
 */
struct JumpNodeData
{
    static constexpr std::uint32_t InvalidTarget = std::numeric_limits<std::uint32_t>::max(); /**< @brief Sentinel for unresolved targets. */

    std::uint32_t TargetNode = InvalidTarget; /**< @brief Absolute node index to jump to. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for a boolean branch.
 */
struct BranchNodeData
{
    static constexpr std::uint32_t InvalidTarget = std::numeric_limits<std::uint32_t>::max(); /**< @brief Sentinel for unresolved targets. */

    SlotId Condition{}; /**< @brief Bool slot controlling branch direction. */
    std::uint32_t TrueTarget = InvalidTarget; /**< @brief Target node index when condition is true. */
    std::uint32_t FalseTarget = InvalidTarget; /**< @brief Target node index when condition is false. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for reading a reflected field from `ExecutionContext::Self`.
 */
struct SelfFieldReadNodeData
{
    SlotId Output{}; /**< @brief Destination slot. */
    const TypeInfo* OwnerType = nullptr; /**< @brief Reflected owner type expected for self. */
    const FieldInfo* Field = nullptr; /**< @brief Bound reflected field metadata. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for writing a reflected field on `ExecutionContext::Self`.
 */
struct SelfFieldWriteNodeData
{
    SlotId Input{}; /**< @brief Source slot. */
    const TypeInfo* OwnerType = nullptr; /**< @brief Reflected owner type expected for self. */
    const FieldInfo* Field = nullptr; /**< @brief Bound reflected field metadata. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for invoking a reflected method on `ExecutionContext::Self`.
 */
struct SelfMethodCallNodeData
{
    std::vector<SlotId> Inputs; /**< @brief Ordered argument slots. */
    std::optional<SlotId> Output; /**< @brief Optional return slot for non-void methods. */
    const TypeInfo* OwnerType = nullptr; /**< @brief Reflected owner type expected for self. */
    const MethodInfo* Method = nullptr; /**< @brief Bound reflected method metadata. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for reading a reflected field from a handle-resolved instance.
 */
struct InstanceFieldReadNodeData
{
    SlotId Instance{}; /**< @brief Handle slot identifying the target object. */
    SlotId Output{}; /**< @brief Destination slot. */
    const TypeInfo* OwnerType = nullptr; /**< @brief Reflected owner type required by the node. */
    const FieldInfo* Field = nullptr; /**< @brief Bound reflected field metadata. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for writing a reflected field on a handle-resolved instance.
 */
struct InstanceFieldWriteNodeData
{
    SlotId Instance{}; /**< @brief Handle slot identifying the target object. */
    SlotId Input{}; /**< @brief Source slot. */
    const TypeInfo* OwnerType = nullptr; /**< @brief Reflected owner type required by the node. */
    const FieldInfo* Field = nullptr; /**< @brief Bound reflected field metadata. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Node payload for invoking a reflected method on a handle-resolved instance.
 */
struct InstanceMethodCallNodeData
{
    SlotId Instance{}; /**< @brief Handle slot identifying the target object. */
    std::vector<SlotId> Inputs; /**< @brief Ordered argument slots. */
    std::optional<SlotId> Output; /**< @brief Optional return slot for non-void methods. */
    const TypeInfo* OwnerType = nullptr; /**< @brief Reflected owner type required by the node. */
    const MethodInfo* Method = nullptr; /**< @brief Bound reflected method metadata. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Variant over every public compiled-node payload type.
 *
 * `NodeData` is the data half of one compiled opcode.
 * The `Execute` function stored in `BoundNode` determines which alternative is valid.
 */
using NodeData = std::variant<ConstantNodeData,
                              SlotCopyNodeData,
                              DefaultConstructNodeData,
                              UnaryIntrinsicNodeData,
                              BinaryIntrinsicNodeData,
                              JumpNodeData,
                              BranchNodeData,
                              SelfFieldReadNodeData,
                              SelfFieldWriteNodeData,
                              SelfMethodCallNodeData,
                              InstanceFieldReadNodeData,
                              InstanceFieldWriteNodeData,
                              InstanceMethodCallNodeData>;

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Control-flow decision returned by one executed node.
 *
 * When `NextNodeIndex` is empty, execution falls through to the next node.
 * When set, the graph interpreter jumps to that absolute node index.
 */
struct NodeExecutionControl
{
    std::optional<std::uint32_t> NextNodeIndex{}; /**< @brief Optional explicit next node index. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Canonical result type returned by one runtime opcode.
 */
using NodeExecuteResult = TExpected<NodeExecutionControl>;

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Canonical erased execute signature for one compiled node.
 *
 * Parameters:
 * - `Data`: payload for this opcode
 * - `Frame`: current frame storage
 * - `Context`: caller-supplied execution context
 * - `ScratchArgs`: temporary pointer array reused for reflected method invocation
 */
using ExecuteNodeFn = NodeExecuteResult (*)(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            std::span<void*> ScratchArgs);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief One compiled runtime node/opcode.
 *
 * This is the lowest-level executable unit in Conduit today.
 */
struct BoundNode
{
    ENodeKind Kind = ENodeKind::Constant; /**< @brief Coarse opcode tag. */
    ExecuteNodeFn Execute = nullptr; /**< @brief Erased runtime execute callback. */
    NodeData Data{}; /**< @brief Payload for the specific opcode. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief One compiled graph entrypoint.
 *
 * Entry points are authored callable roots. They define bounded execution regions so
 * `OnCreate`, `Tick`, and custom entries can coexist in one linear compiled node array
 * without falling through into each other.
 */
struct GraphEntryPoint
{
    std::string Name{}; /**< @brief Stable entrypoint name used for lookup and diagnostics. */
    EBuiltinEntryPoint Builtin = EBuiltinEntryPoint::None; /**< @brief Reserved lifecycle id when this is a built-in entrypoint. */
    std::uint32_t StartNodeIndex = 0; /**< @brief First executable node in this entrypoint region. */
    std::uint32_t EndNodeIndex = 0; /**< @brief One-past-the-end node index for this entrypoint region. */
    SlotId DeltaSecondsSlot{}; /**< @brief Optional float slot injected before tick-like lifecycle entrypoints execute. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief One compiled persistent graph variable.
 */
struct CompiledGraphVariable
{
    Uuid Id{}; /**< @brief Stable authored variable id. */
    std::string Name{}; /**< @brief Authored variable name. */
    SlotId Slot{}; /**< @brief Fixed frame slot used to store the live variable value. */
    const TypeInfo* Type = nullptr; /**< @brief Reflected variable type. */
    SerializedValue DefaultValue{}; /**< @brief Optional authored default payload. Empty type means use default construction. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Fully bound, executable Conduit graph.
 *
 * A compiled graph contains:
 * - one frame layout
 * - one ordered opcode list
 * - the maximum scratch argument count needed by reflected method call nodes
 *
 * This is the runtime product produced by `GraphBuilder` and eventually by the authored
 * Conduit graph asset compiler.
 */
struct CompiledGraph
{
    FrameLayout Layout; /**< @brief Slot layout for one graph instance. */
    std::vector<BoundNode> Nodes; /**< @brief Ordered executable nodes. */
    std::vector<GraphEntryPoint> EntryPoints; /**< @brief Named callable roots compiled into bounded execution regions. */
    std::vector<CompiledGraphVariable> Variables; /**< @brief Persistent graph-owned instance variables compiled into fixed slots. */
    std::size_t MaxScratchArgs = 0; /**< @brief Maximum temporary argument pointer count needed by any method call node. */

    /**
     * @brief Execute the compiled graph against one frame and context.
     * @param Frame Owned frame storage for this run.
     * @param Context Runtime execution context.
     * @param ScratchArgs Caller-owned scratch argument buffer.
     * @return Success or error.
     *
     * Execution model:
     * - starts at the first explicit entrypoint when present, otherwise node index 0
     * - follows fallthrough or explicit jump results
     * - stops when the program counter exits the node array
     * - enforces `ExecutionContext::MaxNodeExecutions`
     */
    Result Execute(FrameStorage& Frame, const ExecutionContext& Context, std::span<void*> ScratchArgs) const;

    /**
     * @brief Execute a specific named entrypoint.
     * @param Name Entrypoint name.
     * @param Frame Owned frame storage for this run.
     * @param Context Runtime execution context.
     * @param ScratchArgs Caller-owned scratch argument buffer.
     * @return Success or error.
     */
    Result ExecuteEntry(std::string_view Name,
                        FrameStorage& Frame,
                        const ExecutionContext& Context,
                        std::span<void*> ScratchArgs) const;

    /**
     * @brief Execute a specific built-in lifecycle entrypoint.
     * @param Builtin Entrypoint id.
     * @param Frame Owned frame storage for this run.
     * @param Context Runtime execution context.
     * @param ScratchArgs Caller-owned scratch argument buffer.
     * @return Success or error.
     */
    Result ExecuteEntry(EBuiltinEntryPoint Builtin,
                        FrameStorage& Frame,
                        const ExecutionContext& Context,
                        std::span<void*> ScratchArgs) const;

    /**
     * @brief Find a compiled named entrypoint.
     * @param Name Entrypoint name.
     * @return Pointer to entrypoint metadata or null.
     */
    [[nodiscard]] const GraphEntryPoint* FindEntryPoint(std::string_view Name) const;

    /**
     * @brief Find a compiled built-in entrypoint.
     * @param Builtin Entrypoint id.
     * @return Pointer to entrypoint metadata or null.
     */
    [[nodiscard]] const GraphEntryPoint* FindEntryPoint(EBuiltinEntryPoint Builtin) const;

    /**
     * @brief Find a compiled graph-owned variable by name.
     * @param Name Variable name.
     * @return Pointer to variable metadata or null.
     */
    [[nodiscard]] const CompiledGraphVariable* FindVariable(std::string_view Name) const;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Live runtime instance of a compiled graph.
 *
 * `GraphInstance` owns:
 * - one frame allocation
 * - one reusable scratch argument buffer
 *
 * It does not own the `CompiledGraph`; the compiled graph is expected to outlive the instance.
 */
class GraphInstance
{
public:
    /**
     * @brief Construct an instance for one compiled graph.
     * @param Graph Compiled graph to instantiate.
     */
    explicit GraphInstance(const CompiledGraph& Graph);

    /**
     * @brief Mutable access to frame storage.
     * @return Frame storage.
     */
    [[nodiscard]] FrameStorage& Frame()
    {
        return m_frame;
    }

    /**
     * @brief Const access to frame storage.
     * @return Frame storage.
     */
    [[nodiscard]] const FrameStorage& Frame() const
    {
        return m_frame;
    }

    /**
     * @brief Execute the graph against the current frame.
     * @param Context Runtime execution context.
     * @return Success or error.
     */
    Result Execute(const ExecutionContext& Context);

    /**
     * @brief Execute one named entrypoint against the current frame.
     * @param Name Entrypoint name.
     * @param Context Runtime execution context.
     * @return Success or error.
     */
    Result ExecuteEntry(std::string_view Name, const ExecutionContext& Context);

    /**
     * @brief Execute one built-in entrypoint against the current frame.
     * @param Builtin Entrypoint id.
     * @param Context Runtime execution context.
     * @return Success or error.
     */
    Result ExecuteEntry(EBuiltinEntryPoint Builtin, const ExecutionContext& Context);

private:
    const CompiledGraph* m_graph = nullptr; /**< @brief Non-owning pointer to the compiled graph contract. */
    FrameStorage m_frame; /**< @brief Owned frame storage for this instance. */
    std::vector<void*> m_scratchArgs; /**< @brief Reused scratch buffer for reflected method invocation. */
    std::optional<Error> m_initializationError{}; /**< @brief Deferred constructor-time frame initialization failure, if any. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @name Node Execute Functions
 * @{
 * These functions are the public runtime opcode entry points used by `BoundNode::Execute`.
 * They are primarily exposed for testing, diagnostics, and future compiler/debug tooling.
 */
NodeExecuteResult ExecuteConstantNode(const NodeData& Data,
                                      FrameStorage& Frame,
                                      const ExecutionContext& Context,
                                      std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteSlotCopyNode(const NodeData& Data,
                                      FrameStorage& Frame,
                                      const ExecutionContext& Context,
                                      std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteDefaultConstructNode(const NodeData& Data,
                                             FrameStorage& Frame,
                                             const ExecutionContext& Context,
                                             std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteUnaryIntrinsicNode(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteBinaryIntrinsicNode(const NodeData& Data,
                                             FrameStorage& Frame,
                                             const ExecutionContext& Context,
                                             std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteJumpNode(const NodeData& Data,
                                  FrameStorage& Frame,
                                  const ExecutionContext& Context,
                                  std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteBranchNode(const NodeData& Data,
                                    FrameStorage& Frame,
                                    const ExecutionContext& Context,
                                    std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteSelfFieldReadNode(const NodeData& Data,
                                           FrameStorage& Frame,
                                           const ExecutionContext& Context,
                                           std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteSelfFieldWriteNode(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteSelfMethodCallNode(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteInstanceFieldReadNode(const NodeData& Data,
                                               FrameStorage& Frame,
                                               const ExecutionContext& Context,
                                               std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteInstanceFieldWriteNode(const NodeData& Data,
                                                FrameStorage& Frame,
                                                const ExecutionContext& Context,
                                                std::span<void*> ScratchArgs);
NodeExecuteResult ExecuteInstanceMethodCallNode(const NodeData& Data,
                                                FrameStorage& Frame,
                                                const ExecutionContext& Context,
                                                std::span<void*> ScratchArgs);
/** @} */

} // namespace SnAPI::GameFramework::Conduit
