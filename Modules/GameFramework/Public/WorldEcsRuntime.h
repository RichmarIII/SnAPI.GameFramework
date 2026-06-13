#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Handles.h"
#include "ObjectRegistry.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class IWorld;
class BaseNode;
class BaseComponent;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Access the thread-local suppression flag used to defer runtime/component `OnCreate` execution.
 *
 * This low-level flag is primarily for bootstrap code that needs objects to exist before
 * their `OnCreate` hooks are allowed to run. The flag is thread-local so one thread can
 * defer creation callbacks without affecting unrelated owner threads.
 *
 * @return Reference to the current thread's suppression flag.
 *
 * @warning Prefer `ScopedComponentOnCreateSuppression` over mutating this flag directly.
 */
inline bool& ComponentOnCreateSuppressionFlag()
{
    static thread_local bool SuppressOnCreate = false;
    return SuppressOnCreate;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Check whether runtime/component `OnCreate` hooks are currently suppressed on this thread.
 * @return `true` when newly created runtime objects should defer `OnCreate` until a later flush.
 */
inline bool IsComponentOnCreateSuppressed()
{
    return ComponentOnCreateSuppressionFlag();
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief RAII helper that defers runtime/component `OnCreate` hooks for the current thread.
 *
 * Nested scopes are supported. Destruction restores the previous suppression state rather
 * than unconditionally setting the flag to `false`.
 *
 * Typical usage:
 * - create nodes/components/runtime records during bootstrap or deserialization
 * - initialize dependent systems
 * - call the relevant `FlushPendingOnCreate()` API once the environment is ready
 *
 * Threading:
 * - Affects only the current thread because the underlying flag is thread-local.
 */
class ScopedComponentOnCreateSuppression
{
public:
    /** @brief Enable `OnCreate` suppression for the current thread, preserving the previous state. */
    ScopedComponentOnCreateSuppression()
        : m_previousState(ComponentOnCreateSuppressionFlag())
    {
        ComponentOnCreateSuppressionFlag() = true;
    }

    /** @brief Restore the previous suppression state for the current thread. */
    ~ScopedComponentOnCreateSuppression()
    {
        ComponentOnCreateSuppressionFlag() = m_previousState;
    }

    ScopedComponentOnCreateSuppression(const ScopedComponentOnCreateSuppression&) = delete;
    ScopedComponentOnCreateSuppression& operator=(const ScopedComponentOnCreateSuppression&) = delete;

private:
    bool m_previousState = false;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time contract for types that may live in the dense ECS runtime.
 *
 * Runtime objects are intentionally constrained:
 * - they must be class types
 * - they must be non-polymorphic
 * - they must expose a static reflected name through `TTypeNameV<T>`
 *
 * The main design goal is hot-path storage and ticking without vtable dispatch or heap
 * indirection per object.
 */
template<typename T>
concept NonPolymorphicRuntimeType =
    std::is_class_v<T> &&
    !std::is_polymorphic_v<T> &&
    requires {
        { TTypeNameV<T> } -> std::convertible_to<const char*>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief Marker CRTP base for dense runtime objects that participate in the generic runtime-phase system.
 * @tparam TDerived Concrete runtime type.
 *
 * The base is marker-only. It does not provide virtual hooks or storage. Lifecycle
 * functions are discovered directly on `TDerived` via the surrounding concepts.
 */
template<typename TDerived>
struct TRuntimeTickCRTP
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` derives from `TRuntimeTickCRTP<T>`.
 */
template<typename T>
inline constexpr bool kUsesRuntimeTickCRTP = std::is_base_of_v<TRuntimeTickCRTP<T>, T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Marker CRTP base for dense runtime node types.
 * @tparam TDerived Concrete runtime node type.
 */
template<typename TDerived>
struct NodeCRTP
{
    NodeCRTP() = default;
    NodeCRTP(const NodeCRTP&) = delete;
    NodeCRTP& operator=(const NodeCRTP&) = delete;
    NodeCRTP(NodeCRTP&&) noexcept = default;
    NodeCRTP& operator=(NodeCRTP&&) noexcept = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Marker CRTP base for dense runtime component records.
 * @tparam TDerived Concrete runtime component type.
 */
template<typename TDerived>
struct ComponentCRTP
{
    ComponentCRTP() = default;
    ComponentCRTP(const ComponentCRTP&) = delete;
    ComponentCRTP& operator=(const ComponentCRTP&) = delete;
    ComponentCRTP(ComponentCRTP&&) noexcept = default;
    ComponentCRTP& operator=(ComponentCRTP&&) noexcept = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` derives from `NodeCRTP<T>`.
 */
template<typename T>
inline constexpr bool kUsesNodeCRTP = std::is_base_of_v<NodeCRTP<T>, T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` derives from `ComponentCRTP<T>`.
 */
template<typename T>
inline constexpr bool kUsesComponentCRTP = std::is_base_of_v<ComponentCRTP<T>, T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` is safe to relocate inside dense ECS storage by move.
 *
 * Node/component runtime types are expected to be move-only value wrappers whose runtime
 * side effects are driven explicitly through `OnCreate()` / `OnDestroy()`, not through copy
 * semantics or heavy destructor work during container relocation.
 */
template<typename T>
concept DenseRuntimeRelocatableType =
    std::is_move_constructible_v<T> &&
    std::is_nothrow_move_constructible_v<T> &&
    std::is_move_assignable_v<T> &&
    std::is_nothrow_move_assignable_v<T> &&
    std::is_nothrow_destructible_v<T> &&
    !std::is_copy_constructible_v<T> &&
    !std::is_copy_assignable_v<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` is a dense ECS node type with the required relocation contract.
 */
template<typename T>
concept DenseRuntimeNodeType = kUsesNodeCRTP<T> && DenseRuntimeRelocatableType<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` is a dense ECS component type with the required relocation contract.
 */
template<typename T>
concept DenseRuntimeComponentType = kUsesComponentCRTP<T> && DenseRuntimeRelocatableType<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time contract for types eligible to live in `TDenseRuntimeStorage`.
 *
 * A valid runtime type must satisfy `NonPolymorphicRuntimeType` and opt into one of the
 * marker CRTP families. Optional lifecycle hooks such as `OnCreate`, `Tick`,
 * `FixedTick`, `LateTick`, and `PostTick` are then detected automatically.
 */
template<typename T>
concept RuntimeTickType =
    NonPolymorphicRuntimeType<T> &&
    (kUsesRuntimeTickCRTP<T> || DenseRuntimeNodeType<T> || DenseRuntimeComponentType<T>);

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void OnCreate(IWorld&)`.
 */
template<typename T>
concept DeclaresOnCreateWithWorld =
    requires {
        { &T::OnCreate } -> std::same_as<void (T::*)(IWorld&)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void OnCreate()`.
 */
template<typename T>
concept DeclaresOnCreateNoWorld =
    requires {
        { &T::OnCreate } -> std::same_as<void (T::*)()>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `OnCreate` signature.
 */
template<typename T>
concept HasRuntimeOnCreatePhase = DeclaresOnCreateWithWorld<T> || DeclaresOnCreateNoWorld<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void OnDestroy(IWorld&)`.
 */
template<typename T>
concept DeclaresOnDestroyWithWorld =
    requires {
        { &T::OnDestroy } -> std::same_as<void (T::*)(IWorld&)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void OnDestroy()`.
 */
template<typename T>
concept DeclaresOnDestroyNoWorld =
    requires {
        { &T::OnDestroy } -> std::same_as<void (T::*)()>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `OnDestroy` signature.
 */
template<typename T>
concept HasRuntimeOnDestroyPhase = DeclaresOnDestroyWithWorld<T> || DeclaresOnDestroyNoWorld<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void PreTick(IWorld&, float)`.
 */
template<typename T>
concept DeclaresPreTickWithWorld =
    requires {
        { &T::PreTick } -> std::same_as<void (T::*)(IWorld&, float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void PreTick(float)`.
 */
template<typename T>
concept DeclaresPreTickNoWorld =
    requires {
        { &T::PreTick } -> std::same_as<void (T::*)(float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `PreTick` signature.
 */
template<typename T>
concept HasRuntimePreTickPhase = DeclaresPreTickWithWorld<T> || DeclaresPreTickNoWorld<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void Tick(IWorld&, float)`.
 */
template<typename T>
concept DeclaresTickWithWorld =
    requires {
        { &T::Tick } -> std::same_as<void (T::*)(IWorld&, float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void Tick(float)`.
 */
template<typename T>
concept DeclaresTickNoWorld =
    requires {
        { &T::Tick } -> std::same_as<void (T::*)(float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `Tick` signature.
 */
template<typename T>
concept HasRuntimeTickPhase = DeclaresTickWithWorld<T> || DeclaresTickNoWorld<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void FixedTick(IWorld&, float)`.
 */
template<typename T>
concept DeclaresFixedTickWithWorld =
    requires {
        { &T::FixedTick } -> std::same_as<void (T::*)(IWorld&, float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void FixedTick(float)`.
 */
template<typename T>
concept DeclaresFixedTickNoWorld =
    requires {
        { &T::FixedTick } -> std::same_as<void (T::*)(float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `FixedTick` signature.
 */
template<typename T>
concept HasRuntimeFixedTickPhase = DeclaresFixedTickWithWorld<T> || DeclaresFixedTickNoWorld<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void LateTick(IWorld&, float)`.
 */
template<typename T>
concept DeclaresLateTickWithWorld =
    requires {
        { &T::LateTick } -> std::same_as<void (T::*)(IWorld&, float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void LateTick(float)`.
 */
template<typename T>
concept DeclaresLateTickNoWorld =
    requires {
        { &T::LateTick } -> std::same_as<void (T::*)(float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `LateTick` signature.
 */
template<typename T>
concept HasRuntimeLateTickPhase = DeclaresLateTickWithWorld<T> || DeclaresLateTickNoWorld<T>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void PostTick(IWorld&, float)`.
 */
template<typename T>
concept DeclaresPostTickWithWorld =
    requires {
        { &T::PostTick } -> std::same_as<void (T::*)(IWorld&, float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void PostTick(float)`.
 */
template<typename T>
concept DeclaresPostTickNoWorld =
    requires {
        { &T::PostTick } -> std::same_as<void (T::*)(float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `PostTick` signature.
 */
template<typename T>
concept HasRuntimePostTickPhase = DeclaresPostTickWithWorld<T> || DeclaresPostTickNoWorld<T>;

#if defined(WITH_EDITOR) && WITH_EDITOR
/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void EditorTick(IWorld&, float)`.
 */
template<typename T>
concept DeclaresEditorTickWithWorld =
    requires {
        { &T::EditorTick } -> std::same_as<void (T::*)(IWorld&, float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` declares `void EditorTick(float)`.
 */
template<typename T>
concept DeclaresEditorTickNoWorld =
    requires {
        { &T::EditorTick } -> std::same_as<void (T::*)(float)>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief `true` when `T` exposes any supported `EditorTick` signature.
 */
template<typename T>
concept HasRuntimeEditorTickPhase = DeclaresEditorTickWithWorld<T> || DeclaresEditorTickNoWorld<T>;
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime tick phases that can carry independent class-level priorities.
 */
enum class ERuntimeTickPhase : std::uint8_t
{
    PreTick,
    Tick,
    FixedTick,
    LateTick,
    PostTick,
#if defined(WITH_EDITOR) && WITH_EDITOR
    EditorTick,
#endif
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Read a runtime type's compile-time tick priority.
 * @tparam TObject Runtime object type.
 * @return `TObject::kTickPriority` when present, otherwise `0`.
 *
 * Lower values execute earlier because `WorldEcsRuntime` sorts tick entries in ascending
 * priority order.
 */
template<typename TObject, ERuntimeTickPhase Phase>
consteval int RuntimePhasePriority()
{
    if constexpr (Phase == ERuntimeTickPhase::PreTick)
    {
        if constexpr (requires { TObject::kPreTickPriority; })
        {
            return static_cast<int>(TObject::kPreTickPriority);
        }
    }
    else if constexpr (Phase == ERuntimeTickPhase::Tick)
    {
        if constexpr (requires { TObject::kTickPriority; })
        {
            return static_cast<int>(TObject::kTickPriority);
        }
    }
    else if constexpr (Phase == ERuntimeTickPhase::FixedTick)
    {
        if constexpr (requires { TObject::kFixedTickPriority; })
        {
            return static_cast<int>(TObject::kFixedTickPriority);
        }
    }
    else if constexpr (Phase == ERuntimeTickPhase::LateTick)
    {
        if constexpr (requires { TObject::kLateTickPriority; })
        {
            return static_cast<int>(TObject::kLateTickPriority);
        }
    }
    else if constexpr (Phase == ERuntimeTickPhase::PostTick)
    {
        if constexpr (requires { TObject::kPostTickPriority; })
        {
            return static_cast<int>(TObject::kPostTickPriority);
        }
    }
#if defined(WITH_EDITOR) && WITH_EDITOR
    else if constexpr (Phase == ERuntimeTickPhase::EditorTick)
    {
        if constexpr (requires { TObject::kEditorTickPriority; })
        {
            return static_cast<int>(TObject::kEditorTickPriority);
        }
    }
#endif

    if constexpr (requires { TObject::kTickPriority; })
    {
        return static_cast<int>(TObject::kTickPriority);
    }

    return 0;
}

template<typename TObject>
consteval int RuntimeTickPriority()
{
    return RuntimePhasePriority<TObject, ERuntimeTickPhase::Tick>();
}

template<typename TObject>
consteval std::size_t TStoragePageSize()
{
    if constexpr (requires { TObject::kStoragePageSize; })
    {
        return static_cast<std::size_t>(TObject::kStoragePageSize);
    }
    else
    {
        return 1024u;
    }
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generation-safe handle used by dense ECS runtime storages.
 * @tparam TObject Runtime object type.
 *
 * `TDenseRuntimeHandle` is the runtime-only equivalent of an engine handle:
 * - `Id` provides stable identity across serialization-like boundaries
 * - `StorageToken` identifies the owning dense storage instance
 * - `Index` addresses the current slot inside that storage
 * - `Generation` rejects stale handles after slot reuse
 *
 * Ownership and lifetime:
 * - The handle is a value type and owns no object memory.
 * - A handle remains valid only while the target slot is alive and its generation matches.
 */
template<typename TObject>
struct TDenseRuntimeHandle
{
    static constexpr uint32_t kInvalidStorageToken = 0;
    static constexpr uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

    Uuid Id{};
    uint32_t StorageToken = kInvalidStorageToken;
    uint32_t Index = kInvalidIndex;
    uint32_t Generation = 0;

    /** @brief Return `true` when the handle carries no UUID identity. */
    bool IsNull() const noexcept
    {
        return Id.is_nil();
    }

    /** @brief Return `true` when the handle contains a storage token plus slot index. */
    bool HasRuntimeKey() const noexcept
    {
        return StorageToken != kInvalidStorageToken && Index != kInvalidIndex;
    }

    /** @brief Boolean test for non-null handle identity. */
    explicit operator bool() const noexcept
    {
        return !IsNull();
    }

    bool operator==(const TDenseRuntimeHandle&) const noexcept = default;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Dense, generation-safe storage for one runtime object type.
 * @tparam TObject Non-polymorphic runtime object type.
 *
 * `TDenseRuntimeStorage` is the hot-path container behind the ECS refactor. Objects are
 * stored in fixed-capacity pages so addresses remain stable for the lifetime of each
 * object, while stable identity is tracked through a slot table plus generation-safe handles.
 *
 * Core semantics:
 * - Runtime addresses remain stable until the object is destroyed.
 * - Create never relocates existing objects; new pages are allocated instead.
 * - Iteration order is unstable and may change on destroy because the active-runtime-index list
 *   uses swap-pop compaction.
 * - UUID identity is unique within the storage.
 * - `OnCreate` may run immediately or be deferred via `PendingOnCreate`.
 * - `OnDestroy` runs synchronously during destroy/clear, not at a later frame boundary.
 *
 * Ownership and lifetime:
 * - The storage owns all contained `TObject` instances by value inside fixed pages.
 * - Resolved pointers are borrowed and remain valid across unrelated creates/destroys until the
 *   specific object is destroyed or the storage is cleared.
 *
 * Threading:
 * - Main-thread only.
 *
 * Performance:
 * - Handle resolution is O(1).
 * - UUID fallback resolution is O(1) average through `m_idToSlot`.
 * - Tick phases iterate linearly over the active-runtime-index list and page slots.
 */
template<RuntimeTickType TObject>
class TDenseRuntimeStorage final
{
public:
    static_assert(!std::is_polymorphic_v<TObject>,
                  "Node/component runtime types must be non-polymorphic");

    static constexpr bool kHasOnCreatePhase = HasRuntimeOnCreatePhase<TObject>;
    static constexpr bool kHasOnDestroyPhase = HasRuntimeOnDestroyPhase<TObject>;
    static constexpr bool kHasPreTickPhase = HasRuntimePreTickPhase<TObject>;
    static constexpr bool kHasTickPhase = HasRuntimeTickPhase<TObject>;
    static constexpr bool kHasFixedTickPhase = HasRuntimeFixedTickPhase<TObject>;
    static constexpr bool kHasLateTickPhase = HasRuntimeLateTickPhase<TObject>;
    static constexpr bool kHasPostTickPhase = HasRuntimePostTickPhase<TObject>;
#if defined(WITH_EDITOR) && WITH_EDITOR
    static constexpr bool kHasEditorTickPhase = HasRuntimeEditorTickPhase<TObject>;
#endif
    static constexpr uint32_t kPageSize = static_cast<uint32_t>(TStoragePageSize<TObject>());
    static_assert(kPageSize > 0u, "Dense runtime storage page size must be greater than zero");
    static_assert((kPageSize & (kPageSize - 1u)) == 0u,
                  "Dense runtime storage page size must be a power of two");
    static constexpr uint32_t kPageShift = std::countr_zero(kPageSize);
    static constexpr uint32_t kPageMask = kPageSize - 1u;

    using Handle = TDenseRuntimeHandle<TObject>;

    /**
     * @brief Construct a storage bound to a specific storage token.
     * @param StorageToken Storage identity used to validate handles. `0` is replaced with `1`.
     */
    explicit TDenseRuntimeStorage(
        const uint32_t StorageToken = 1)
        : m_storageToken(StorageToken == Handle::kInvalidStorageToken ? 1u : StorageToken)
    {
    }

    ~TDenseRuntimeStorage()
    {
        DestroyAllObjectsWithoutLifecycle();
    }

    /** @brief Get the stable token that identifies this storage instance in handles. */
    [[nodiscard]] uint32_t StorageToken() const noexcept
    {
        return m_storageToken;
    }

    /** @brief Get the current number of live runtime objects in dense storage. */
    [[nodiscard]] std::size_t Size() const noexcept
    {
        return m_activeRuntimeIndices.size()
            - std::min<std::size_t>(m_activeRuntimeIndices.size(), m_pendingDestroyCount);
    }

    /** @brief Return `true` when the storage contains no live objects. */
    [[nodiscard]] bool Empty() const noexcept
    {
        return Size() == 0u;
    }

    /**
     * @brief Create a new runtime object with a generated UUID.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param Args Constructor arguments for `TObject`.
     * @return Handle to the created object, or an error on failure.
     */
    template<typename... TArgs>
    TExpected<Handle> Create(IWorld& WorldRef, TArgs&&... Args)
    {
        return CreateWithId(WorldRef, NewUuid(), std::forward<TArgs>(Args)...);
    }

    /**
     * @brief Create a new runtime object under an explicit UUID.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param Id Stable identity for the new object.
     * @param Args Constructor arguments for `TObject`.
     * @return Handle to the created object, or an error when the UUID is invalid,
     *         duplicated, or construction fails.
     *
     * Semantics:
     * - UUID collisions fail and do not overwrite an existing object.
     * - If `OnCreate` is suppressed on the current thread, the object is marked
     *   `PendingOnCreate` and must be flushed later.
     * - On construction failure, slot allocation is rolled back.
     */
    template<typename... TArgs>
    TExpected<Handle> CreateWithId(IWorld& WorldRef, const Uuid& Id, TArgs&&... Args)
    {
        if (Id.is_nil())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime object UUID is nil"));
        }
        if (m_idToSlot.contains(Id))
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime object already exists"));
        }

        const uint32_t SlotIndex = AcquireSlot(Id);
        SlotMeta& Slot = m_slots[SlotIndex];
        TObject* Object = ObjectByRuntimeIndex(SlotIndex);

        try
        {
            std::construct_at(Object, std::forward<TArgs>(Args)...);
        }
        catch (...)
        {
            RollbackCreate(SlotIndex);
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to construct runtime object"));
        }

        Slot.Alive = true;
        Slot.ActiveIndex = static_cast<uint32_t>(m_activeRuntimeIndices.size());
        m_activeRuntimeIndices.push_back(SlotIndex);

        if constexpr (kHasOnCreatePhase)
        {
            if (!IsComponentOnCreateSuppressed())
            {
                InvokeOnCreate(*Object, WorldRef);
                Slot.PendingOnCreate = false;
            }
            else
            {
                Slot.PendingOnCreate = true;
            }
        }
        else
        {
            (void)WorldRef;
            Slot.PendingOnCreate = false;
        }
        return MakeHandle(SlotIndex);
    }

    /**
     * @brief Destroy a runtime object by handle.
     * @param WorldRef Owning world passed through to `OnDestroy` when present.
     * @param InHandle Handle to destroy.
     * @return `true` when the handle resolved and the object was destroyed.
     *
     * Destruction is immediate. Dense order may change because the last dense object is
     * swapped into the removed slot.
     */
    bool Destroy(IWorld& WorldRef, const Handle& InHandle)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex, true))
        {
            return false;
        }

        DestroyBySlot(WorldRef, SlotIndex);
        return true;
    }

    /**
     * @brief Schedule a runtime object for deferred destruction at `EndFrame()`.
     * @param InHandle Handle to destroy later.
     * @return `true` when the handle resolved to a live object.
     *
     * Public resolve APIs stop returning the object immediately after it enters
     * pending-destroy state, but already-borrowed pointers remain valid until the
     * deferred destroy flush runs.
     */
    bool DestroyLater(const Handle& InHandle)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex, true))
        {
            return false;
        }

        return MarkPendingDestroy(SlotIndex);
    }

    /**
     * @brief Destroy a runtime object by UUID fallback lookup.
     * @param WorldRef Owning world passed through to `OnDestroy`.
     * @param Id UUID to destroy.
     * @return `true` when the UUID resolved to a live object.
     */
    bool DestroySlow(IWorld& WorldRef, const Uuid& Id)
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return false;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size() || !m_slots[SlotIndex].Alive)
        {
            return false;
        }

        DestroyBySlot(WorldRef, SlotIndex);
        return true;
    }

    /**
     * @brief Schedule a runtime object for deferred destruction by UUID fallback lookup.
     * @param Id UUID to destroy later.
     * @return `true` when the UUID resolved to a live object.
     */
    bool DestroySlowLater(const Uuid& Id)
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return false;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size() || !m_slots[SlotIndex].Alive)
        {
            return false;
        }

        return MarkPendingDestroy(SlotIndex);
    }

    /**
     * @brief Resolve a handle to a borrowed mutable object pointer.
     * @param InHandle Handle to resolve.
     * @return Borrowed pointer to the live object, or `nullptr` if the handle is stale.
     */
    TObject* Resolve(const Handle& InHandle)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return nullptr;
        }
        return ObjectByRuntimeIndex(SlotIndex);
    }

    /** @brief Const overload of `Resolve(const Handle&)`. */
    const TObject* Resolve(const Handle& InHandle) const
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return nullptr;
        }
        return ObjectByRuntimeIndex(SlotIndex);
    }

    /** @brief Resolve a handle while including objects already pending destroy. */
    TObject* ResolveIncludingPendingDestroy(const Handle& InHandle)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex, true))
        {
            return nullptr;
        }
        return ObjectByRuntimeIndex(SlotIndex);
    }

    /** @brief Const overload of `ResolveIncludingPendingDestroy(const Handle&)`. */
    const TObject* ResolveIncludingPendingDestroy(const Handle& InHandle) const
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex, true))
        {
            return nullptr;
        }
        return ObjectByRuntimeIndex(SlotIndex);
    }

    /**
     * @brief Resolve a UUID to a borrowed mutable object pointer.
     * @param Id UUID to resolve.
     * @return Borrowed pointer to the live object, or `nullptr` when missing.
     * @note This is the slow path compared with handle resolution.
     */
    TObject* ResolveSlowById(const Uuid& Id)
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return nullptr;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size())
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.PendingDestroy)
        {
            return nullptr;
        }

        return ObjectByRuntimeIndex(SlotIndex);
    }

    /** @brief Const overload of `ResolveSlowById(const Uuid&)`. */
    const TObject* ResolveSlowById(const Uuid& Id) const
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return nullptr;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size())
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.PendingDestroy)
        {
            return nullptr;
        }

        return ObjectByRuntimeIndex(SlotIndex);
    }

    /**
     * @brief Rebuild a current handle from a UUID.
     * @param Id UUID to resolve.
     * @return Fresh handle for the live object, or an error when not found.
     */
    TExpected<Handle> HandleById(const Uuid& Id) const
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime object not found"));
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime slot index out of range"));
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.PendingDestroy)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime object not alive"));
        }

        return MakeHandle(SlotIndex);
    }

    template<typename Visitor>
    void ForEach(Visitor&& VisitorFn)
    {
        for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
        {
            if (!IsRuntimeIndexVisible(RuntimeIndex))
            {
                continue;
            }
            VisitorFn(MakeHandle(RuntimeIndex), *ObjectByRuntimeIndex(RuntimeIndex));
        }
    }

    template<typename Visitor>
    void ForEach(Visitor&& VisitorFn) const
    {
        for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
        {
            if (!IsRuntimeIndexVisible(RuntimeIndex))
            {
                continue;
            }
            VisitorFn(MakeHandle(RuntimeIndex), *ObjectByRuntimeIndex(RuntimeIndex));
        }
    }

    template<typename Visitor>
    void ForEachAll(Visitor&& VisitorFn)
    {
        for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
        {
            if (RuntimeIndex >= m_slots.size())
            {
                continue;
            }

            const SlotMeta& Slot = m_slots[RuntimeIndex];
            if (!Slot.Alive)
            {
                continue;
            }

            VisitorFn(MakeHandle(RuntimeIndex), *ObjectByRuntimeIndex(RuntimeIndex));
        }
    }

    template<typename Visitor>
    void ForEachAll(Visitor&& VisitorFn) const
    {
        for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
        {
            if (RuntimeIndex >= m_slots.size())
            {
                continue;
            }

            const SlotMeta& Slot = m_slots[RuntimeIndex];
            if (!Slot.Alive)
            {
                continue;
            }

            VisitorFn(MakeHandle(RuntimeIndex), *ObjectByRuntimeIndex(RuntimeIndex));
        }
    }

    /**
     * @brief Execute the storage's `PreTick` phase across all live objects.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void PreTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasPreTickPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (!IsRuntimeIndexVisible(RuntimeIndex))
                {
                    continue;
                }
                InvokePreTick(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    /**
     * @brief Execute the storage's `Tick` phase across all live objects.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void Tick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasTickPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (!IsRuntimeIndexVisible(RuntimeIndex))
                {
                    continue;
                }
                InvokeTick(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    /**
     * @brief Execute the storage's `FixedTick` phase across all live objects.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param DeltaSeconds Fixed-step delta in seconds.
     */
    void FixedTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasFixedTickPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (!IsRuntimeIndexVisible(RuntimeIndex))
                {
                    continue;
                }
                InvokeFixedTick(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    /**
     * @brief Execute the storage's `LateTick` phase across all live objects.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void LateTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasLateTickPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (!IsRuntimeIndexVisible(RuntimeIndex))
                {
                    continue;
                }
                InvokeLateTick(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    /**
     * @brief Execute the storage's `PostTick` phase across all live objects.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void PostTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasPostTickPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (!IsRuntimeIndexVisible(RuntimeIndex))
                {
                    continue;
                }
                InvokePostTick(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief Execute the storage's editor-only tick phase across all live objects.
     * @param WorldRef Owning world passed through to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void EditorTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasEditorTickPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (!IsRuntimeIndexVisible(RuntimeIndex))
                {
                    continue;
                }
                InvokeEditorTick(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }
#endif

    /**
     * @brief Invoke any deferred `OnCreate` hooks that were suppressed during creation.
     * @param WorldRef Owning world passed through to `OnCreate`.
     *
     * Ordering is slot-table order, which usually matches creation order but is not
     * documented as a stable cross-version contract.
     */
    void FlushPendingOnCreate(IWorld& WorldRef)
    {
        if constexpr (!kHasOnCreatePhase)
        {
            (void)WorldRef;
            return;
        }

        for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
        {
            if (RuntimeIndex >= m_slots.size())
            {
                continue;
            }

            SlotMeta& Slot = m_slots[RuntimeIndex];
            if (!Slot.Alive || Slot.PendingDestroy || !Slot.PendingOnCreate)
            {
                continue;
            }

            InvokeOnCreate(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef);
            Slot.PendingOnCreate = false;
        }
    }

    /**
     * @brief Flush a specific pending `OnCreate` hook by handle.
     * @param WorldRef Owning world passed through to `OnCreate`.
     * @param InHandle Handle whose pending create hook should run.
     * @return `true` when the handle resolved and the hook was invoked.
     */
    bool FlushPendingOnCreate(IWorld& WorldRef, const Handle& InHandle)
    {
        if constexpr (!kHasOnCreatePhase)
        {
            (void)WorldRef;
            (void)InHandle;
            return false;
        }

        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex, true))
        {
            return false;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.PendingOnCreate || Slot.PendingDestroy)
        {
            return false;
        }

        InvokeOnCreate(*ObjectByRuntimeIndex(SlotIndex), WorldRef);
        Slot.PendingOnCreate = false;
        return true;
    }

    /**
     * @brief Flush all deferred destroys that were scheduled earlier in the frame.
     * @param WorldRef Owning world passed through to `OnDestroy`.
     */
    void EndFrame(IWorld& WorldRef)
    {
        std::vector<uint32_t> PendingDestroy = std::move(m_pendingDestroySlots);
        m_pendingDestroySlots.clear();

        for (const uint32_t SlotIndex : PendingDestroy)
        {
            if (SlotIndex >= m_slots.size())
            {
                continue;
            }

            const SlotMeta& Slot = m_slots[SlotIndex];
            if (!Slot.Alive || !Slot.PendingDestroy)
            {
                continue;
            }

            DestroyBySlot(WorldRef, SlotIndex);
        }
    }

    /**
     * @brief Destroy all live objects immediately and reset the storage to empty.
     * @param WorldRef Owning world passed through to `OnDestroy`.
     *
     * This invalidates every outstanding handle and borrowed pointer.
     */
    void Clear(IWorld& WorldRef)
    {
        if constexpr (kHasOnDestroyPhase)
        {
            for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
            {
                if (RuntimeIndex >= m_slots.size())
                {
                    continue;
                }

                SlotMeta& Slot = m_slots[RuntimeIndex];
                if (!Slot.Alive)
                {
                    continue;
                }

                InvokeOnDestroy(*ObjectByRuntimeIndex(RuntimeIndex), WorldRef);
                std::destroy_at(ObjectByRuntimeIndex(RuntimeIndex));
            }
        }
        else
        {
            (void)WorldRef;
            DestroyAllObjectsWithoutLifecycle();
        }

        m_idToSlot.clear();
        m_freeSlotIndices.clear();
        m_pendingDestroySlots.clear();
        m_pendingDestroyCount = 0u;
        m_activeRuntimeIndices.clear();
        m_freeSlotIndices.reserve(m_slots.size());
        for (uint32_t SlotIndex = 0; SlotIndex < m_slots.size(); ++SlotIndex)
        {
            SlotMeta& Slot = m_slots[SlotIndex];
            Slot.Id = {};
            Slot.Alive = false;
            Slot.ActiveIndex = Handle::kInvalidIndex;
            Slot.PendingOnCreate = false;
            Slot.PendingDestroy = false;
            Slot.Generation = (Slot.Generation == std::numeric_limits<uint32_t>::max()) ? 1u : (Slot.Generation + 1u);
            if (Slot.Generation == 0u)
            {
                Slot.Generation = 1u;
            }
            m_freeSlotIndices.push_back(SlotIndex);
        }
    }

