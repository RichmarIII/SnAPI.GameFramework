#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Math.h"
#include "ObjectRegistry.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class IWorld;

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
 * @brief Marker CRTP base for dense runtime node records.
 * @tparam TDerived Concrete runtime node type.
 */
template<typename TDerived>
struct NodeCRTP
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Marker CRTP base for dense runtime component records.
 * @tparam TDerived Concrete runtime component type.
 */
template<typename TDerived>
struct ComponentCRTP
{
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
 * @brief Compile-time contract for types eligible to live in `TDenseRuntimeStorage`.
 *
 * A valid runtime type must satisfy `NonPolymorphicRuntimeType` and opt into one of the
 * marker CRTP families. Optional lifecycle hooks such as `OnCreate`, `Tick`,
 * `FixedTick`, `LateTick`, and `PostTick` are then detected automatically.
 */
template<typename T>
concept RuntimeTickType =
    NonPolymorphicRuntimeType<T> &&
    (kUsesRuntimeTickCRTP<T> || kUsesNodeCRTP<T> || kUsesComponentCRTP<T>);

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
 * @brief Read a runtime type's compile-time tick priority.
 * @tparam TObject Runtime object type.
 * @return `TObject::kTickPriority` when present, otherwise `0`.
 *
 * Lower values execute earlier because `WorldEcsRuntime` sorts tick entries in ascending
 * priority order.
 */
template<typename TObject>
consteval int RuntimeTickPriority()
{
    if constexpr (requires { TObject::kTickPriority; })
    {
        return static_cast<int>(TObject::kTickPriority);
    }
    else
    {
        return 0;
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
 * stored contiguously in `m_denseObjects`, while stable identity is tracked through a
 * slot table plus generation-safe handles.
 *
 * Core semantics:
 * - Dense order is unstable and may change on destroy via swap-pop compaction.
 * - UUID identity is unique within the storage.
 * - `OnCreate` may run immediately or be deferred via `PendingOnCreate`.
 * - `OnDestroy` runs synchronously during destroy/clear, not at a later frame boundary.
 *
 * Ownership and lifetime:
 * - The storage owns all contained `TObject` instances by value.
 * - Resolved pointers are borrowed and invalidated by any destroy or clear that moves or
 *   erases the underlying dense array.
 *
 * Threading:
 * - Main-thread only.
 *
 * Performance:
 * - Handle resolution is O(1).
 * - UUID fallback resolution is O(1) average through `m_idToSlot`.
 * - Tick phases iterate linearly over contiguous storage.
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

    /** @brief Get the stable token that identifies this storage instance in handles. */
    [[nodiscard]] uint32_t StorageToken() const noexcept
    {
        return m_storageToken;
    }

    /** @brief Get the current number of live runtime objects in dense storage. */
    [[nodiscard]] std::size_t Size() const noexcept
    {
        return m_denseObjects.size();
    }

    /** @brief Return `true` when the storage contains no live objects. */
    [[nodiscard]] bool Empty() const noexcept
    {
        return m_denseObjects.empty();
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
        const uint32_t DenseIndex = static_cast<uint32_t>(m_denseObjects.size());

        try
        {
            m_denseObjects.emplace_back(std::forward<TArgs>(Args)...);
        }
        catch (...)
        {
            RollbackCreate(SlotIndex);
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to construct runtime object"));
        }

        m_denseSlotIndices.push_back(SlotIndex);
        Slot.Alive = true;
        Slot.DenseIndex = DenseIndex;

        if constexpr (kHasOnCreatePhase)
        {
            if (!IsComponentOnCreateSuppressed())
            {
                InvokeOnCreate(m_denseObjects.back(), WorldRef);
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
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return false;
        }

        DestroyBySlot(WorldRef, SlotIndex);
        return true;
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

        const SlotMeta& Slot = m_slots[SlotIndex];
        return &m_denseObjects[Slot.DenseIndex];
    }

    /** @brief Const overload of `Resolve(const Handle&)`. */
    const TObject* Resolve(const Handle& InHandle) const
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        return &m_denseObjects[Slot.DenseIndex];
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
        if (!Slot.Alive || Slot.DenseIndex >= m_denseObjects.size())
        {
            return nullptr;
        }

        return &m_denseObjects[Slot.DenseIndex];
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
        if (!Slot.Alive || Slot.DenseIndex >= m_denseObjects.size())
        {
            return nullptr;
        }

        return &m_denseObjects[Slot.DenseIndex];
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
        if (!Slot.Alive)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime object not alive"));
        }

        return MakeHandle(SlotIndex);
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
            for (TObject& Object : m_denseObjects)
            {
                InvokePreTick(Object, WorldRef, DeltaSeconds);
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
            for (TObject& Object : m_denseObjects)
            {
                InvokeTick(Object, WorldRef, DeltaSeconds);
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
            for (TObject& Object : m_denseObjects)
            {
                InvokeFixedTick(Object, WorldRef, DeltaSeconds);
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
            for (TObject& Object : m_denseObjects)
            {
                InvokeLateTick(Object, WorldRef, DeltaSeconds);
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
            for (TObject& Object : m_denseObjects)
            {
                InvokePostTick(Object, WorldRef, DeltaSeconds);
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
            for (TObject& Object : m_denseObjects)
            {
                InvokeEditorTick(Object, WorldRef, DeltaSeconds);
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

        for (SlotMeta& Slot : m_slots)
        {
            if (!Slot.Alive || !Slot.PendingOnCreate || Slot.DenseIndex >= m_denseObjects.size())
            {
                continue;
            }

            InvokeOnCreate(m_denseObjects[Slot.DenseIndex], WorldRef);
            Slot.PendingOnCreate = false;
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
            for (TObject& Object : m_denseObjects)
            {
                InvokeOnDestroy(Object, WorldRef);
            }
        }
        else
        {
            (void)WorldRef;
        }

        m_denseObjects.clear();
        m_denseSlotIndices.clear();
        m_idToSlot.clear();
        m_freeSlotIndices.clear();

        m_freeSlotIndices.reserve(m_slots.size());
        for (uint32_t SlotIndex = 0; SlotIndex < m_slots.size(); ++SlotIndex)
        {
            SlotMeta& Slot = m_slots[SlotIndex];
            Slot.Id = {};
            Slot.Alive = false;
            Slot.DenseIndex = Handle::kInvalidIndex;
            Slot.PendingOnCreate = false;
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
        uint32_t DenseIndex = Handle::kInvalidIndex;
        bool Alive = false;
        bool PendingOnCreate = false;
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

    bool ResolveSlot(const Handle& InHandle, uint32_t& OutSlotIndex) const
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
        if (!Slot.Alive || Slot.Generation != InHandle.Generation || Slot.Id != InHandle.Id)
        {
            return false;
        }

        OutSlotIndex = InHandle.Index;
        return true;
    }

    uint32_t AcquireSlot(const Uuid& Id)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!m_freeSlotIndices.empty())
        {
            SlotIndex = m_freeSlotIndices.back();
            m_freeSlotIndices.pop_back();
        }
        else
        {
            SlotIndex = static_cast<uint32_t>(m_slots.size());
            m_slots.emplace_back();
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        Slot.Id = Id;
        Slot.Alive = false;
        Slot.DenseIndex = Handle::kInvalidIndex;
        Slot.PendingOnCreate = false;
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
        Slot.DenseIndex = Handle::kInvalidIndex;
        Slot.Alive = false;
        Slot.PendingOnCreate = false;
        m_freeSlotIndices.push_back(SlotIndex);
    }

    void DestroyBySlot(IWorld& WorldRef, const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_slots.size())
        {
            return;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.DenseIndex == Handle::kInvalidIndex || Slot.DenseIndex >= m_denseObjects.size())
        {
            return;
        }

        const uint32_t DenseIndex = Slot.DenseIndex;
        const uint32_t LastDenseIndex = static_cast<uint32_t>(m_denseObjects.size() - 1u);

        if constexpr (kHasOnDestroyPhase)
        {
            InvokeOnDestroy(m_denseObjects[DenseIndex], WorldRef);
        }
        else
        {
            (void)WorldRef;
        }

        if (DenseIndex != LastDenseIndex)
        {
            std::swap(m_denseObjects[DenseIndex], m_denseObjects[LastDenseIndex]);

            const uint32_t MovedSlotIndex = m_denseSlotIndices[LastDenseIndex];
            m_denseSlotIndices[DenseIndex] = MovedSlotIndex;
            m_slots[MovedSlotIndex].DenseIndex = DenseIndex;
        }

        m_denseObjects.pop_back();
        m_denseSlotIndices.pop_back();

        m_idToSlot.erase(Slot.Id);
        Slot.Id = {};
        Slot.Alive = false;
        Slot.DenseIndex = Handle::kInvalidIndex;
        Slot.PendingOnCreate = false;
        Slot.Generation = (Slot.Generation == std::numeric_limits<uint32_t>::max()) ? 1u : (Slot.Generation + 1u);
        if (Slot.Generation == 0u)
        {
            Slot.Generation = 1u;
        }
        m_freeSlotIndices.push_back(SlotIndex);
    }

    uint32_t m_storageToken = 1;
    std::vector<TObject> m_denseObjects{};
    std::vector<uint32_t> m_denseSlotIndices{};
    std::vector<SlotMeta> m_slots{};
    std::vector<uint32_t> m_freeSlotIndices{};
    std::unordered_map<Uuid, uint32_t, UuidHash> m_idToSlot{};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Minimal runtime-owned metadata stored for each ECS runtime node.
 *
 * This is the compact node-side record tracked by `WorldNodeRuntime`. Higher-level node
 * behavior may still live elsewhere; this struct only carries the identity and flags
 * needed by the dense runtime layer itself.
 */
struct RuntimeNodeRecord final : NodeCRTP<RuntimeNodeRecord>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RuntimeNodeRecord";

    std::string Name{"Node"}; /**< @brief Debug/editor-facing node label. */
    TypeId Type{}; /**< @brief Reflected runtime node type. */
    bool Active = true; /**< @brief Runtime active flag available to higher-level systems. */
    bool Replicated = false; /**< @brief Replication intent flag for networking layers. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Typed handle alias for runtime nodes stored in `WorldNodeRuntime`.
 */
using RuntimeNodeHandle = TDenseRuntimeHandle<RuntimeNodeRecord>;

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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Local or world transform used by the dense runtime node hierarchy.
 *
 * Units and coordinate space:
 * - `Position` uses the same world-space units as the rest of GameFramework.
 * - `Rotation` is a quaternion.
 * - `Scale` is component-wise relative scale.
 */
struct RuntimeNodeTransform
{
    Vec3 Position{}; /**< @brief Translation in local or world space, depending on the API. */
    Quat Rotation = Quat::Identity(); /**< @brief Orientation quaternion. */
    Vec3 Scale{1.0f, 1.0f, 1.0f}; /**< @brief Component-wise scale. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Dense hierarchy runtime that owns runtime nodes, parent/child links, and cached transforms.
 *
 * `WorldNodeRuntime` is the node-side half of the ECS runtime refactor. It centralizes:
 * - runtime node identity and metadata
 * - parent/child hierarchy links
 * - root tracking
 * - local and cached world transforms
 *
 * Core semantics:
 * - Handles are generation-safe and become invalid once a node slot is reused.
 * - Root membership is maintained automatically by attach/detach operations.
 * - World transforms are cached and recomputed lazily when a subtree is marked dirty.
 * - Destroying a node destroys its entire subtree iteratively in child-first order.
 *
 * Threading:
 * - Main-thread only.
 */
class WorldNodeRuntime final
{
public:
    using Handle = RuntimeNodeHandle;

    /**
     * @brief Create a runtime node with a generated UUID.
     * @param WorldRef Owning world passed to runtime lifecycle hooks.
     * @param Name Debug/editor-facing node name.
     * @param Type Reflected runtime node type.
     * @return Handle to the new node, or an error on failure.
     */
    TExpected<Handle> CreateNode(IWorld& WorldRef, std::string Name, const TypeId& Type)
    {
        return CreateNodeWithId(WorldRef, NewUuid(), std::move(Name), Type);
    }

    /**
     * @brief Create a runtime node with an explicit UUID.
     * @param WorldRef Owning world passed to runtime lifecycle hooks.
     * @param Id Stable node identity.
     * @param Name Debug/editor-facing node name.
     * @param Type Reflected runtime node type.
     * @return Handle to the new node, or an error when creation fails.
     *
     * Newly created nodes start as roots with no parent and no explicit local transform.
     */
    TExpected<Handle> CreateNodeWithId(IWorld& WorldRef, const Uuid& Id, std::string Name, const TypeId& Type)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime node type is null"));
        }

        RuntimeNodeRecord Record{};
        Record.Name = std::move(Name);
        Record.Type = Type;

        auto HandleResult = m_nodes.CreateWithId(WorldRef, Id, std::move(Record));
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }

        const Handle CreatedHandle = *HandleResult;
        if (CreatedHandle.Index == Handle::kInvalidIndex)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime node slot index is invalid"));
        }

        EnsureHierarchySlot(CreatedHandle.Index);
        HierarchyEntry& Entry = m_hierarchyBySlot[CreatedHandle.Index];
        Entry = {};
        Entry.Generation = CreatedHandle.Generation;
        Entry.Alive = true;

        AddRootIfMissing(CreatedHandle);
        return CreatedHandle;
    }

    /**
     * @brief Destroy a runtime node and all descendants.
     * @param WorldRef Owning world passed to runtime lifecycle hooks.
     * @param NodeHandle Root of the subtree to destroy.
     * @return Success or an error when the handle is invalid.
     */
    Result DestroyNode(IWorld& WorldRef, const Handle NodeHandle)
    {
        if (NodeHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime node handle is null"));
        }

        return DestroyNodeIterative(WorldRef, NodeHandle);
    }

    /**
     * @brief Attach a child node under a parent node.
     * @param ParentHandle Parent runtime node.
     * @param ChildHandle Child runtime node.
     * @return Success or an error when handles are invalid, the child already has a
     *         parent, or the operation would create a cycle.
     */
    Result AttachChild(const Handle ParentHandle, const Handle ChildHandle)
    {
        if (ParentHandle.IsNull() || ChildHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Parent/child handle is null"));
        }
        if (ParentHandle == ChildHandle)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node cannot be parent of itself"));
        }
        if (!m_nodes.Resolve(ParentHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Parent runtime node not found"));
        }
        if (!m_nodes.Resolve(ChildHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child runtime node not found"));
        }

        HierarchyEntry* ParentState = EntryForHandle(ParentHandle);
        HierarchyEntry* ChildState = EntryForHandle(ChildHandle);
        if (!ParentState || !ChildState)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime hierarchy state missing"));
        }

        if (!ChildState->Parent.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child already has a parent"));
        }

        for (Handle Cursor = ParentHandle; !Cursor.IsNull();)
        {
            if (Cursor == ChildHandle)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Hierarchy cycle detected"));
            }

            const HierarchyEntry* CursorState = EntryForHandle(Cursor);
            if (!CursorState)
            {
                break;
            }
            Cursor = CursorState->Parent;
        }

        auto& ParentChildren = ParentState->Children;
        if (std::find(ParentChildren.begin(), ParentChildren.end(), ChildHandle) == ParentChildren.end())
        {
            ParentChildren.push_back(ChildHandle);
        }

        ChildState->Parent = ParentHandle;
        RemoveRootIfPresent(ChildHandle);
        MarkSubtreeDirty(ChildHandle);
        return Ok();
    }

    /**
     * @brief Detach a node from its current parent, promoting it to a root.
     * @param ChildHandle Child runtime node to detach.
     * @return Success or an error when the handle is invalid.
     */
    Result DetachChild(const Handle ChildHandle)
    {
        if (ChildHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child handle is null"));
        }
        if (!m_nodes.Resolve(ChildHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child runtime node not found"));
        }

        HierarchyEntry* ChildState = EntryForHandle(ChildHandle);
        if (!ChildState)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Child hierarchy state missing"));
        }

        const Handle ParentHandle = ChildState->Parent;
        if (!ParentHandle.IsNull())
        {
            if (HierarchyEntry* ParentState = EntryForHandle(ParentHandle))
            {
                RemoveChildLink(*ParentState, ChildHandle);
            }
        }
        ChildState->Parent = {};
        AddRootIfMissing(ChildHandle);
        MarkSubtreeDirty(ChildHandle);
        return Ok();
    }

    /** @brief Resolve a runtime node handle to borrowed mutable node metadata. */
    [[nodiscard]] RuntimeNodeRecord* Resolve(const Handle NodeHandle)
    {
        return m_nodes.Resolve(NodeHandle);
    }

    /** @brief Const overload of `Resolve(const Handle)`. */
    [[nodiscard]] const RuntimeNodeRecord* Resolve(const Handle NodeHandle) const
    {
        return m_nodes.Resolve(NodeHandle);
    }

    /** @brief Rebuild a current runtime node handle from a UUID. */
    [[nodiscard]] TExpected<Handle> HandleById(const Uuid& Id) const
    {
        return m_nodes.HandleById(Id);
    }

    /**
     * @brief Get the parent handle of a runtime node.
     * @param ChildHandle Child runtime node.
     * @return Parent handle when the link is valid, otherwise a null handle.
     */
    [[nodiscard]] Handle Parent(const Handle ChildHandle) const
    {
        const HierarchyEntry* ChildState = EntryForHandle(ChildHandle);
        if (!ChildState)
        {
            return {};
        }

        const Handle ParentHandle = ChildState->Parent;
        return EntryForHandle(ParentHandle) ? ParentHandle : Handle{};
    }

    /**
     * @brief Collect the current live children of a parent node.
     * @param ParentHandle Parent runtime node.
     * @return Vector of live child handles in stored child order.
     */
    [[nodiscard]] std::vector<Handle> Children(const Handle ParentHandle) const
    {
        const HierarchyEntry* ParentState = EntryForHandle(ParentHandle);
        if (!ParentState)
        {
            return {};
        }

        std::vector<Handle> Result{};
        Result.reserve(ParentState->Children.size());
        ForEachChild(ParentHandle, [&](const Handle ChildHandle) {
            Result.push_back(ChildHandle);
        });
        return Result;
    }

    /**
     * @brief Visit each live child handle of a parent node.
     * @tparam TVisitor Callable invocable as `Visitor(Handle)`.
     * @param ParentHandle Parent runtime node.
     * @param Visitor Callback invoked for each currently live child.
     */
    template<typename TVisitor>
        requires std::invocable<TVisitor, Handle>
    void ForEachChild(const Handle ParentHandle, TVisitor&& Visitor) const
    {
        const HierarchyEntry* ParentState = EntryForHandle(ParentHandle);
        if (!ParentState)
        {
            return;
        }

        for (const Handle ChildHandle : ParentState->Children)
        {
            if (EntryForHandle(ChildHandle))
            {
                Visitor(ChildHandle);
            }
        }
    }

    /** @brief Borrow the current root-handle list. */
    [[nodiscard]] const std::vector<Handle>& Roots() const
    {
        return m_roots;
    }

    /** @brief Get the number of live runtime nodes. */
    [[nodiscard]] std::size_t Size() const
    {
        return m_nodes.Size();
    }

    /**
     * @brief Assign an explicit local transform to a runtime node.
     * @param NodeHandle Target runtime node.
     * @param LocalTransform Local transform relative to the parent.
     * @return `true` when the node exists and the transform was stored.
     *
     * The input rotation is normalized before storage. The entire subtree is marked dirty
     * so cached world transforms will be recomputed lazily.
     */
    bool SetLocalTransform(const Handle NodeHandle, const RuntimeNodeTransform& LocalTransform)
    {
        if (!m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        HierarchyEntry* Entry = EntryForHandle(NodeHandle);
        if (!Entry)
        {
            return false;
        }

        Entry->LocalTransform = NormalizeTransformRotation(LocalTransform);
        Entry->HasLocalTransform = true;
        MarkSubtreeDirty(NodeHandle);
        return true;
    }

    /**
     * @brief Remove a node's explicit local transform.
     * @param NodeHandle Target runtime node.
     * @return `true` when the node exists.
     *
     * Clearing a local transform means the node contributes no authored transform of its
     * own; world transform queries may then inherit only ancestor transforms.
     */
    bool ClearLocalTransform(const Handle NodeHandle)
    {
        if (!m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        HierarchyEntry* Entry = EntryForHandle(NodeHandle);
        if (!Entry)
        {
            return false;
        }

        if (!Entry->HasLocalTransform)
        {
            return true;
        }

        Entry->LocalTransform = IdentityTransform();
        Entry->HasLocalTransform = false;
        MarkSubtreeDirty(NodeHandle);
        return true;
    }

    /**
     * @brief Read the stored local transform for a node.
     * @param NodeHandle Target runtime node.
     * @param OutTransform Receives the local transform on success. Reset to identity on entry.
     * @return `true` when the node has an explicit local transform.
     */
    bool TryGetLocalTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform) const
    {
        OutTransform = IdentityTransform();

        if (!m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        const HierarchyEntry* Entry = EntryForHandle(NodeHandle);
        if (!Entry || !Entry->HasLocalTransform)
        {
            return false;
        }

        OutTransform = Entry->LocalTransform;
        return true;
    }

    /**
     * @brief Compute or fetch the cached world transform for a node.
     * @param NodeHandle Target runtime node.
     * @param OutTransform Receives the computed world transform. Reset to identity on entry.
     * @return `true` when the node has a world transform to report.
     *
     * Returns `false` both for invalid nodes and for valid nodes that neither define a
     * local transform nor inherit one from an ancestor.
     */
    bool TryGetWorldTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform)
    {
        OutTransform = IdentityTransform();
        bool HasTransform = false;
        if (!ComputeWorldTransform(NodeHandle, OutTransform, HasTransform))
        {
            return false;
        }
        return HasTransform;
    }

    /**
     * @brief Compute the world transform of a node's parent.
     * @param NodeHandle Child runtime node.
     * @param OutTransform Receives the parent world transform.
     * @return `true` when the node has a parent and that parent has a world transform.
     */
    bool TryGetParentWorldTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform)
    {
        OutTransform = IdentityTransform();
        if (NodeHandle.IsNull() || !m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        const Handle ParentHandle = Parent(NodeHandle);
        if (ParentHandle.IsNull())
        {
            return false;
        }

        bool HasTransform = false;
        if (!ComputeWorldTransform(ParentHandle, OutTransform, HasTransform))
        {
            return false;
        }
        return HasTransform;
    }

    /**
     * @brief Set a node's world transform by converting it into local space relative to the parent.
     * @param NodeHandle Target runtime node.
     * @param WorldTransform Desired world-space transform.
     * @return `true` when the node exists and the local transform was updated.
     *
     * Parent scale is inverted with a safety threshold, so near-zero parent scale axes
     * collapse to `0` rather than producing infinities.
     */
    bool TrySetWorldTransform(const Handle NodeHandle, const RuntimeNodeTransform& WorldTransform)
    {
        if (NodeHandle.IsNull() || !m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        RuntimeNodeTransform ParentWorld = IdentityTransform();
        const bool HasParentWorld = TryGetParentWorldTransform(NodeHandle, ParentWorld);

        const RuntimeNodeTransform NormalizedWorld = NormalizeTransformRotation(WorldTransform);
        const RuntimeNodeTransform Local = HasParentWorld
            ? LocalTransformFromWorld(ParentWorld, NormalizedWorld)
            : NormalizedWorld;

        return SetLocalTransform(NodeHandle, Local);
    }

    /**
     * @brief Destroy all runtime nodes and reset the hierarchy runtime to empty.
     * @param WorldRef Owning world passed to runtime lifecycle hooks.
     */
    void Clear(IWorld& WorldRef)
    {
        m_hierarchyBySlot.clear();
        m_roots.clear();
        m_nodes.Clear(WorldRef);
    }

private:
    struct HierarchyEntry
    {
        Handle Parent{};
        std::vector<Handle> Children{};
        RuntimeNodeTransform LocalTransform{};
        RuntimeNodeTransform CachedWorldTransform{};
        uint32_t Generation = 0;
        bool HasLocalTransform = false;
        bool CachedHasWorldTransform = false;
        bool Dirty = true;
        bool Alive = false;
    };

    static void RemoveChildLink(HierarchyEntry& ParentEntry, const Handle ChildHandle)
    {
        auto It = std::remove(ParentEntry.Children.begin(), ParentEntry.Children.end(), ChildHandle);
        if (It != ParentEntry.Children.end())
        {
            ParentEntry.Children.erase(It, ParentEntry.Children.end());
        }
    }

    void EnsureHierarchySlot(const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_hierarchyBySlot.size())
        {
            m_hierarchyBySlot.resize(static_cast<std::size_t>(SlotIndex) + 1u);
        }
    }

    HierarchyEntry* EntryForHandle(const Handle NodeHandle)
    {
        if (NodeHandle.IsNull() || NodeHandle.Index == Handle::kInvalidIndex)
        {
            return nullptr;
        }
        if (NodeHandle.Index >= m_hierarchyBySlot.size())
        {
            return nullptr;
        }

        HierarchyEntry& Entry = m_hierarchyBySlot[NodeHandle.Index];
        if (!Entry.Alive || Entry.Generation != NodeHandle.Generation)
        {
            return nullptr;
        }
        return &Entry;
    }

    const HierarchyEntry* EntryForHandle(const Handle NodeHandle) const
    {
        if (NodeHandle.IsNull() || NodeHandle.Index == Handle::kInvalidIndex)
        {
            return nullptr;
        }
        if (NodeHandle.Index >= m_hierarchyBySlot.size())
        {
            return nullptr;
        }

        const HierarchyEntry& Entry = m_hierarchyBySlot[NodeHandle.Index];
        if (!Entry.Alive || Entry.Generation != NodeHandle.Generation)
        {
            return nullptr;
        }
        return &Entry;
    }

    void AddRootIfMissing(const Handle NodeHandle)
    {
        if (std::find(m_roots.begin(), m_roots.end(), NodeHandle) == m_roots.end())
        {
            m_roots.push_back(NodeHandle);
        }
    }

    void RemoveRootIfPresent(const Handle NodeHandle)
    {
        auto It = std::find(m_roots.begin(), m_roots.end(), NodeHandle);
        if (It != m_roots.end())
        {
            m_roots.erase(It);
        }
    }

    static RuntimeNodeTransform IdentityTransform()
    {
        return {};
    }

    static RuntimeNodeTransform NormalizeTransformRotation(const RuntimeNodeTransform& InTransform)
    {
        RuntimeNodeTransform Out = InTransform;
        Out.Rotation = NormalizeQuatOrIdentity(Out.Rotation);
        return Out;
    }

    static Quat NormalizeQuatOrIdentity(const Quat& Rotation)
    {
        Quat Out = Rotation;
        if (Out.squaredNorm() > static_cast<Quat::Scalar>(0))
        {
            Out.normalize();
        }
        else
        {
            Out = Quat::Identity();
        }
        return Out;
    }

    static Vec3 SafeScaleDivide(const Vec3& Numerator, const Vec3& Denominator)
    {
        constexpr Vec3::Scalar kMinScaleMagnitude = static_cast<Vec3::Scalar>(1.0e-6);
        Vec3 Out{};
        const auto DivideAxis = [&](const Vec3::Scalar Value, const Vec3::Scalar Divisor) -> Vec3::Scalar {
            if (std::abs(Divisor) <= kMinScaleMagnitude)
            {
                return static_cast<Vec3::Scalar>(0);
            }
            return Value / Divisor;
        };

        Out.x() = DivideAxis(Numerator.x(), Denominator.x());
        Out.y() = DivideAxis(Numerator.y(), Denominator.y());
        Out.z() = DivideAxis(Numerator.z(), Denominator.z());
        return Out;
    }

    static RuntimeNodeTransform ComposeTransform(const RuntimeNodeTransform& ParentWorld, const RuntimeNodeTransform& Local)
    {
        const RuntimeNodeTransform NormalizedParent = NormalizeTransformRotation(ParentWorld);
        const RuntimeNodeTransform NormalizedLocal = NormalizeTransformRotation(Local);

        RuntimeNodeTransform Out = IdentityTransform();
        Out.Position = NormalizedParent.Position
                     + (NormalizedParent.Rotation * NormalizedParent.Scale.cwiseProduct(NormalizedLocal.Position));
        Out.Rotation = NormalizeQuatOrIdentity(NormalizedParent.Rotation * NormalizedLocal.Rotation);
        Out.Scale = NormalizedParent.Scale.cwiseProduct(NormalizedLocal.Scale);
        return Out;
    }

    static RuntimeNodeTransform LocalTransformFromWorld(const RuntimeNodeTransform& ParentWorld, const RuntimeNodeTransform& World)
    {
        const RuntimeNodeTransform NormalizedParent = NormalizeTransformRotation(ParentWorld);
        const RuntimeNodeTransform NormalizedWorld = NormalizeTransformRotation(World);

        const Quat ParentInverse = NormalizedParent.Rotation.conjugate();
        const Vec3 ParentSpacePosition = ParentInverse * (NormalizedWorld.Position - NormalizedParent.Position);

        RuntimeNodeTransform Out = IdentityTransform();
        Out.Position = SafeScaleDivide(ParentSpacePosition, NormalizedParent.Scale);
        Out.Rotation = NormalizeQuatOrIdentity(ParentInverse * NormalizedWorld.Rotation);
        Out.Scale = SafeScaleDivide(NormalizedWorld.Scale, NormalizedParent.Scale);
        return Out;
    }

    void MarkSubtreeDirty(const Handle NodeHandle)
    {
        if (NodeHandle.IsNull())
        {
            return;
        }

        m_dirtyTraversalScratch.clear();
        m_dirtyTraversalScratch.push_back(NodeHandle);
        while (!m_dirtyTraversalScratch.empty())
        {
            const Handle Current = m_dirtyTraversalScratch.back();
            m_dirtyTraversalScratch.pop_back();

            HierarchyEntry* Entry = EntryForHandle(Current);
            if (!Entry)
            {
                continue;
            }

            Entry->Dirty = true;
            for (const Handle ChildHandle : Entry->Children)
            {
                m_dirtyTraversalScratch.push_back(ChildHandle);
            }
        }
    }

    bool ComputeWorldTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform, bool& OutHasTransform)
    {
        OutTransform = IdentityTransform();
        OutHasTransform = false;

        if (NodeHandle.IsNull() || !m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        std::vector<Handle> Ancestry{};
        Ancestry.reserve(16);

        Handle Cursor = NodeHandle;
        std::size_t Depth = 0;
        const std::size_t MaxDepth = m_hierarchyBySlot.size() + 1u;
        while (true)
        {
            if (Depth++ > MaxDepth)
            {
                return false;
            }

            HierarchyEntry* Entry = EntryForHandle(Cursor);
            if (!Entry)
            {
                return false;
            }

            Ancestry.push_back(Cursor);
            if (!Entry->Dirty)
            {
                OutTransform = Entry->CachedWorldTransform;
                OutHasTransform = Entry->CachedHasWorldTransform;
                break;
            }

            if (Entry->Parent.IsNull())
            {
                OutTransform = IdentityTransform();
                OutHasTransform = false;
                break;
            }

            Cursor = Entry->Parent;
            if (!m_nodes.Resolve(Cursor))
            {
                return false;
            }
        }

        for (auto It = Ancestry.rbegin(); It != Ancestry.rend(); ++It)
        {
            HierarchyEntry* Entry = EntryForHandle(*It);
            if (!Entry)
            {
                return false;
            }

            if (!Entry->Dirty)
            {
                OutTransform = Entry->CachedWorldTransform;
                OutHasTransform = Entry->CachedHasWorldTransform;
                continue;
            }

            RuntimeNodeTransform ComputedWorld = IdentityTransform();
            bool ComputedHasWorldTransform = false;
            if (Entry->HasLocalTransform)
            {
                ComputedWorld = OutHasTransform ? ComposeTransform(OutTransform, Entry->LocalTransform) : Entry->LocalTransform;
                ComputedHasWorldTransform = true;
            }
            else if (OutHasTransform)
            {
                ComputedWorld = OutTransform;
                ComputedHasWorldTransform = true;
            }

            Entry->CachedWorldTransform = ComputedWorld;
            Entry->CachedHasWorldTransform = ComputedHasWorldTransform;
            Entry->Dirty = false;

            OutTransform = ComputedWorld;
            OutHasTransform = ComputedHasWorldTransform;
        }

        return true;
    }

    Result DestroyNodeIterative(IWorld& WorldRef, const Handle RootHandle)
    {
        if (RootHandle.IsNull() || !m_nodes.Resolve(RootHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime node not found"));
        }

        m_destroyTraversalScratch.clear();
        m_destroyTraversalScratch.emplace_back(RootHandle, false);

        while (!m_destroyTraversalScratch.empty())
        {
            const auto [CurrentHandle, Expanded] = m_destroyTraversalScratch.back();
            m_destroyTraversalScratch.pop_back();

            if (!Expanded)
            {
                if (!m_nodes.Resolve(CurrentHandle))
                {
                    continue;
                }

                HierarchyEntry* Entry = EntryForHandle(CurrentHandle);
                if (!Entry)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime hierarchy state missing"));
                }

                m_destroyTraversalScratch.emplace_back(CurrentHandle, true);
                for (const Handle ChildHandle : Entry->Children)
                {
                    if (m_nodes.Resolve(ChildHandle))
                    {
                        m_destroyTraversalScratch.emplace_back(ChildHandle, false);
                    }
                }
                continue;
            }

            HierarchyEntry* Entry = EntryForHandle(CurrentHandle);
            if (!Entry)
            {
                continue;
            }

            const Handle ParentHandle = Entry->Parent;
            if (!ParentHandle.IsNull())
            {
                if (HierarchyEntry* ParentEntry = EntryForHandle(ParentHandle))
                {
                    RemoveChildLink(*ParentEntry, CurrentHandle);
                }
            }

            RemoveRootIfPresent(CurrentHandle);
            *Entry = HierarchyEntry{};

            if (!m_nodes.Destroy(WorldRef, CurrentHandle))
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to destroy runtime node"));
            }
        }

        return Ok();
    }

    TDenseRuntimeStorage<RuntimeNodeRecord> m_nodes{};
    std::vector<HierarchyEntry> m_hierarchyBySlot{};
    std::vector<Handle> m_roots{};
    std::vector<Handle> m_dirtyTraversalScratch{};
    std::vector<std::pair<Handle, bool>> m_destroyTraversalScratch{};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned orchestration layer for dense runtime nodes and typed runtime component storages.
 *
 * `WorldEcsRuntime` is the top-level container that ties together:
 * - `WorldNodeRuntime` for runtime node identity and hierarchy
 * - lazily created `TDenseRuntimeStorage<T>` instances for concrete runtime types
 * - one-component-per-type attachments from runtime nodes to runtime components
 * - globally ordered tick dispatch by compile-time priority
 *
 * Core semantics:
 * - Typed `Storage<T>()` creation is lazy and also registers tick dispatch for `T` when
 *   it exposes any runtime tick phase.
 * - Tick order is ascending `RuntimeTickPriority<T>()`; ties keep storage creation order.
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
    [[nodiscard]] WorldNodeRuntime& Nodes()
    {
        return m_nodeRuntime;
    }

    /** @brief Const access to the world-owned runtime node hierarchy. */
    [[nodiscard]] const WorldNodeRuntime& Nodes() const
    {
        return m_nodeRuntime;
    }

    /**
     * @brief Create and attach a typed runtime component to a runtime node.
     * @tparam TObject Runtime component type.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Runtime node that will own the component.
     * @param Args Constructor arguments for `TObject`.
     * @return Typed runtime-component handle, or an error when the node is invalid or
     *         already owns that component type.
     */
    template<RuntimeTickType TObject, typename... TArgs>
    TExpected<TDenseRuntimeHandle<TObject>> AddComponent(IWorld& WorldRef,
                                                         const RuntimeNodeHandle Owner,
                                                         TArgs&&... Args)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
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
     * @param Owner Runtime node that will own the component.
     * @param Id Stable component identity.
     * @param Args Constructor arguments for `TObject`.
     * @return Typed runtime-component handle, or an error on failure.
     */
    template<RuntimeTickType TObject, typename... TArgs>
    TExpected<TDenseRuntimeHandle<TObject>> AddComponentWithId(IWorld& WorldRef,
                                                               const RuntimeNodeHandle Owner,
                                                               const Uuid& Id,
                                                               TArgs&&... Args)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
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
    TObject* Component(const RuntimeNodeHandle Owner)
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
    const TObject* Component(const RuntimeNodeHandle Owner) const
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
     * @param Owner Runtime node that owns the component.
     * @return `true` when the underlying typed storage destroyed the component.
     *
     * @warning Current behavior removes the node-to-component attachment record once it
     *          is found, even if the typed storage destroy path reports failure.
     */
    template<RuntimeTickType TObject>
    bool RemoveComponent(IWorld& WorldRef, const RuntimeNodeHandle Owner)
    {
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
            Destroyed = TypedStorage->Destroy(WorldRef, ToTypedRuntimeHandle<TObject>(GenericHandle));
        }
        RemoveNodeComponentAt(*Attachment, *LinkIndex);
        return Destroyed;
    }

    /**
     * @brief Dynamically create and attach a runtime component by reflected type id.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Runtime node that will own the component.
     * @param Type Reflected component type.
     * @return Generic runtime-component handle, or an error on failure.
     *
     * Unlike the typed overload, this path does not create a storage model implicitly.
     * The target storage must already exist, usually because `Storage<T>()` was created
     * earlier for that runtime type.
     */
    TExpected<RuntimeComponentHandle> AddComponent(IWorld& WorldRef,
                                                   const RuntimeNodeHandle Owner,
                                                   const TypeId& Type)
    {
        return AddComponentWithId(WorldRef, Owner, Type, {});
    }

    /**
     * @brief Dynamic overload of `AddComponent(...)` with explicit component UUID.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param Owner Runtime node that will own the component.
     * @param Type Reflected component type.
     * @param Id Explicit component UUID. A nil UUID requests auto-generation.
     * @return Generic runtime-component handle, or an error on failure.
     */
    TExpected<RuntimeComponentHandle> AddComponentWithId(IWorld& WorldRef,
                                                         const RuntimeNodeHandle Owner,
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
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
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
     * @param Owner Runtime node that owns the component.
     * @param Type Reflected component type.
     * @return Success when the backing storage destroy path succeeded, otherwise an error.
     *
     * @warning As with the typed overload, the node attachment record is removed even if
     *          the underlying storage destroy path fails.
     */
    Result RemoveComponent(IWorld& WorldRef,
                           const RuntimeNodeHandle Owner,
                           const TypeId& Type)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime component type is null"));
        }

        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
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
            Destroyed = StorageModel->DestroyByRuntimeHandle(WorldRef, GenericHandle);
        }
        else if (IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            Destroyed = TypeStorageModel->DestroyByRuntimeHandle(WorldRef, GenericHandle);
        }

        RemoveNodeComponentAt(*Attachment, *LinkIndex);
        if (!GenericHandle.Id.is_nil())
        {
            ObjectRegistry::Instance().Unregister(GenericHandle.Id);
        }
        return Destroyed ? Ok() : std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found"));
    }

