#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "Expected.h"
#include "TypeName.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Dense identifier for one runtime frame slot.
 *
 * A `SlotId` indexes one logical storage location inside a `FrameLayout`.
 * Slots are the core dataflow currency of compiled Conduit graphs.
 *
 * Semantics:
 * - ids are layout-local, not globally stable
 * - ids are assigned in builder order
 * - an invalid id means "no slot" or "slot lookup failed"
 *
 * Typical usage:
 * - builder APIs return `SlotId` when new storage is allocated
 * - compiled nodes read/write slot ids during execution
 * - the authored graph layer should treat slot ids as compiler internals
 */
struct SlotId
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::SlotId";
    static constexpr std::uint32_t InvalidValue = std::numeric_limits<std::uint32_t>::max(); /**< @brief Sentinel used for invalid slot ids. */

    std::uint32_t Value = InvalidValue; /**< @brief Dense slot index within one frame layout. */

    /**
     * @brief Whether this slot id is valid.
     * @return True when `Value` is not the invalid sentinel.
     */
    [[nodiscard]] bool IsValid() const
    {
        return Value != InvalidValue;
    }

    auto operator<=>(const SlotId&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Dense identifier for one builder label used by control-flow fixups.
 *
 * Labels are authored/builder-time placeholders that later resolve to concrete
 * node indices when `GraphBuilder::Build()` applies fixups.
 *
 * They intentionally mirror assembly-style label semantics:
 * - create a label handle
 * - emit jumps/branches that reference it
 * - mark the label at a later node index
 */
struct LabelId
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::LabelId";
    static constexpr std::uint32_t InvalidValue = std::numeric_limits<std::uint32_t>::max(); /**< @brief Sentinel used for invalid labels. */

    std::uint32_t Value = InvalidValue; /**< @brief Dense label index inside one builder instance. */

    /**
     * @brief Whether this label id is valid.
     * @return True when `Value` is not the invalid sentinel.
     */
    [[nodiscard]] bool IsValid() const
    {
        return Value != InvalidValue;
    }

    auto operator<=>(const LabelId&) const = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Reserved built-in entrypoints that map to host lifecycle callbacks.
 *
 * Built-in entrypoints are authored graph roots that the runtime may invoke automatically
 * against a bound host object. Custom entrypoints are represented by `None` plus a user
 * supplied string name.
 */
enum class EBuiltinEntryPoint : std::uint8_t
{
    None,      /**< @brief Custom named entrypoint. */
    OnCreate,  /**< @brief Host/component creation entrypoint. */
    PreTick,   /**< @brief Host pre-tick entrypoint. */
    Tick,      /**< @brief Host tick entrypoint. */
    FixedTick, /**< @brief Host fixed-tick entrypoint. */
    LateTick,  /**< @brief Host late-tick entrypoint. */
    PostTick,  /**< @brief Host post-tick entrypoint. */
    OnDestroy  /**< @brief Host/component teardown entrypoint. */
};

/**
 * @brief Return the canonical authored/runtime name for one built-in entrypoint.
 * @param EntryPoint Built-in entrypoint id.
 * @return Stable string name.
 */
[[nodiscard]] constexpr std::string_view BuiltinEntryPointName(const EBuiltinEntryPoint EntryPoint)
{
    switch (EntryPoint)
    {
    case EBuiltinEntryPoint::None:
        return {};
    case EBuiltinEntryPoint::OnCreate:
        return "OnCreate";
    case EBuiltinEntryPoint::PreTick:
        return "PreTick";
    case EBuiltinEntryPoint::Tick:
        return "Tick";
    case EBuiltinEntryPoint::FixedTick:
        return "FixedTick";
    case EBuiltinEntryPoint::LateTick:
        return "LateTick";
    case EBuiltinEntryPoint::PostTick:
        return "PostTick";
    case EBuiltinEntryPoint::OnDestroy:
        return "OnDestroy";
    }

    return {};
}

/**
 * @brief Whether a built-in entrypoint accepts delta-seconds injection.
 * @param EntryPoint Built-in entrypoint id.
 * @return True for tick-like entrypoints.
 */
[[nodiscard]] constexpr bool BuiltinEntryPointUsesDeltaSeconds(const EBuiltinEntryPoint EntryPoint)
{
    switch (EntryPoint)
    {
    case EBuiltinEntryPoint::PreTick:
    case EBuiltinEntryPoint::Tick:
    case EBuiltinEntryPoint::FixedTick:
    case EBuiltinEntryPoint::LateTick:
    case EBuiltinEntryPoint::PostTick:
        return true;
    case EBuiltinEntryPoint::None:
    case EBuiltinEntryPoint::OnCreate:
    case EBuiltinEntryPoint::OnDestroy:
        return false;
    }

    return false;
}

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Storage/lifetime category for one frame slot.
 *
 * `Value` means the slot owns raw bytes inside the frame.
 * `Handle` means the slot still stores an owned value, but that value is interpreted
 * as a stable handle/reference carrier rather than ordinary dataflow payload.
 *
 * Design intent:
 * - value slots are the default for numbers, strings, structs, enums, and booleans
 * - handle slots are used when runtime execution must later resolve a live instance
 * - Conduit does not persist raw borrows in frame storage
 */
enum class ESlotKind : std::uint8_t
{
    Value,  /**< @brief Owned value stored directly in the frame. */
    Handle  /**< @brief Owned handle value later resolved to a live runtime target. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Runtime opcode classification for compiled Conduit nodes.
 *
 * These kinds are primarily useful for:
 * - debugging
 * - instrumentation
 * - future graph inspectors/disassemblers
 * - validation of builder/compiler output
 *
 * They are not a replacement for `NodeData`; they are the coarse opcode tag.
 */
enum class ENodeKind : std::uint8_t
{
    Constant,          /**< @brief Copy a baked constant into one output slot. */
    SlotCopy,          /**< @brief Copy one initialized slot value into another slot. */
    UnaryIntrinsic,    /**< @brief Execute one built-in unary operation. */
    BinaryIntrinsic,   /**< @brief Execute one built-in binary operation. */
    Jump,              /**< @brief Unconditional control-flow jump. */
    Branch,            /**< @brief Conditional control-flow branch on a bool slot. */
    SelfFieldRead,     /**< @brief Read a reflected field from the execution-context self object. */
    SelfFieldWrite,    /**< @brief Write a reflected field on the execution-context self object. */
    SelfMethodCall,    /**< @brief Invoke a reflected method on the execution-context self object. */
    InstanceFieldRead, /**< @brief Read a reflected field from a handle-resolved instance. */
    InstanceFieldWrite,/**< @brief Write a reflected field on a handle-resolved instance. */
    InstanceMethodCall /**< @brief Invoke a reflected method on a handle-resolved instance. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Built-in unary operations supported by Conduit intrinsic nodes.
 *
 * Intrinsics exist so authored graphs do not need reflected helper methods for
 * fundamental logic and arithmetic operations.
 */
enum class EUnaryIntrinsicOp : std::uint8_t
{
    LogicalNot, /**< @brief Boolean negation (`!Value`). */
    Negate      /**< @brief Numeric negation (`-Value`). */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Built-in binary operations supported by Conduit intrinsic nodes.
 *
 * Support is type-dependent and validated at bind time:
 * - arithmetic/order ops require supported numeric types
 * - logical ops require `bool`
 * - equality can use either specialized intrinsic dispatch or reflected `RuntimeOps->Equals`
 */
enum class EBinaryIntrinsicOp : std::uint8_t
{
    Add,          /**< @brief `Left + Right`. */
    Subtract,     /**< @brief `Left - Right`. */
    Multiply,     /**< @brief `Left * Right`. */
    Divide,       /**< @brief `Left / Right`. */
    Equal,        /**< @brief `Left == Right`. */
    NotEqual,     /**< @brief `Left != Right`. */
    Less,         /**< @brief `Left < Right`. */
    LessEqual,    /**< @brief `Left <= Right`. */
    Greater,      /**< @brief `Left > Right`. */
    GreaterEqual, /**< @brief `Left >= Right`. */
    LogicalAnd,   /**< @brief `Left && Right`. */
    LogicalOr     /**< @brief `Left || Right`. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Result of resolving a handle slot to a live runtime object.
 *
 * `Instance` is a borrowed pointer valid only for the current execute call.
 * `Type` identifies the resolved concrete reflected type and may be more derived
 * than the statically expected owner type used by the node binding.
 */
struct ResolvedTarget
{
    void* Instance = nullptr; /**< @brief Borrowed instance pointer returned by a resolver. */
    const TypeInfo* Type = nullptr; /**< @brief Concrete reflected type of the resolved instance. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Callback signature used to resolve handle-slot payloads to live runtime instances.
 *
 * Parameters:
 * - `UserData`: caller-owned resolver context
 * - `ExpectedType`: type required by the bound field/method node
 * - `HandleType`: reflected type stored in the handle slot
 * - `HandleValue`: pointer to the handle payload stored in the frame
 *
 * Contract:
 * - return a live borrowed instance pointer when resolution succeeds
 * - validate type compatibility and report `TypeMismatch` when needed
 * - do not return long-lived ownership
 */
using HandleResolverFn = TExpected<ResolvedTarget> (*)(const void* UserData,
                                                       const TypeInfo& ExpectedType,
                                                       const TypeInfo& HandleType,
                                                       const void* HandleValue);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Per-execution context supplied to `CompiledGraph::Execute`.
 *
 * This context carries all runtime values that are intentionally external to the frame:
 * - the optional self object for self-bound nodes
 * - the optional reflected type of that self object
 * - the optional custom handle resolver callback
 * - a per-run instruction/node execution guard
 *
 * `MaxNodeExecutions` is the primary runaway-loop safety valve. Lower-level runtime users
 * can tune it when graphs are expected to run for many iterations in one execute call.
 */
struct ExecutionContext
{
    void* Self = nullptr; /**< @brief Borrowed self-instance used by `Self*` nodes. */
    const TypeInfo* SelfType = nullptr; /**< @brief Reflected type of `Self` when known. */
    HandleResolverFn ResolveHandle = nullptr; /**< @brief Optional custom resolver callback used before builtin handle-family lookup. */
    const void* HandleResolverUserData = nullptr; /**< @brief Caller-owned opaque context forwarded to `ResolveHandle`. */
    std::size_t MaxNodeExecutions = 65536; /**< @brief Hard cap on executed nodes for one run to catch runaway control flow. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Metadata for one concrete slot inside a frame layout.
 *
 * A slot description is the compiled contract for one storage location:
 * - type
 * - byte offset
 * - size/alignment
 * - interpretation as value vs handle
 *
 * `FrameStorage` uses this metadata to manage object lifetime in raw storage.
 */
struct SlotDesc
{
    SlotId Id{}; /**< @brief Dense slot id inside the owning layout. */
    const TypeInfo* Type = nullptr; /**< @brief Reflected type stored in the slot. */
    std::uint32_t Offset = 0; /**< @brief Byte offset into frame storage. */
    std::uint32_t Size = 0; /**< @brief Size in bytes of the stored value. */
    std::uint16_t Align = 0; /**< @brief Required alignment for the slot storage. */
    ESlotKind Kind = ESlotKind::Value; /**< @brief Interpretation of the slot payload. */
};

} // namespace SnAPI::GameFramework::Conduit

namespace SnAPI::GameFramework
{

SNAPI_DEFINE_TYPE_NAME(Conduit::EBuiltinEntryPoint, "SnAPI::GameFramework::Conduit::EBuiltinEntryPoint")
SNAPI_DEFINE_TYPE_NAME(Conduit::ESlotKind, "SnAPI::GameFramework::Conduit::ESlotKind")
SNAPI_DEFINE_TYPE_NAME(Conduit::ENodeKind, "SnAPI::GameFramework::Conduit::ENodeKind")
SNAPI_DEFINE_TYPE_NAME(Conduit::EUnaryIntrinsicOp, "SnAPI::GameFramework::Conduit::EUnaryIntrinsicOp")
SNAPI_DEFINE_TYPE_NAME(Conduit::EBinaryIntrinsicOp, "SnAPI::GameFramework::Conduit::EBinaryIntrinsicOp")

} // namespace SnAPI::GameFramework