private:
    static void InvokeOnCreate(TObject& Object, IWorld& WorldRef)
    {
        if constexpr (DeclaresOnCreateWithWorld<TObject>)
        {
            Object.OnCreate(WorldRef);
        }
        else if constexpr (DeclaresOnCreateNoWorld<TObject>)
        {
            Object.OnCreate();
        }
        else
        {
            (void)Object;
            (void)WorldRef;
        }
    }

    static void InvokeOnDestroy(TObject& Object, IWorld& WorldRef)
    {
        if constexpr (DeclaresOnDestroyWithWorld<TObject>)
        {
            Object.OnDestroy(WorldRef);
        }
        else if constexpr (DeclaresOnDestroyNoWorld<TObject>)
        {
            Object.OnDestroy();
        }
        else
        {
            (void)Object;
            (void)WorldRef;
        }
    }

    static void InvokePreTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (DeclaresPreTickWithWorld<TObject>)
        {
            Object.PreTick(WorldRef, DeltaSeconds);
        }
        else if constexpr (DeclaresPreTickNoWorld<TObject>)
        {
            Object.PreTick(DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokeTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (DeclaresTickWithWorld<TObject>)
        {
            Object.Tick(WorldRef, DeltaSeconds);
        }
        else if constexpr (DeclaresTickNoWorld<TObject>)
        {
            Object.Tick(DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokeFixedTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (DeclaresFixedTickWithWorld<TObject>)
        {
            Object.FixedTick(WorldRef, DeltaSeconds);
        }
        else if constexpr (DeclaresFixedTickNoWorld<TObject>)
        {
            Object.FixedTick(DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokeLateTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (DeclaresLateTickWithWorld<TObject>)
        {
            Object.LateTick(WorldRef, DeltaSeconds);
        }
        else if constexpr (DeclaresLateTickNoWorld<TObject>)
        {
            Object.LateTick(DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokePostTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (DeclaresPostTickWithWorld<TObject>)
        {
            Object.PostTick(WorldRef, DeltaSeconds);
        }
        else if constexpr (DeclaresPostTickNoWorld<TObject>)
        {
            Object.PostTick(DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    static void InvokeEditorTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (DeclaresEditorTickWithWorld<TObject>)
        {
            Object.EditorTick(WorldRef, DeltaSeconds);
        }
        else if constexpr (DeclaresEditorTickNoWorld<TObject>)
        {
            Object.EditorTick(DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }
#endif

    struct SlotMeta
    {
        Uuid Id{};
        uint32_t Generation = 1;
        uint32_t ActiveIndex = Handle::kInvalidIndex;
        bool Alive = false;
        bool PendingOnCreate = false;
        bool PendingDestroy = false;
    };

    Handle MakeHandle(const uint32_t SlotIndex) const
    {
        const SlotMeta& Slot = m_slots[SlotIndex];
        return Handle{
            .Id = Slot.Id,
            .StorageToken = m_storageToken,
            .Index = SlotIndex,
            .Generation = Slot.Generation};
    }

    bool ResolveSlot(const Handle& InHandle, uint32_t& OutSlotIndex, const bool IncludePendingDestroy = false) const
    {
        if (InHandle.StorageToken != m_storageToken || InHandle.Index == Handle::kInvalidIndex)
        {
            return false;
        }
        if (InHandle.Index >= m_slots.size())
        {
            return false;
        }

        const SlotMeta& Slot = m_slots[InHandle.Index];
        if (!Slot.Alive || (!IncludePendingDestroy && Slot.PendingDestroy) || Slot.Generation != InHandle.Generation
            || Slot.Id != InHandle.Id)
        {
            return false;
        }

        OutSlotIndex = InHandle.Index;
        return true;
    }

    uint32_t AcquireSlot(const Uuid& Id)
    {
        if (m_freeSlotIndices.empty())
        {
            AllocatePage();
        }

        const uint32_t SlotIndex = m_freeSlotIndices.back();
        m_freeSlotIndices.pop_back();

        SlotMeta& Slot = m_slots[SlotIndex];
        Slot.Id = Id;
        Slot.Alive = false;
        Slot.ActiveIndex = Handle::kInvalidIndex;
        Slot.PendingOnCreate = false;
        Slot.PendingDestroy = false;
        if (Slot.Generation == 0u)
        {
            Slot.Generation = 1u;
        }
        m_idToSlot[Id] = SlotIndex;
        return SlotIndex;
    }

    void RollbackCreate(const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_slots.size())
        {
            return;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        m_idToSlot.erase(Slot.Id);
        Slot.Id = {};
        Slot.ActiveIndex = Handle::kInvalidIndex;
        Slot.Alive = false;
        Slot.PendingOnCreate = false;
        Slot.PendingDestroy = false;
        m_freeSlotIndices.push_back(SlotIndex);
    }

    bool MarkPendingDestroy(const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_slots.size())
        {
            return false;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive)
        {
            return false;
        }
        if (Slot.PendingDestroy)
        {
            return true;
        }

        Slot.PendingDestroy = true;
        Slot.PendingOnCreate = false;
        m_pendingDestroySlots.push_back(SlotIndex);
        ++m_pendingDestroyCount;
        return true;
    }

    bool IsRuntimeIndexVisible(const uint32_t RuntimeIndex) const
    {
        if (RuntimeIndex >= m_slots.size())
        {
            return false;
        }

        const SlotMeta& Slot = m_slots[RuntimeIndex];
        return Slot.Alive && !Slot.PendingDestroy;
    }

    void DestroyBySlot(IWorld& WorldRef, const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_slots.size())
        {
            return;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive)
        {
            return;
        }

        if (Slot.PendingDestroy && m_pendingDestroyCount > 0u)
        {
            --m_pendingDestroyCount;
        }

        TObject* Object = ObjectByRuntimeIndex(SlotIndex);

        if constexpr (kHasOnDestroyPhase)
        {
            InvokeOnDestroy(*Object, WorldRef);
        }
        else
        {
            (void)WorldRef;
        }

        std::destroy_at(Object);

        if (Slot.ActiveIndex != Handle::kInvalidIndex && !m_activeRuntimeIndices.empty())
        {
            const uint32_t ActiveIndex = Slot.ActiveIndex;
            const uint32_t LastRuntimeIndex = m_activeRuntimeIndices.back();
            if (ActiveIndex + 1u != m_activeRuntimeIndices.size())
            {
                m_activeRuntimeIndices[ActiveIndex] = LastRuntimeIndex;
                m_slots[LastRuntimeIndex].ActiveIndex = ActiveIndex;
            }
            m_activeRuntimeIndices.pop_back();
        }

        m_idToSlot.erase(Slot.Id);
        Slot.Id = {};
        Slot.Alive = false;
        Slot.ActiveIndex = Handle::kInvalidIndex;
        Slot.PendingOnCreate = false;
        Slot.PendingDestroy = false;
        Slot.Generation = (Slot.Generation == std::numeric_limits<uint32_t>::max()) ? 1u : (Slot.Generation + 1u);
        if (Slot.Generation == 0u)
        {
            Slot.Generation = 1u;
        }
        m_freeSlotIndices.push_back(SlotIndex);
    }

    struct Page
    {
        Page()
            : Objects(std::allocator<TObject>{}.allocate(kPageSize))
        {
        }

        ~Page()
        {
            if (Objects)
            {
                std::allocator<TObject>{}.deallocate(Objects, kPageSize);
            }
        }

        TObject* Objects = nullptr;
    };

    static constexpr uint32_t PageIndexFromRuntimeIndex(const uint32_t RuntimeIndex) noexcept
    {
        return RuntimeIndex >> kPageShift;
    }

    static constexpr uint32_t SlotOffsetFromRuntimeIndex(const uint32_t RuntimeIndex) noexcept
    {
        return RuntimeIndex & kPageMask;
    }

    static constexpr uint32_t PackRuntimeIndex(const uint32_t PageIndex, const uint32_t SlotOffset) noexcept
    {
        return (PageIndex << kPageShift) | SlotOffset;
    }

    TObject* ObjectByRuntimeIndex(const uint32_t RuntimeIndex)
    {
        const uint32_t PageIndex = PageIndexFromRuntimeIndex(RuntimeIndex);
        const uint32_t SlotOffset = SlotOffsetFromRuntimeIndex(RuntimeIndex);
        return m_pages[PageIndex]->Objects + SlotOffset;
    }

    const TObject* ObjectByRuntimeIndex(const uint32_t RuntimeIndex) const
    {
        const uint32_t PageIndex = PageIndexFromRuntimeIndex(RuntimeIndex);
        const uint32_t SlotOffset = SlotOffsetFromRuntimeIndex(RuntimeIndex);
        return m_pages[PageIndex]->Objects + SlotOffset;
    }

    void AllocatePage()
    {
        const uint32_t PageIndex = static_cast<uint32_t>(m_pages.size());
        auto NewPage = std::make_unique<Page>();
        m_pages.push_back(NewPage.get());
        m_pageOwners.push_back(std::move(NewPage));

        const std::size_t OldSlotCount = m_slots.size();
        m_slots.resize(OldSlotCount + kPageSize);
        m_freeSlotIndices.reserve(m_freeSlotIndices.size() + kPageSize);
        for (uint32_t SlotOffset = kPageSize; SlotOffset > 0u; --SlotOffset)
        {
            m_freeSlotIndices.push_back(PackRuntimeIndex(PageIndex, SlotOffset - 1u));
        }
    }

    void DestroyAllObjectsWithoutLifecycle()
    {
        for (const uint32_t RuntimeIndex : m_activeRuntimeIndices)
        {
            if (RuntimeIndex >= m_slots.size())
            {
                continue;
            }

            const SlotMeta& Slot = m_slots[RuntimeIndex];
            if (!Slot.Alive)
            {
                continue;
            }

            std::destroy_at(ObjectByRuntimeIndex(RuntimeIndex));
        }
        m_activeRuntimeIndices.clear();
    }

    uint32_t m_storageToken = 1;
    std::vector<Page*> m_pages{};
    std::vector<std::unique_ptr<Page>> m_pageOwners{};
    std::vector<SlotMeta> m_slots{};
    std::vector<uint32_t> m_activeRuntimeIndices{};
    std::vector<uint32_t> m_freeSlotIndices{};
    std::vector<uint32_t> m_pendingDestroySlots{};
    std::size_t m_pendingDestroyCount = 0u;
    std::unordered_map<Uuid, uint32_t, UuidHash> m_idToSlot{};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Minimal marker record used to type-erase runtime component handles.
 */
struct RuntimeComponentRecord final
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RuntimeComponentRecord";
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Type-erased dense handle used for runtime components attached to nodes.
 */
using RuntimeComponentHandle = TDenseRuntimeHandle<RuntimeComponentRecord>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a typed dense runtime handle into the generic runtime-component handle form.
 * @tparam TObject Concrete runtime component type.
 * @param InHandle Typed handle to convert.
 * @return Handle with identical identity fields but erased component type.
 */
template<typename TObject>
RuntimeComponentHandle ToRuntimeComponentHandle(const TDenseRuntimeHandle<TObject>& InHandle)
{
    return RuntimeComponentHandle{
        .Id = InHandle.Id,
        .StorageToken = InHandle.StorageToken,
        .Index = InHandle.Index,
        .Generation = InHandle.Generation};
}

template<typename TObject>
NodeHandle ToNodeHandle(const TDenseRuntimeHandle<TObject>& InHandle)
{
    return NodeHandle{
        InHandle.Id,
        InHandle.StorageToken,
        InHandle.Index,
        InHandle.Generation};
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reinterpret a generic runtime-component handle as a typed dense handle.
 * @tparam TObject Concrete runtime component type expected by the caller.
 * @param InHandle Generic runtime-component handle.
 * @return Typed handle carrying the same identity fields.
 *
 * This conversion does not validate that the handle really points at storage for
 * `TObject`. Callers are expected to pair it with a known component type.
 */
template<typename TObject>
TDenseRuntimeHandle<TObject> ToTypedRuntimeHandle(const RuntimeComponentHandle& InHandle)
{
    return TDenseRuntimeHandle<TObject>{
        .Id = InHandle.Id,
        .StorageToken = InHandle.StorageToken,
        .Index = InHandle.Index,
        .Generation = InHandle.Generation};
}

template<typename TObject>
TDenseRuntimeHandle<TObject> ToTypedNodeRuntimeHandle(const NodeHandle& InHandle)
{
    return TDenseRuntimeHandle<TObject>{
        .Id = InHandle.Id,
        .StorageToken = InHandle.RuntimePoolToken,
        .Index = InHandle.RuntimeIndex,
        .Generation = InHandle.RuntimeGeneration};
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned orchestration layer for typed runtime component storages.
 *
 * `WorldEcsRuntime` is the top-level container that ties together:
 * - lazily created `TDenseRuntimeStorage<T>` instances for concrete runtime types
 * - one-component-per-type attachments from real node handles to runtime components
 * - globally ordered tick dispatch by compile-time priority
 *
 * Core semantics:
 * - Typed `Storage<T>()` creation is lazy and also registers tick dispatch for `T` when
 *   it exposes any runtime tick phase.
 * - Tick order is phase-specific and uses ascending compile-time priorities per phase.
 * - The typed `AddComponent<T>()` path can create storage on demand.
 * - The dynamic `AddComponent(TypeId)` path only works for types whose storage model has
 *   already been created.
 * - `FlushPendingOnCreate()` iterates an `unordered_map`, so inter-type flush order is
 *   intentionally unspecified.
 *
 * Threading:
 * - Main-thread only.
 */
class WorldEcsRuntime final
{
    class INodeStorageModel;
    template<RuntimeTickType TObject>
    class TNodeStorageModel;
    class IStorageModel;
    template<RuntimeTickType TObject>
    class TStorageModel;

public:
    /**
     * @brief Minimal cold-path interface for type-erased runtime storages.
     *
     * This interface exists for reflection, serialization, and dynamic component APIs.
     * Hot-path ticking continues to operate through typed storage pointers.
     */
    class IErasedStorage
    {
    public:
        /** @brief Virtual destructor. */
        virtual ~IErasedStorage() = default;
        /** @brief Get the reflected type stored by this erased storage. */
        [[nodiscard]] virtual TypeId Type() const = 0;
        /** @brief Get the current live object count. */
        [[nodiscard]] virtual std::size_t Size() const = 0;
        /** @brief Resolve an object by UUID to a borrowed mutable pointer. */
        [[nodiscard]] virtual void* ResolveRaw(const Uuid& Id) = 0;
        /** @brief Resolve an object by UUID to a borrowed const pointer. */
        [[nodiscard]] virtual const void* ResolveRaw(const Uuid& Id) const = 0;
        /** @brief Destroy an object by UUID. */
        virtual bool DestroyById(IWorld& WorldRef, const Uuid& Id) = 0;
    };

    class IErasedNodeStorage
    {
    public:
        virtual ~IErasedNodeStorage() = default;
        [[nodiscard]] virtual TypeId Type() const = 0;
        [[nodiscard]] virtual std::size_t Size() const = 0;
        [[nodiscard]] virtual BaseNode* ResolveRaw(const Uuid& Id) = 0;
        [[nodiscard]] virtual const BaseNode* ResolveRaw(const Uuid& Id) const = 0;
    };

    template<typename TNode>
    requires (std::is_base_of_v<BaseNode, TNode> && RuntimeTickType<TNode>)
    TDenseRuntimeStorage<TNode>& NodeStorage()
    {
        const TypeId& Type = StaticTypeId<TNode>();
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            auto* Model = static_cast<TNodeStorageModel<TNode>*>(It->second.get());
            return Model->TypedStorage;
        }

        const uint32_t StorageToken = AcquireStorageToken();
        auto Model = std::make_unique<TNodeStorageModel<TNode>>(StorageToken);
        auto* ModelPtr = Model.get();
        auto* TypedStorage = &Model->TypedStorage;

        RegisterTickEntry<TNode>(TypedStorage);
        m_nodeStorages.emplace(Type, std::move(Model));
        m_nodeStorageByToken[ModelPtr->StorageToken()] = ModelPtr;
        return *TypedStorage;
    }

    template<typename TNode>
    requires (std::is_base_of_v<BaseNode, TNode> && RuntimeTickType<TNode>)
    TDenseRuntimeStorage<TNode>* FindNodeStorage()
    {
        const TypeId& Type = StaticTypeId<TNode>();
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            auto* Model = static_cast<TNodeStorageModel<TNode>*>(It->second.get());
            return &Model->TypedStorage;
        }
        return nullptr;
    }

    template<typename TNode>
    requires (std::is_base_of_v<BaseNode, TNode> && RuntimeTickType<TNode>)
    const TDenseRuntimeStorage<TNode>* FindNodeStorage() const
    {
        const TypeId& Type = StaticTypeId<TNode>();
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            const auto* Model = static_cast<const TNodeStorageModel<TNode>*>(It->second.get());
            return &Model->TypedStorage;
        }
        return nullptr;
    }

    [[nodiscard]] IErasedNodeStorage* FindErasedNodeStorage(const TypeId& Type)
    {
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] const IErasedNodeStorage* FindErasedNodeStorage(const TypeId& Type) const
    {
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] TExpected<NodeHandle> CreateNode(IWorld& WorldRef,
                                                   const TypeId& Type,
                                                   std::string Name,
                                                   const Uuid* ExplicitId = nullptr)
    {
        INodeStorageModel* StorageModel = FindNodeStorageModel(Type);
        if (!StorageModel)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node storage was not registered"));
        }
        return StorageModel->CreateDefault(WorldRef, std::move(Name), ExplicitId);
    }

    [[nodiscard]] BaseNode* ResolveNode(const NodeHandle& Handle)
    {
        if (Handle.IsNull() || !Handle.HasRuntimeKey())
        {
            return nullptr;
        }

        if (INodeStorageModel* StorageModel = FindNodeStorageModelByToken(Handle.RuntimePoolToken))
        {
            return StorageModel->ResolveRawByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    [[nodiscard]] const BaseNode* ResolveNode(const NodeHandle& Handle) const
    {
        if (Handle.IsNull() || !Handle.HasRuntimeKey())
        {
            return nullptr;
        }

        if (const INodeStorageModel* StorageModel = FindNodeStorageModelByToken(Handle.RuntimePoolToken))
        {
            return StorageModel->ResolveRawByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    [[nodiscard]] BaseNode* ResolveNodeIncludingPendingDestroy(const NodeHandle& Handle)
    {
        if (Handle.IsNull() || !Handle.HasRuntimeKey())
        {
            return nullptr;
        }

        if (INodeStorageModel* StorageModel = FindNodeStorageModelByToken(Handle.RuntimePoolToken))
        {
            return StorageModel->ResolveIncludingPendingDestroyByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    [[nodiscard]] const BaseNode* ResolveNodeIncludingPendingDestroy(const NodeHandle& Handle) const
    {
        if (Handle.IsNull() || !Handle.HasRuntimeKey())
        {
            return nullptr;
        }

        if (const INodeStorageModel* StorageModel = FindNodeStorageModelByToken(Handle.RuntimePoolToken))
        {
            return StorageModel->ResolveIncludingPendingDestroyByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    [[nodiscard]] TExpected<NodeHandle> NodeHandleById(const Uuid& Id) const
    {
        if (Id.is_nil())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node UUID is nil"));
        }

        for (const auto& [_, StorageModel] : m_nodeStorages)
        {
            if (!StorageModel)
            {
                continue;
            }

            if (auto HandleResult = StorageModel->HandleById(Id); HandleResult)
            {
                return *HandleResult;
            }
        }
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
    }

    [[nodiscard]] bool DestroyNodeLater(const NodeHandle& Handle)
    {
        if (Handle.IsNull() || !Handle.HasRuntimeKey())
        {
            return false;
        }

        if (INodeStorageModel* StorageModel = FindNodeStorageModelByToken(Handle.RuntimePoolToken))
        {
            return StorageModel->DestroyLaterByRuntimeHandle(Handle);
        }
        return false;
    }

    void FlushPendingNodeOnCreate(IWorld& WorldRef)
    {
        for (auto& [_, StorageModel] : m_nodeStorages)
        {
            if (StorageModel)
            {
                StorageModel->FlushPendingOnCreate(WorldRef);
            }
        }
    }

    [[nodiscard]] bool FlushPendingNodeOnCreate(IWorld& WorldRef, const NodeHandle& Handle)
    {
        if (Handle.IsNull() || !Handle.HasRuntimeKey())
        {
            return false;
        }

        if (INodeStorageModel* StorageModel = FindNodeStorageModelByToken(Handle.RuntimePoolToken))
        {
            return StorageModel->FlushPendingOnCreate(WorldRef, Handle);
        }
        return false;
    }

    void ForEachNode(void (*Visitor)(void*, const NodeHandle&, BaseNode&), void* UserData)
    {
        if (!Visitor)
        {
            return;
        }

        for (auto& [_, StorageModel] : m_nodeStorages)
        {
            if (StorageModel)
            {
                StorageModel->ForEachNode(Visitor, UserData);
            }
        }
    }

    /**
     * @brief Get or lazily create the typed storage for `TObject`.
     * @tparam TObject Runtime object type.
     * @return Reference to the owned typed storage.
     *
     * The first call also:
     * - acquires a unique storage token
     * - creates the erased storage model
     * - registers tick dispatch when the type exposes any runtime tick phase
     */
    template<RuntimeTickType TObject>
    TDenseRuntimeStorage<TObject>& Storage()
    {
        const TypeId& Type = StaticTypeId<TObject>();
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            auto* Model = static_cast<TStorageModel<TObject>*>(It->second.get());
            return Model->TypedStorage;
        }

        const uint32_t StorageToken = AcquireStorageToken();
        auto Model = std::make_unique<TStorageModel<TObject>>(StorageToken);
        auto* ModelPtr = Model.get();
        auto* TypedStorage = &Model->TypedStorage;

        RegisterTickEntry<TObject>(TypedStorage);
        m_storages.emplace(Type, std::move(Model));
        m_storageByToken[ModelPtr->StorageToken()] = ModelPtr;
        return *TypedStorage;
    }

    /**
     * @brief Find an existing typed storage without creating one.
     * @tparam TObject Runtime object type.
     * @return Pointer to the storage, or `nullptr` when no storage has been created yet.
     */
    template<RuntimeTickType TObject>
    TDenseRuntimeStorage<TObject>* FindStorage()
    {
        const TypeId& Type = StaticTypeId<TObject>();
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            auto* Model = static_cast<TStorageModel<TObject>*>(It->second.get());
            return &Model->TypedStorage;
        }
        return nullptr;
    }

    /** @brief Const overload of `FindStorage<TObject>()`. */
    template<RuntimeTickType TObject>
    const TDenseRuntimeStorage<TObject>* FindStorage() const
    {
        const TypeId& Type = StaticTypeId<TObject>();
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            const auto* Model = static_cast<const TStorageModel<TObject>*>(It->second.get());
            return &Model->TypedStorage;
        }
        return nullptr;
    }

    /** @brief Find an existing erased storage by reflected type id. */
    [[nodiscard]] IErasedStorage* FindErased(const TypeId& Type)
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    /** @brief Const overload of `FindErased(const TypeId&)`. */
    [[nodiscard]] const IErasedStorage* FindErased(const TypeId& Type) const
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    /** @brief Access the world-owned runtime node hierarchy. */
    /**
     * @brief Create and attach a typed runtime component to a node.
     * @tparam TObject Runtime component type.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     * @param Args Constructor arguments for `TObject`.
     * @return Typed runtime-component handle, or an error when the node is invalid or
     *         already owns that component type.
     */
    template<RuntimeTickType TObject, typename... TArgs>
    TExpected<TDenseRuntimeHandle<TObject>> AddComponent(IWorld& WorldRef,
                                                         const NodeHandle& Owner,
                                                         TArgs&&... Args)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }
        if (FindNodeComponentIndex(*Attachment, Type).has_value())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime component already exists on node"));
        }

        auto& TypedStorage = Storage<TObject>();
        auto CreateResult = TypedStorage.Create(WorldRef, std::forward<TArgs>(Args)...);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        Attachment->Components.push_back(NodeComponentLink{
            .Type = Type,
            .Handle = ToRuntimeComponentHandle(*CreateResult)});
        return *CreateResult;
    }

    /**
     * @brief Create and attach a typed runtime component under an explicit UUID.
     * @tparam TObject Runtime component type.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     * @param Id Stable component identity.
     * @param Args Constructor arguments for `TObject`.
     * @return Typed runtime-component handle, or an error on failure.
     */
    template<RuntimeTickType TObject, typename... TArgs>
    TExpected<TDenseRuntimeHandle<TObject>> AddComponentWithId(IWorld& WorldRef,
                                                               const NodeHandle& Owner,
                                                               const Uuid& Id,
                                                               TArgs&&... Args)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }
        if (FindNodeComponentIndex(*Attachment, Type).has_value())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime component already exists on node"));
        }

        auto& TypedStorage = Storage<TObject>();
        auto CreateResult = TypedStorage.CreateWithId(WorldRef, Id, std::forward<TArgs>(Args)...);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        Attachment->Components.push_back(NodeComponentLink{
            .Type = Type,
            .Handle = ToRuntimeComponentHandle(*CreateResult)});
        return *CreateResult;
    }

    /** @brief Resolve a typed runtime component attached to a node. */
    template<RuntimeTickType TObject>
    TObject* Component(const NodeHandle& Owner)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return nullptr;
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return nullptr;
        }

        auto* TypedStorage = FindStorage<TObject>();
        if (!TypedStorage)
        {
            return nullptr;
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        return TypedStorage->Resolve(ToTypedRuntimeHandle<TObject>(GenericHandle));
    }

    /** @brief Const overload of `Component<TObject>(...)`. */
    template<RuntimeTickType TObject>
    const TObject* Component(const NodeHandle& Owner) const
    {
        const TypeId& Type = StaticTypeId<TObject>();
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return nullptr;
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return nullptr;
        }

        const auto* TypedStorage = FindStorage<TObject>();
        if (!TypedStorage)
        {
            return nullptr;
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        return TypedStorage->Resolve(ToTypedRuntimeHandle<TObject>(GenericHandle));
    }

    /**
     * @brief Remove a typed runtime component from a node.
     * @tparam TObject Runtime component type.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     * @return `true` when the underlying typed storage destroyed the component.
     *
     * @warning Current behavior removes the node-to-component attachment record once it
     *          is found, even if the typed storage destroy path reports failure.
     */
    template<RuntimeTickType TObject>
    bool RemoveComponent(IWorld& WorldRef, const NodeHandle& Owner)
    {
        (void)WorldRef;
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return false;
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return false;
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        bool Destroyed = false;
        if (auto* TypedStorage = FindStorage<TObject>())
        {
            Destroyed = TypedStorage->DestroyLater(ToTypedRuntimeHandle<TObject>(GenericHandle));
        }
        RemoveNodeComponentAt(*Attachment, *LinkIndex);
        return Destroyed;
    }

    /**
     * @brief Dynamically create and attach a runtime component by reflected type id.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     * @param Type Reflected component type.
     * @return Generic runtime-component handle, or an error on failure.
     *
     * Unlike the typed overload, this path does not create a storage model implicitly.
     * The target storage must already exist, usually because `Storage<T>()` was created
     * earlier for that runtime type.
     */
    TExpected<RuntimeComponentHandle> AddComponent(IWorld& WorldRef,
                                                   const NodeHandle& Owner,
                                                   const TypeId& Type)
    {
        return AddComponentWithId(WorldRef, Owner, Type, {});
    }

    /**
     * @brief Dynamic overload of `AddComponent(...)` with explicit component UUID.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     * @param Type Reflected component type.
     * @param Id Explicit component UUID. A nil UUID requests auto-generation.
     * @return Generic runtime-component handle, or an error on failure.
     */
    TExpected<RuntimeComponentHandle> AddComponentWithId(IWorld& WorldRef,
                                                         const NodeHandle& Owner,
                                                         const TypeId& Type,
                                                         const Uuid& Id)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime component type is null"));
        }

        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }
        if (FindNodeComponentIndex(*Attachment, Type).has_value())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime component already exists on node"));
        }

        IStorageModel* StorageModel = FindStorageModel(Type);
        if (!StorageModel)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime storage for component type not found"));
        }

        auto CreateResult = StorageModel->CreateDefault(WorldRef, Id.is_nil() ? nullptr : &Id);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        Attachment->Components.push_back(NodeComponentLink{
            .Type = Type,
            .Handle = *CreateResult});
        return *CreateResult;
    }

    /**
     * @brief Remove a dynamically addressed runtime component from a node.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     * @param Type Reflected component type.
     * @return Success when the backing storage destroy path succeeded, otherwise an error.
     *
     * @warning As with the typed overload, the node attachment record is removed even if
     *          the underlying storage destroy path fails.
     */
    Result RemoveComponent(IWorld& WorldRef,
                           const NodeHandle& Owner,
                           const TypeId& Type)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime component type is null"));
        }

        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found on node"));
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        bool Destroyed = false;
        if (IStorageModel* StorageModel = FindStorageModelByToken(GenericHandle.StorageToken))
        {
            Destroyed = StorageModel->DestroyLaterByRuntimeHandle(GenericHandle);
        }
        else if (IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            Destroyed = TypeStorageModel->DestroyLaterByRuntimeHandle(GenericHandle);
        }

        RemoveNodeComponentAt(*Attachment, *LinkIndex);
        return Destroyed ? Ok() : std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found"));
    }

    /** @brief Return `true` when a node currently owns a component of the given reflected type. */
    [[nodiscard]] bool HasComponent(const NodeHandle& Owner, const TypeId& Type) const
    {
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        return Attachment && FindNodeComponentIndex(*Attachment, Type).has_value();
    }

    /**
     * @brief Fetch the generic runtime-component handle attached to a node for a specific reflected type.
     * @param Owner Owner node handle.
     * @param Type Reflected component type.
     * @return Generic runtime-component handle, or an error when the attachment is absent.
     */
    [[nodiscard]] TExpected<RuntimeComponentHandle> ComponentHandle(const NodeHandle& Owner,
                                                                    const TypeId& Type) const
    {
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found on node"));
        }

        return Attachment->Components[*LinkIndex].Handle;
    }

    /**
     * @brief Resolve a generic runtime-component handle to a raw mutable pointer.
     * @param Handle Generic runtime-component handle.
     * @param Type Reflected component type expected by the caller.
     * @return Borrowed pointer to the live component, or `nullptr` when the handle/type
     *         pair does not resolve.
     */
    [[nodiscard]] void* ResolveComponentRaw(const RuntimeComponentHandle Handle, const TypeId& Type)
    {
        if (Handle.IsNull() || Type == TypeId{})
        {
            return nullptr;
        }

        if (IStorageModel* StorageModel = FindStorageModelByToken(Handle.StorageToken))
        {
            if (StorageModel->Type() == Type)
            {
                return StorageModel->ResolveRawByRuntimeHandle(Handle);
            }
        }
        if (IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            return TypeStorageModel->ResolveRawByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    /** @brief Const overload of `ResolveComponentRaw(...)`. */
    [[nodiscard]] const void* ResolveComponentRaw(const RuntimeComponentHandle Handle, const TypeId& Type) const
    {
        if (Handle.IsNull() || Type == TypeId{})
        {
            return nullptr;
        }

        if (const IStorageModel* StorageModel = FindStorageModelByToken(Handle.StorageToken))
        {
            if (StorageModel->Type() == Type)
            {
                return StorageModel->ResolveRawByRuntimeHandle(Handle);
            }
        }
        if (const IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            return TypeStorageModel->ResolveRawByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    /**
     * @brief Schedule destruction of all runtime components attached to a node.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Owner node handle.
     */
    void DestroyComponentsOnNode(IWorld& WorldRef, const NodeHandle& Owner)
    {
        RemoveAllComponentsOnNode(WorldRef, Owner);
        ClearNodeAttachment(Owner);
    }

    /**
     * @brief Execute variable-step runtime phases across all registered storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     *
     * Per-storage execution order is ascending runtime tick priority, then storage
     * creation order for ties. Within a storage, phases execute as `PreTick`, `Tick`,
     * `PostTick`.
     */
    void Tick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const PhaseEntry& Entry : m_preTickEntries)
        {
            Entry.Invoke(Entry.Storage, WorldRef, DeltaSeconds);
        }
        for (const PhaseEntry& Entry : m_tickEntries)
        {
            Entry.Invoke(Entry.Storage, WorldRef, DeltaSeconds);
        }
        for (const PhaseEntry& Entry : m_postTickEntries)
        {
            Entry.Invoke(Entry.Storage, WorldRef, DeltaSeconds);
        }
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief Execute editor-only runtime phases across all registered storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void EditorTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const PhaseEntry& Entry : m_editorTickEntries)
        {
            Entry.Invoke(Entry.Storage, WorldRef, DeltaSeconds);
        }
    }
#endif

    /**
     * @brief Execute fixed-step runtime phases across all registered storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param DeltaSeconds Fixed-step delta in seconds.
     */
    void FixedTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const PhaseEntry& Entry : m_fixedTickEntries)
        {
            Entry.Invoke(Entry.Storage, WorldRef, DeltaSeconds);
        }
    }

    /**
     * @brief Execute late runtime phases across all registered storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void LateTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const PhaseEntry& Entry : m_lateTickEntries)
        {
            Entry.Invoke(Entry.Storage, WorldRef, DeltaSeconds);
        }
    }

    /**
     * @brief Flush deferred destroys across all runtime storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     */
    void EndFrame(IWorld& WorldRef)
    {
        for (auto& [_, StorageModel] : m_storages)
        {
            if (StorageModel)
            {
                StorageModel->EndFrame(WorldRef);
            }
        }
        for (auto& [_, StorageModel] : m_nodeStorages)
        {
            if (StorageModel)
            {
                StorageModel->EndFrame(WorldRef);
            }
        }
    }

    /**
     * @brief Flush deferred `OnCreate` hooks across all storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     *
     * @warning Cross-storage flush order is unspecified because storages are traversed
     *          through an `unordered_map`.
     */
    void FlushPendingOnCreate(IWorld& WorldRef)
    {
        for (auto& [_, StorageModel] : m_nodeStorages)
        {
            if (StorageModel)
            {
                StorageModel->FlushPendingOnCreate(WorldRef);
            }
        }
        for (auto& [_, StorageModel] : m_storages)
        {
            if (StorageModel)
            {
                StorageModel->FlushPendingOnCreate(WorldRef);
            }
        }
    }

    /**
     * @brief Destroy all runtime components and reset the ECS runtime to empty.
     * @param WorldRef Owning world passed to lifecycle hooks.
     */
    void Clear(IWorld& WorldRef)
    {
        for (auto& [_, Attachments] : m_nodeComponentsByStorageToken)
        {
            for (const NodeComponentAttachment& Attachment : Attachments)
            {
                if (!Attachment.Alive)
                {
                    continue;
                }

                for (const NodeComponentLink& Link : Attachment.Components)
                {
                    if (!Link.Handle.Id.is_nil())
                    {
                        ObjectRegistry::Instance().Unregister(Link.Handle.Id);
                    }
                }
            }
        }

        for (auto& [Type, Storage] : m_storages)
        {
            (void)Type;
            Storage->Clear(WorldRef);
        }

        for (auto& [Type, Storage] : m_nodeStorages)
        {
            (void)Type;
            Storage->Clear(WorldRef);
        }

        m_nodeStorages.clear();
        m_nodeStorageByToken.clear();
        m_storages.clear();
        m_storageByToken.clear();
        m_nodeComponentsByStorageToken.clear();
        m_preTickEntries.clear();
        m_tickEntries.clear();
        m_fixedTickEntries.clear();
        m_lateTickEntries.clear();
        m_postTickEntries.clear();
#if defined(WITH_EDITOR) && WITH_EDITOR
        m_editorTickEntries.clear();
#endif
        m_nextTickSequence = 0;
    }