    /** @brief Return `true` when a runtime node currently owns a component of the given reflected type. */
    [[nodiscard]] bool HasComponent(const RuntimeNodeHandle Owner, const TypeId& Type) const
    {
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        return Attachment && FindNodeComponentIndex(*Attachment, Type).has_value();
    }

    /**
     * @brief Fetch the generic runtime-component handle attached to a node for a specific reflected type.
     * @param Owner Runtime node owner.
     * @param Type Reflected component type.
     * @return Generic runtime-component handle, or an error when the attachment is absent.
     */
    [[nodiscard]] TExpected<RuntimeComponentHandle> ComponentHandle(const RuntimeNodeHandle Owner,
                                                                    const TypeId& Type) const
    {
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
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
     * @brief Destroy a runtime node subtree and all runtime components attached within that subtree.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param RootHandle Root of the subtree to destroy.
     * @return Success or an error when the root handle is invalid.
     *
     * Components are destroyed child-first before the node hierarchy itself is removed.
     */
    Result DestroyRuntimeNode(IWorld& WorldRef, const RuntimeNodeHandle RootHandle)
    {
        if (RootHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime node handle is null"));
        }
        if (!m_nodeRuntime.Resolve(RootHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime node not found"));
        }

        m_componentDestroyScratch.clear();
        m_componentDestroyScratch.push_back(RootHandle);
        for (std::size_t Index = 0; Index < m_componentDestroyScratch.size(); ++Index)
        {
            const RuntimeNodeHandle Current = m_componentDestroyScratch[Index];
            if (!m_nodeRuntime.Resolve(Current))
            {
                continue;
            }

            m_nodeRuntime.ForEachChild(Current, [&](const RuntimeNodeHandle Child) {
                if (m_nodeRuntime.Resolve(Child))
                {
                    m_componentDestroyScratch.push_back(Child);
                }
            });
        }

        for (auto It = m_componentDestroyScratch.rbegin(); It != m_componentDestroyScratch.rend(); ++It)
        {
            RemoveAllComponentsOnNode(WorldRef, *It);
            ClearNodeAttachment(*It);
        }

        return m_nodeRuntime.DestroyNode(WorldRef, RootHandle);
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
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.PreTick)
            {
                Entry.PreTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
            if (Entry.Tick)
            {
                Entry.Tick(Entry.Storage, WorldRef, DeltaSeconds);
            }
            if (Entry.PostTick)
            {
                Entry.PostTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
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
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.EditorTick)
            {
                Entry.EditorTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
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
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.FixedTick)
            {
                Entry.FixedTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
        }
    }

    /**
     * @brief Execute late runtime phases across all registered storages.
     * @param WorldRef Owning world passed to lifecycle hooks.
     * @param DeltaSeconds Variable-step delta in seconds.
     */
    void LateTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.LateTick)
            {
                Entry.LateTick(Entry.Storage, WorldRef, DeltaSeconds);
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
        for (auto& [_, StorageModel] : m_storages)
        {
            if (StorageModel)
            {
                StorageModel->FlushPendingOnCreate(WorldRef);
            }
        }
    }

    /**
     * @brief Destroy all runtime components and nodes and reset the ECS runtime to empty.
     * @param WorldRef Owning world passed to lifecycle hooks.
     */
    void Clear(IWorld& WorldRef)
    {
        for (const NodeComponentAttachment& Attachment : m_nodeComponentsBySlot)
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

        for (auto& [Type, Storage] : m_storages)
        {
            (void)Type;
            Storage->Clear(WorldRef);
        }
        m_nodeComponentsBySlot.clear();
        m_componentDestroyScratch.clear();
        m_nodeRuntime.Clear(WorldRef);
    }

private:
    class IStorageModel;

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

    [[nodiscard]] NodeComponentAttachment* EnsureNodeAttachment(const RuntimeNodeHandle Owner)
    {
        if (Owner.IsNull() || Owner.Index == RuntimeNodeHandle::kInvalidIndex || !m_nodeRuntime.Resolve(Owner))
        {
            return nullptr;
        }

        if (Owner.Index >= m_nodeComponentsBySlot.size())
        {
            m_nodeComponentsBySlot.resize(static_cast<std::size_t>(Owner.Index) + 1u);
        }

        NodeComponentAttachment& Attachment = m_nodeComponentsBySlot[Owner.Index];
        if (!Attachment.Alive || Attachment.Generation != Owner.Generation)
        {
            Attachment = NodeComponentAttachment{};
            Attachment.Generation = Owner.Generation;
            Attachment.Alive = true;
        }
        return &Attachment;
    }

    [[nodiscard]] const NodeComponentAttachment* FindNodeAttachment(const RuntimeNodeHandle Owner) const
    {
        if (Owner.IsNull() || Owner.Index == RuntimeNodeHandle::kInvalidIndex || !m_nodeRuntime.Resolve(Owner))
        {
            return nullptr;
        }
        if (Owner.Index >= m_nodeComponentsBySlot.size())
        {
            return nullptr;
        }

        const NodeComponentAttachment& Attachment = m_nodeComponentsBySlot[Owner.Index];
        if (!Attachment.Alive || Attachment.Generation != Owner.Generation)
        {
            return nullptr;
        }
        return &Attachment;
    }

    [[nodiscard]] NodeComponentAttachment* FindNodeAttachment(const RuntimeNodeHandle Owner)
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

    void ClearNodeAttachment(const RuntimeNodeHandle Owner)
    {
        if (Owner.IsNull() || Owner.Index == RuntimeNodeHandle::kInvalidIndex)
        {
            return;
        }
        if (Owner.Index >= m_nodeComponentsBySlot.size())
        {
            return;
        }

        m_nodeComponentsBySlot[Owner.Index] = NodeComponentAttachment{};
    }

    void RemoveAllComponentsOnNode(IWorld& WorldRef, const RuntimeNodeHandle Owner)
    {
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return;
        }

        for (const NodeComponentLink& Link : Attachment->Components)
        {
            if (IStorageModel* StorageModel = FindStorageModelByToken(Link.Handle.StorageToken))
            {
                (void)StorageModel->DestroyByRuntimeHandle(WorldRef, Link.Handle);
            }
            else if (IStorageModel* TypeStorageModel = FindStorageModel(Link.Type))
            {
                (void)TypeStorageModel->DestroyByRuntimeHandle(WorldRef, Link.Handle);
            }

            if (!Link.Handle.Id.is_nil())
            {
                ObjectRegistry::Instance().Unregister(Link.Handle.Id);
            }
        }
    }

    struct TickEntry
    {
        int Priority = 0;
        uint64_t Sequence = 0;
        void* Storage = nullptr;
        void (*PreTick)(void*, IWorld&, float) = nullptr;
        void (*Tick)(void*, IWorld&, float) = nullptr;
        void (*FixedTick)(void*, IWorld&, float) = nullptr;
        void (*LateTick)(void*, IWorld&, float) = nullptr;
        void (*PostTick)(void*, IWorld&, float) = nullptr;
#if defined(WITH_EDITOR) && WITH_EDITOR
        void (*EditorTick)(void*, IWorld&, float) = nullptr;
#endif
    };

    class IStorageModel : public IErasedStorage
    {
    public:
        [[nodiscard]] virtual uint32_t StorageToken() const = 0;
        [[nodiscard]] virtual TExpected<RuntimeComponentHandle> CreateDefault(IWorld& WorldRef, const Uuid* ExplicitId) = 0;
        virtual void FlushPendingOnCreate(IWorld& WorldRef) = 0;
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
                return ToRuntimeComponentHandle(*CreateResult);
            }
        }

        void FlushPendingOnCreate(IWorld& WorldRef) override
        {
            TypedStorage.FlushPendingOnCreate(WorldRef);
        }

        bool DestroyByRuntimeHandle(IWorld& WorldRef, const RuntimeComponentHandle Handle) override
        {
            return TypedStorage.Destroy(WorldRef, ToTypedRuntimeHandle<TObject>(Handle));
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
    };

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

        const int Priority = RuntimeTickPriority<TObject>();
        const uint64_t Sequence = m_nextTickSequence++;

        TickEntry Entry{};
        Entry.Priority = Priority;
        Entry.Sequence = Sequence;
        Entry.Storage = Storage;
        if constexpr (TDenseRuntimeStorage<TObject>::kHasPreTickPhase)
        {
            Entry.PreTick = &DispatchPreTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasTickPhase)
        {
            Entry.Tick = &DispatchTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasFixedTickPhase)
        {
            Entry.FixedTick = &DispatchFixedTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasLateTickPhase)
        {
            Entry.LateTick = &DispatchLateTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasPostTickPhase)
        {
            Entry.PostTick = &DispatchPostTick<TObject>;
        }