private:
    struct NodeComponentLink
    {
        TypeId Type{};
        RuntimeComponentHandle Handle{};
    };

    struct NodeComponentAttachment
    {
        uint32_t Generation = 0;
        bool Alive = false;
        std::vector<NodeComponentLink> Components{};
    };

    [[nodiscard]] static std::vector<NodeComponentAttachment>* FindNodeAttachmentBucket(
        std::unordered_map<uint32_t, std::vector<NodeComponentAttachment>>& Buckets,
        const uint32_t StorageToken)
    {
        if (auto It = Buckets.find(StorageToken); It != Buckets.end())
        {
            return &It->second;
        }
        return nullptr;
    }

    [[nodiscard]] static const std::vector<NodeComponentAttachment>* FindNodeAttachmentBucket(
        const std::unordered_map<uint32_t, std::vector<NodeComponentAttachment>>& Buckets,
        const uint32_t StorageToken)
    {
        if (auto It = Buckets.find(StorageToken); It != Buckets.end())
        {
            return &It->second;
        }
        return nullptr;
    }

    [[nodiscard]] NodeComponentAttachment* EnsureNodeAttachment(const NodeHandle& Owner)
    {
        NodeHandle ResolvedOwner = Owner;
        if (ResolvedOwner.IsNull())
        {
            return nullptr;
        }

        if (!ResolvedOwner.HasRuntimeKey())
        {
            (void)ResolvedOwner.Borrowed();
        }

        if (!ResolvedOwner.HasRuntimeKey())
        {
            return nullptr;
        }

        auto& Attachments = m_nodeComponentsByStorageToken[ResolvedOwner.RuntimePoolToken];
        if (ResolvedOwner.RuntimeIndex >= Attachments.size())
        {
            Attachments.resize(static_cast<std::size_t>(ResolvedOwner.RuntimeIndex) + 1u);
        }

        NodeComponentAttachment& Attachment = Attachments[ResolvedOwner.RuntimeIndex];
        if (!Attachment.Alive || Attachment.Generation != ResolvedOwner.RuntimeGeneration)
        {
            Attachment = NodeComponentAttachment{};
            Attachment.Generation = ResolvedOwner.RuntimeGeneration;
            Attachment.Alive = true;
        }
        return &Attachment;
    }

    [[nodiscard]] const NodeComponentAttachment* FindNodeAttachment(const NodeHandle& Owner) const
    {
        NodeHandle ResolvedOwner = Owner;
        if (ResolvedOwner.IsNull())
        {
            return nullptr;
        }

        if (!ResolvedOwner.HasRuntimeKey())
        {
            (void)ResolvedOwner.Borrowed();
        }

        if (!ResolvedOwner.HasRuntimeKey())
        {
            return nullptr;
        }
        const auto* Attachments = FindNodeAttachmentBucket(m_nodeComponentsByStorageToken, ResolvedOwner.RuntimePoolToken);
        if (!Attachments)
        {
            return nullptr;
        }

        if (ResolvedOwner.RuntimeIndex >= Attachments->size())
        {
            return nullptr;
        }

        const NodeComponentAttachment& Attachment = (*Attachments)[ResolvedOwner.RuntimeIndex];
        if (!Attachment.Alive || Attachment.Generation != ResolvedOwner.RuntimeGeneration)
        {
            return nullptr;
        }
        return &Attachment;
    }

    [[nodiscard]] NodeComponentAttachment* FindNodeAttachment(const NodeHandle& Owner)
    {
        return const_cast<NodeComponentAttachment*>(
            static_cast<const WorldEcsRuntime*>(this)->FindNodeAttachment(Owner));
    }

    [[nodiscard]] static std::optional<std::size_t> FindNodeComponentIndex(const NodeComponentAttachment& Attachment,
                                                                            const TypeId& Type)
    {
        for (std::size_t Index = 0; Index < Attachment.Components.size(); ++Index)
        {
            if (Attachment.Components[Index].Type == Type)
            {
                return Index;
            }
        }
        return std::nullopt;
    }

    static void RemoveNodeComponentAt(NodeComponentAttachment& Attachment, const std::size_t Index)
    {
        if (Index >= Attachment.Components.size())
        {
            return;
        }

        if (Index + 1u < Attachment.Components.size())
        {
            Attachment.Components[Index] = std::move(Attachment.Components.back());
        }
        Attachment.Components.pop_back();
    }

    void ClearNodeAttachment(const NodeHandle& Owner)
    {
        NodeHandle ResolvedOwner = Owner;
        if (ResolvedOwner.IsNull() || !ResolvedOwner.HasRuntimeKey())
        {
            return;
        }
        auto* Attachments = FindNodeAttachmentBucket(m_nodeComponentsByStorageToken, ResolvedOwner.RuntimePoolToken);
        if (!Attachments)
        {
            return;
        }

        if (ResolvedOwner.RuntimeIndex >= Attachments->size())
        {
            return;
        }

        (*Attachments)[ResolvedOwner.RuntimeIndex] = NodeComponentAttachment{};
    }

    void RemoveAllComponentsOnNode(IWorld& WorldRef, const NodeHandle& Owner)
    {
        (void)WorldRef;
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return;
        }

        for (const NodeComponentLink& Link : Attachment->Components)
        {
            if (IStorageModel* StorageModel = FindStorageModelByToken(Link.Handle.StorageToken))
            {
                (void)StorageModel->DestroyLaterByRuntimeHandle(Link.Handle);
            }
            else if (IStorageModel* TypeStorageModel = FindStorageModel(Link.Type))
            {
                (void)TypeStorageModel->DestroyLaterByRuntimeHandle(Link.Handle);
            }
        }
    }

    struct PhaseEntry
    {
        int Priority = 0;
        uint64_t Sequence = 0;
        std::string_view TypeName{};
        void* Storage = nullptr;
        void (*Invoke)(void*, IWorld&, float) = nullptr;
#if defined(WITH_EDITOR) && WITH_EDITOR
        ERuntimeTickPhase Phase = ERuntimeTickPhase::Tick;
#else
        ERuntimeTickPhase Phase = ERuntimeTickPhase::Tick;
#endif
    };

    class INodeStorageModel : public IErasedNodeStorage
    {
    public:
        [[nodiscard]] virtual uint32_t StorageToken() const = 0;
        [[nodiscard]] virtual TExpected<NodeHandle> CreateDefault(IWorld& WorldRef,
                                                                  std::string Name,
                                                                  const Uuid* ExplicitId) = 0;
        [[nodiscard]] virtual BaseNode* ResolveRawByRuntimeHandle(const NodeHandle& Handle) = 0;
        [[nodiscard]] virtual const BaseNode* ResolveRawByRuntimeHandle(const NodeHandle& Handle) const = 0;
        [[nodiscard]] virtual BaseNode* ResolveIncludingPendingDestroyByRuntimeHandle(const NodeHandle& Handle) = 0;
        [[nodiscard]] virtual const BaseNode* ResolveIncludingPendingDestroyByRuntimeHandle(const NodeHandle& Handle) const = 0;
        [[nodiscard]] virtual TExpected<NodeHandle> HandleById(const Uuid& Id) const = 0;
        virtual void FlushPendingOnCreate(IWorld& WorldRef) = 0;
        virtual bool FlushPendingOnCreate(IWorld& WorldRef, const NodeHandle& Handle) = 0;
        virtual void EndFrame(IWorld& WorldRef) = 0;
        virtual bool DestroyLaterByRuntimeHandle(const NodeHandle& Handle) = 0;
        virtual void Clear(IWorld& WorldRef) = 0;
        virtual void ForEachNode(void (*Visitor)(void*, const NodeHandle&, BaseNode&), void* UserData) = 0;
    };

    template<RuntimeTickType TObject>
    class TNodeStorageModel final : public INodeStorageModel
    {
    public:
        explicit TNodeStorageModel(const uint32_t StorageToken)
            : TypedStorage(StorageToken)
        {
        }

        ~TNodeStorageModel() override
        {
            ObjectRegistry::Instance().ReleaseRuntimePoolToken(TypedStorage.StorageToken());
        }

        [[nodiscard]] TypeId Type() const override
        {
            return StaticTypeId<TObject>();
        }

        [[nodiscard]] std::size_t Size() const override
        {
            return TypedStorage.Size();
        }

        [[nodiscard]] uint32_t StorageToken() const override
        {
            return TypedStorage.StorageToken();
        }

        [[nodiscard]] BaseNode* ResolveRaw(const Uuid& Id) override
        {
            return TypedStorage.ResolveSlowById(Id);
        }

        [[nodiscard]] const BaseNode* ResolveRaw(const Uuid& Id) const override
        {
            return TypedStorage.ResolveSlowById(Id);
        }

        [[nodiscard]] TExpected<NodeHandle> CreateDefault(IWorld& WorldRef,
                                                          std::string Name,
                                                          const Uuid* ExplicitId) override
        {
            if constexpr (!std::is_default_constructible_v<TObject>)
            {
                (void)WorldRef;
                (void)Name;
                (void)ExplicitId;
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Node type is not default constructible"));
            }
            else
            {
                auto CreateResult = ExplicitId
                    ? TypedStorage.CreateWithId(WorldRef, *ExplicitId)
                    : TypedStorage.Create(WorldRef);
                if (!CreateResult)
                {
                    return std::unexpected(CreateResult.error());
                }

                TObject* Node = TypedStorage.ResolveIncludingPendingDestroy(*CreateResult);
                if (!Node)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError, "Created node could not be resolved"));
                }

                InitializeNode(*Node, ToNodeHandle(*CreateResult), WorldRef, std::move(Name));
                SyncLiveRegistry(WorldRef);
                return ToNodeHandle(*CreateResult);
            }
        }

        [[nodiscard]] BaseNode* ResolveRawByRuntimeHandle(const NodeHandle& Handle) override
        {
            return TypedStorage.Resolve(ToTypedNodeRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] const BaseNode* ResolveRawByRuntimeHandle(const NodeHandle& Handle) const override
        {
            return TypedStorage.Resolve(ToTypedNodeRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] BaseNode* ResolveIncludingPendingDestroyByRuntimeHandle(const NodeHandle& Handle) override
        {
            return TypedStorage.ResolveIncludingPendingDestroy(ToTypedNodeRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] const BaseNode* ResolveIncludingPendingDestroyByRuntimeHandle(const NodeHandle& Handle) const override
        {
            return TypedStorage.ResolveIncludingPendingDestroy(ToTypedNodeRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] TExpected<NodeHandle> HandleById(const Uuid& Id) const override
        {
            auto HandleResult = TypedStorage.HandleById(Id);
            if (!HandleResult)
            {
                return std::unexpected(HandleResult.error());
            }
            return ToNodeHandle(*HandleResult);
        }

        void FlushPendingOnCreate(IWorld& WorldRef) override
        {
            TypedStorage.FlushPendingOnCreate(WorldRef);
        }

        bool FlushPendingOnCreate(IWorld& WorldRef, const NodeHandle& Handle) override
        {
            return TypedStorage.FlushPendingOnCreate(WorldRef, ToTypedNodeRuntimeHandle<TObject>(Handle));
        }

        void EndFrame(IWorld& WorldRef) override
        {
            TypedStorage.EndFrame(WorldRef);
            SyncLiveRegistry(WorldRef);
        }

        bool DestroyLaterByRuntimeHandle(const NodeHandle& Handle) override
        {
            TObject* Node = TypedStorage.ResolveIncludingPendingDestroy(ToTypedNodeRuntimeHandle<TObject>(Handle));
            if (!Node)
            {
                return false;
            }

            const bool Destroyed = TypedStorage.DestroyLater(ToTypedNodeRuntimeHandle<TObject>(Handle));
            if (Destroyed)
            {
                Node->PendingDestroy(true);
            }
            return Destroyed;
        }

        void Clear(IWorld& WorldRef) override
        {
            TypedStorage.ForEachAll([](const auto&, TObject& Node) {
                if (!Node.Id().is_nil())
                {
                    ObjectRegistry::Instance().Unregister(Node.Id());
                }
            });
            TypedStorage.Clear(WorldRef);
        }

        void ForEachNode(void (*Visitor)(void*, const NodeHandle&, BaseNode&), void* UserData) override
        {
            TypedStorage.ForEach([Visitor, UserData](const auto& Handle, TObject& Node) {
                Visitor(UserData, ToNodeHandle(Handle), Node);
            });
        }

        TDenseRuntimeStorage<TObject> TypedStorage;

    private:
        static void InitializeNode(TObject& Node, const NodeHandle& Handle, IWorld& WorldRef, std::string Name)
        {
            Node.Handle(Handle);
            Node.Name(std::move(Name));
            Node.World(&WorldRef);
            Node.PendingDestroy(false);
            Node.Parent({});
            Node.TypeKey(StaticTypeId<TObject>());
            ObjectRegistry::Instance().RegisterNode(
                Handle.Id,
                &Node,
                Handle.RuntimePoolToken,
                Handle.RuntimeIndex,
                Handle.RuntimeGeneration);
        }

        void SyncLiveRegistry(IWorld& WorldRef)
        {
            TypedStorage.ForEach([&WorldRef](const auto& Handle, TObject& Node) {
                const NodeHandle NodeHandleValue = ToNodeHandle(Handle);
                Node.Handle(NodeHandleValue);
                Node.World(&WorldRef);
                Node.PendingDestroy(false);
                Node.TypeKey(StaticTypeId<TObject>());
                ObjectRegistry::Instance().RegisterNode(
                    NodeHandleValue.Id,
                    &Node,
                    NodeHandleValue.RuntimePoolToken,
                    NodeHandleValue.RuntimeIndex,
                    NodeHandleValue.RuntimeGeneration);
            });
        }
    };

    class IStorageModel : public IErasedStorage
    {
    public:
        [[nodiscard]] virtual uint32_t StorageToken() const = 0;
        [[nodiscard]] virtual TExpected<RuntimeComponentHandle> CreateDefault(IWorld& WorldRef, const Uuid* ExplicitId) = 0;
        virtual void FlushPendingOnCreate(IWorld& WorldRef) = 0;
        virtual void EndFrame(IWorld& WorldRef) = 0;
        virtual bool DestroyLaterByRuntimeHandle(RuntimeComponentHandle Handle) = 0;
        virtual bool DestroyByRuntimeHandle(IWorld& WorldRef, RuntimeComponentHandle Handle) = 0;
        [[nodiscard]] virtual void* ResolveRawByRuntimeHandle(RuntimeComponentHandle Handle) = 0;
        [[nodiscard]] virtual const void* ResolveRawByRuntimeHandle(RuntimeComponentHandle Handle) const = 0;
        virtual void Clear(IWorld& WorldRef) = 0;
    };

    template<RuntimeTickType TObject>
    class TStorageModel final : public IStorageModel
    {
    public:
        explicit TStorageModel(const uint32_t StorageToken)
            : TypedStorage(StorageToken)
        {
        }

        ~TStorageModel() override
        {
            ObjectRegistry::Instance().ReleaseRuntimePoolToken(TypedStorage.StorageToken());
        }

        [[nodiscard]] TypeId Type() const override
        {
            return StaticTypeId<TObject>();
        }

        [[nodiscard]] std::size_t Size() const override
        {
            return TypedStorage.Size();
        }

        [[nodiscard]] uint32_t StorageToken() const override
        {
            return TypedStorage.StorageToken();
        }

        [[nodiscard]] void* ResolveRaw(const Uuid& Id) override
        {
            return TypedStorage.ResolveSlowById(Id);
        }

        [[nodiscard]] const void* ResolveRaw(const Uuid& Id) const override
        {
            return TypedStorage.ResolveSlowById(Id);
        }

        bool DestroyById(IWorld& WorldRef, const Uuid& Id) override
        {
            return TypedStorage.DestroySlow(WorldRef, Id);
        }

        [[nodiscard]] TExpected<RuntimeComponentHandle> CreateDefault(IWorld& WorldRef, const Uuid* ExplicitId) override
        {
            if constexpr (!std::is_default_constructible_v<TObject>)
            {
                (void)WorldRef;
                (void)ExplicitId;
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Runtime component type is not default constructible"));
            }
            else
            {
                auto CreateResult = ExplicitId
                    ? TypedStorage.CreateWithId(WorldRef, *ExplicitId)
                    : TypedStorage.Create(WorldRef);
                if (!CreateResult)
                {
                    return std::unexpected(CreateResult.error());
                }
                SyncLiveRegistry();
                return ToRuntimeComponentHandle(*CreateResult);
            }
        }

        void FlushPendingOnCreate(IWorld& WorldRef) override
        {
            TypedStorage.FlushPendingOnCreate(WorldRef);
        }

        void EndFrame(IWorld& WorldRef) override
        {
            TypedStorage.EndFrame(WorldRef);
            SyncLiveRegistry();
        }

        bool DestroyLaterByRuntimeHandle(const RuntimeComponentHandle Handle) override
        {
            return TypedStorage.DestroyLater(ToTypedRuntimeHandle<TObject>(Handle));
        }

        bool DestroyByRuntimeHandle(IWorld& WorldRef, const RuntimeComponentHandle Handle) override
        {
            const bool Destroyed = TypedStorage.Destroy(WorldRef, ToTypedRuntimeHandle<TObject>(Handle));
            if (Destroyed)
            {
                SyncLiveRegistry();
            }
            return Destroyed;
        }

        [[nodiscard]] void* ResolveRawByRuntimeHandle(const RuntimeComponentHandle Handle) override
        {
            return TypedStorage.Resolve(ToTypedRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] const void* ResolveRawByRuntimeHandle(const RuntimeComponentHandle Handle) const override
        {
            return TypedStorage.Resolve(ToTypedRuntimeHandle<TObject>(Handle));
        }

        void Clear(IWorld& WorldRef) override
        {
            TypedStorage.Clear(WorldRef);
        }

        TDenseRuntimeStorage<TObject> TypedStorage;

    private:
        void SyncLiveRegistry()
        {
            if constexpr (std::is_base_of_v<BaseComponent, TObject>)
            {
                TypedStorage.ForEach([](const auto& Handle, TObject& Component) {
                    const RuntimeComponentHandle GenericHandle = ToRuntimeComponentHandle(Handle);
                    Component.TypeKey(StaticTypeId<TObject>());
                    Component.Id(GenericHandle.Id);
                    Component.RuntimeIdentity(
                        GenericHandle.StorageToken,
                        GenericHandle.Index,
                        GenericHandle.Generation);
                    ObjectRegistry::Instance().RegisterComponent(
                        GenericHandle.Id,
                        &Component,
                        GenericHandle.StorageToken,
                        GenericHandle.Index,
                        GenericHandle.Generation);
                });
            }
        }
    };

    [[nodiscard]] INodeStorageModel* FindNodeStorageModel(const TypeId& Type)
    {
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] const INodeStorageModel* FindNodeStorageModel(const TypeId& Type) const
    {
        if (auto It = m_nodeStorages.find(Type); It != m_nodeStorages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] INodeStorageModel* FindNodeStorageModelByToken(const uint32_t StorageToken)
    {
        if (auto It = m_nodeStorageByToken.find(StorageToken); It != m_nodeStorageByToken.end())
        {
            return It->second;
        }
        return nullptr;
    }

    [[nodiscard]] const INodeStorageModel* FindNodeStorageModelByToken(const uint32_t StorageToken) const
    {
        if (auto It = m_nodeStorageByToken.find(StorageToken); It != m_nodeStorageByToken.end())
        {
            return It->second;
        }
        return nullptr;
    }

    [[nodiscard]] IStorageModel* FindStorageModel(const TypeId& Type)
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] const IStorageModel* FindStorageModel(const TypeId& Type) const
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] IStorageModel* FindStorageModelByToken(const uint32_t StorageToken)
    {
        if (auto It = m_storageByToken.find(StorageToken); It != m_storageByToken.end())
        {
            return It->second;
        }
        return nullptr;
    }

    [[nodiscard]] const IStorageModel* FindStorageModelByToken(const uint32_t StorageToken) const
    {
        if (auto It = m_storageByToken.find(StorageToken); It != m_storageByToken.end())
        {
            return It->second;
        }
        return nullptr;
    }

    template<RuntimeTickType TObject>
    static void DispatchPreTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->PreTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->Tick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchFixedTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->FixedTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchLateTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->LateTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchPostTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->PostTick(WorldRef, DeltaSeconds);
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    template<RuntimeTickType TObject>
    static void DispatchEditorTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->EditorTick(WorldRef, DeltaSeconds);
    }
#endif

    void InsertPhaseEntry(std::vector<PhaseEntry>& Entries, PhaseEntry Entry)
    {
        const auto It = std::lower_bound(Entries.begin(),
                                         Entries.end(),
                                         Entry,
                                         [](const PhaseEntry& Left, const PhaseEntry& Right) {
                                             if (Left.Priority != Right.Priority)
                                             {
                                                 return Left.Priority < Right.Priority;
                                             }
                                             if (Left.TypeName != Right.TypeName)
                                             {
                                                 return Left.TypeName < Right.TypeName;
                                             }
                                             return Left.Sequence < Right.Sequence;
                                         });
        Entries.insert(It, Entry);
    }

    template<RuntimeTickType TObject>
    void RegisterTickEntry(TDenseRuntimeStorage<TObject>* Storage)
    {
        constexpr bool kHasAnyTickPhase =
            TDenseRuntimeStorage<TObject>::kHasPreTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasFixedTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasLateTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasPostTickPhase
#if defined(WITH_EDITOR) && WITH_EDITOR
            || TDenseRuntimeStorage<TObject>::kHasEditorTickPhase
#endif
            ;
        if constexpr (!kHasAnyTickPhase)
        {
            return;
        }

        const uint64_t Sequence = m_nextTickSequence++;
        const std::string_view TypeName = TTypeNameV<TObject>;
        if constexpr (TDenseRuntimeStorage<TObject>::kHasPreTickPhase)
        {
            InsertPhaseEntry(
                m_preTickEntries,
                PhaseEntry{
                    .Priority = RuntimePhasePriority<TObject, ERuntimeTickPhase::PreTick>(),
                    .Sequence = Sequence,
                    .TypeName = TypeName,
                    .Storage = Storage,
                    .Invoke = &DispatchPreTick<TObject>,
#if defined(WITH_EDITOR) && WITH_EDITOR
                    .Phase = ERuntimeTickPhase::PreTick,
#else
                    .Phase = ERuntimeTickPhase::PreTick,
#endif
                });
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasTickPhase)
        {
            InsertPhaseEntry(
                m_tickEntries,
                PhaseEntry{
                    .Priority = RuntimePhasePriority<TObject, ERuntimeTickPhase::Tick>(),
                    .Sequence = Sequence,
                    .TypeName = TypeName,
                    .Storage = Storage,
                    .Invoke = &DispatchTick<TObject>,
#if defined(WITH_EDITOR) && WITH_EDITOR
                    .Phase = ERuntimeTickPhase::Tick,
#else
                    .Phase = ERuntimeTickPhase::Tick,
#endif
                });
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasFixedTickPhase)
        {
            InsertPhaseEntry(
                m_fixedTickEntries,
                PhaseEntry{
                    .Priority = RuntimePhasePriority<TObject, ERuntimeTickPhase::FixedTick>(),
                    .Sequence = Sequence,
                    .TypeName = TypeName,
                    .Storage = Storage,
                    .Invoke = &DispatchFixedTick<TObject>,
#if defined(WITH_EDITOR) && WITH_EDITOR
                    .Phase = ERuntimeTickPhase::FixedTick,
#else
                    .Phase = ERuntimeTickPhase::FixedTick,
#endif
                });
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasLateTickPhase)
        {
            InsertPhaseEntry(
                m_lateTickEntries,
                PhaseEntry{
                    .Priority = RuntimePhasePriority<TObject, ERuntimeTickPhase::LateTick>(),
                    .Sequence = Sequence,
                    .TypeName = TypeName,
                    .Storage = Storage,
                    .Invoke = &DispatchLateTick<TObject>,
#if defined(WITH_EDITOR) && WITH_EDITOR
                    .Phase = ERuntimeTickPhase::LateTick,
#else
                    .Phase = ERuntimeTickPhase::LateTick,
#endif
                });
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasPostTickPhase)
        {
            InsertPhaseEntry(
                m_postTickEntries,
                PhaseEntry{
                    .Priority = RuntimePhasePriority<TObject, ERuntimeTickPhase::PostTick>(),
                    .Sequence = Sequence,
                    .TypeName = TypeName,
                    .Storage = Storage,
                    .Invoke = &DispatchPostTick<TObject>,
#if defined(WITH_EDITOR) && WITH_EDITOR
                    .Phase = ERuntimeTickPhase::PostTick,
#else
                    .Phase = ERuntimeTickPhase::PostTick,
#endif
                });
        }
#if defined(WITH_EDITOR) && WITH_EDITOR
        if constexpr (TDenseRuntimeStorage<TObject>::kHasEditorTickPhase)
        {
            InsertPhaseEntry(
                m_editorTickEntries,
                PhaseEntry{
                    .Priority = RuntimePhasePriority<TObject, ERuntimeTickPhase::EditorTick>(),
                    .Sequence = Sequence,
                    .TypeName = TypeName,
                    .Storage = Storage,
                    .Invoke = &DispatchEditorTick<TObject>,
                    .Phase = ERuntimeTickPhase::EditorTick,
                });
        }
#endif
    }

    uint32_t AcquireStorageToken()
    {
        const uint32_t Token = ObjectRegistry::Instance().AcquireRuntimePoolToken();
        if (Token == RuntimeComponentHandle::kInvalidStorageToken)
        {
            DEBUG_ASSERT(false, "Failed to acquire runtime storage token");
        }
        return Token;
    }

    std::unordered_map<TypeId, std::unique_ptr<INodeStorageModel>, UuidHash> m_nodeStorages{};
    std::unordered_map<uint32_t, INodeStorageModel*> m_nodeStorageByToken{};
    std::unordered_map<TypeId, std::unique_ptr<IStorageModel>, UuidHash> m_storages{};
    std::unordered_map<uint32_t, IStorageModel*> m_storageByToken{};
    std::unordered_map<uint32_t, std::vector<NodeComponentAttachment>> m_nodeComponentsByStorageToken{};
    std::vector<PhaseEntry> m_preTickEntries{};
    std::vector<PhaseEntry> m_tickEntries{};
    std::vector<PhaseEntry> m_fixedTickEntries{};
    std::vector<PhaseEntry> m_lateTickEntries{};
    std::vector<PhaseEntry> m_postTickEntries{};
#if defined(WITH_EDITOR) && WITH_EDITOR
    std::vector<PhaseEntry> m_editorTickEntries{};
#endif
    uint64_t m_nextTickSequence = 0;
};

} // namespace SnAPI::GameFramework