#if defined(WITH_EDITOR) && WITH_EDITOR
        if constexpr (TDenseRuntimeStorage<TObject>::kHasEditorTickPhase)
        {
            Entry.EditorTick = &DispatchEditorTick<TObject>;
        }
#endif

        const auto It = std::lower_bound(m_tickEntries.begin(),
                                         m_tickEntries.end(),
                                         Entry,
                                         [](const TickEntry& Left, const TickEntry& Right) {
                                             if (Left.Priority != Right.Priority)
                                             {
                                                 return Left.Priority < Right.Priority;
                                             }
                                             return Left.Sequence < Right.Sequence;
                                         });
        m_tickEntries.insert(It, Entry);
    }

    uint32_t AcquireStorageToken()
    {
        const uint32_t Token = ObjectRegistry::Instance().AcquireRuntimePoolToken();
        if (Token == TDenseRuntimeHandle<RuntimeNodeRecord>::kInvalidStorageToken)
        {
            DEBUG_ASSERT(false, "Failed to acquire runtime storage token");
        }
        return Token;
    }

    std::unordered_map<TypeId, std::unique_ptr<IStorageModel>, UuidHash> m_storages{};
    std::unordered_map<uint32_t, IStorageModel*> m_storageByToken{};
    WorldNodeRuntime m_nodeRuntime{};
    std::vector<NodeComponentAttachment> m_nodeComponentsBySlot{};
    std::vector<RuntimeNodeHandle> m_componentDestroyScratch{};
    std::vector<TickEntry> m_tickEntries{};
    uint64_t m_nextTickSequence = 0;
};

} // namespace SnAPI::GameFramework
