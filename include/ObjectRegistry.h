#pragma once

#include <cstdint>
#include <cstdio>
#include <limits>
#include <mutex>
#include "GameThreading.h"
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Assert.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class BaseComponent;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime lookup category stored in `ObjectRegistry`.
 *
 * `BaseNode` and `BaseComponent` have dedicated categories because callers often need
 * to resolve them through their common base type. Arbitrary "other" objects are matched
 * by exact registered `typeid(T)` rather than by inheritance.
 */
enum class EObjectKind
{
    Node,      /**< @brief BaseNode-derived object. */
    Component, /**< @brief BaseComponent-derived object. */
    Other      /**< @brief Arbitrary registered type. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Process-wide bridge from UUID/runtime-handle identity to live object pointers.
 *
 * The registry exists to support two complementary lookup modes:
 * - fast-path resolution through `(runtime pool token, runtime index, generation)`
 * - fallback resolution through stable UUID identity
 *
 * Higher-level systems such as node pools, component storage, and ECS runtime storages
 * register objects here so `THandle` can cheaply recover a live pointer without each
 * subsystem reinventing the same indirection table.
 *
 * Ownership and lifetime:
 * - The registry never owns the objects it points at.
 * - Stored pointers are borrowed and become invalid as soon as the owning system
 *   unregisters the UUID.
 * - Pool tokens are registry-owned identities that outlive the pool instance and are
 *   intentionally never reused.
 *
 * Threading:
 * - Not generally thread-safe.
 * - Internal `GameMutex` use performs affinity validation only; it is not a real
 *   cross-thread lock.
 * - Register, unregister, and resolve on the owning thread or under external
 *   synchronization.
 *
 * Error semantics:
 * - Missing entries resolve to `nullptr`.
 * - Type mismatches also resolve to `nullptr`; no exception or cast failure is thrown.
 */
class ObjectRegistry
{
public:
    /** @brief Runtime pool token sentinel meaning "no runtime pool". */
    static constexpr uint32_t kInvalidRuntimePoolToken = 0;
    /** @brief Runtime slot index sentinel meaning "no runtime slot". */
    static constexpr uint32_t kInvalidRuntimeIndex = std::numeric_limits<uint32_t>::max();

    /**
     * @brief Refreshed runtime identity returned after a UUID fallback successfully finds a live object.
     *
     * Callers that fall back from runtime-key lookup should copy this data back into the
     * same handle instance so future resolutions can return to the fast path.
     */
    struct RuntimeIdentity
    {
        uint32_t RuntimePoolToken = kInvalidRuntimePoolToken;
        uint32_t RuntimeIndex = kInvalidRuntimeIndex;
        uint32_t RuntimeGeneration = 0;
    };

    /** @brief Access the process-wide singleton registry. */
    static ObjectRegistry& Instance()
    {
        static ObjectRegistry Registry;
        return Registry;
    }

    /**
     * @brief Acquire a fresh runtime-pool token for one handle-producing storage instance.
     * @return Token used to bind runtime index lookups for a pool instance.
     *
     * Tokens are monotonically assigned and never reused. That prevents a stale handle
     * from accidentally resolving into a different pool that later reused the same slot
     * index and generation.
     */
    uint32_t AcquireRuntimePoolToken()
    {
        GameLockGuard Lock(m_mutex);
        // Slot 0 is permanently reserved as the invalid runtime-pool token.
        if (m_runtimeSlotsByPool.empty())
        {
            m_runtimeSlotsByPool.emplace_back();
        }

        const uint64_t Next = static_cast<uint64_t>(m_runtimeSlotsByPool.size());
        if (Next > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))
        {
            DEBUG_ASSERT(false, "Runtime pool token overflow");
            return kInvalidRuntimePoolToken;
        }
        m_runtimeSlotsByPool.emplace_back();
        return static_cast<uint32_t>(Next);
    }

    /**
     * @brief Clear all runtime-slot bindings currently associated with a pool token.
     * @param PoolToken Token to release.
     *
     * Release does not recycle the token number. It only clears the fast-path runtime
     * slots so old handles stop resolving by runtime key.
     */
    void ReleaseRuntimePoolToken(uint32_t PoolToken)
    {
        if (PoolToken == kInvalidRuntimePoolToken)
        {
            return;
        }

        GameLockGuard Lock(m_mutex);
        if (PoolToken >= m_runtimeSlotsByPool.size())
        {
            return;
        }

        auto& Slots = m_runtimeSlotsByPool[PoolToken];
        for (auto& Slot : Slots)
        {
            Slot = RuntimeSlot{};
        }
    }

    /**
     * @brief Register a node for UUID-based lookup only.
     * @param Id UUID of the node.
     * @param Node Borrowed node pointer.
     *
     * If an entry already exists for `Id`, the old registration is replaced.
     */
    void RegisterNode(const Uuid& Id, BaseNode* Node)
    {
        RegisterInternal(Id,
            EObjectKind::Node,
            Node,
            nullptr,
            nullptr,
            std::type_index(typeid(void)),
            kInvalidRuntimePoolToken,
            kInvalidRuntimeIndex,
            0);
    }

    /**
     * @brief Register a node with both UUID fallback identity and runtime-key fast-path identity.
     * @param Id UUID of the node.
     * @param Node Borrowed node pointer.
     * @param RuntimePoolToken Runtime pool token from owning pool.
     * @param RuntimeIndex Runtime slot index in owning pool.
     * @param RuntimeGeneration Runtime slot generation in owning pool.
     *
     * @post `ResolveFast<BaseNode>(...)` can resolve the node directly when the runtime
     *       key still matches a live slot.
     */
    void RegisterNode(const Uuid& Id,
        BaseNode* Node,
        uint32_t RuntimePoolToken,
        uint32_t RuntimeIndex,
        uint32_t RuntimeGeneration)
    {
        RegisterInternal(Id,
            EObjectKind::Node,
            Node,
            nullptr,
            nullptr,
            std::type_index(typeid(void)),
            RuntimePoolToken,
            RuntimeIndex,
            RuntimeGeneration);
    }

    /** @brief Register a component for UUID-based lookup only. */
    void RegisterComponent(const Uuid& Id, BaseComponent* Component)
    {
        RegisterInternal(Id,
            EObjectKind::Component,
            nullptr,
            Component,
            nullptr,
            std::type_index(typeid(void)),
            kInvalidRuntimePoolToken,
            kInvalidRuntimeIndex,
            0);
    }

    /**
     * @brief Register a component with both UUID fallback identity and runtime-key fast-path identity.
     * @param Id UUID of the component.
     * @param Component Borrowed component pointer.
     * @param RuntimePoolToken Runtime pool token from owning pool.
     * @param RuntimeIndex Runtime slot index in owning pool.
     * @param RuntimeGeneration Runtime slot generation in owning pool.
     * @remarks Existing registrations for the same UUID are overwritten.
     */
    void RegisterComponent(const Uuid& Id,
        BaseComponent* Component,
        uint32_t RuntimePoolToken,
        uint32_t RuntimeIndex,
        uint32_t RuntimeGeneration)
    {
        RegisterInternal(Id,
            EObjectKind::Component,
            nullptr,
            Component,
            nullptr,
            std::type_index(typeid(void)),
            RuntimePoolToken,
            RuntimeIndex,
            RuntimeGeneration);
    }

    /**
     * @brief Register an arbitrary non-node, non-component object.
     * @tparam T Object type.
     * @param Id UUID of the object.
     * @param Object Borrowed pointer to the object.
     *
     * @remarks Existing registrations for the same UUID are overwritten.
     *
     * For `EObjectKind::Other`, resolution uses exact `typeid(T)` equality. Registering
     * a derived object under `TBase` does not make `Resolve<TDerived>()` succeed.
     */
    template<typename T>
    void Register(const Uuid& Id, T* Object)
    {
        RegisterInternal(Id,
            EObjectKind::Other,
            nullptr,
            nullptr,
            static_cast<void*>(Object),
            typeid(T),
            kInvalidRuntimePoolToken,
            kInvalidRuntimeIndex,
            0);
    }

    /**
     * @brief Remove an object's UUID and runtime-slot bindings from the registry.
     * @param Id UUID to remove.
     * @remarks Safe to call for missing entries.
     */
    void Unregister(const Uuid& Id)
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Id);
        if (It == m_entries.end())
        {
            return;
        }

        ClearRuntimeSlotLocked(It->second);
        m_entries.erase(It);
    }

    /**
     * @brief Resolve an object through the UUID fallback map only.
     * @tparam T Expected type.
     * @param Id UUID to resolve.
     * @return Borrowed pointer to the object, or `nullptr` when missing or type-mismatched.
     */
    template<typename T>
    T* Resolve(const Uuid& Id) const
    {
        if (Id.is_nil())
        {
            return nullptr;
        }
        GameLockGuard Lock(m_mutex);
        return ResolveByIdLocked<T>(Id);
    }

    /**
     * @brief Resolve an object by runtime key and silently fall back to UUID lookup when needed.
     * @tparam T Expected type.
     * @param Id UUID for safety/fallback.
     * @param RuntimePoolToken Runtime pool token.
     * @param RuntimeIndex Runtime slot index.
     * @param RuntimeGeneration Runtime slot generation.
     * @return Borrowed pointer to the object, or `nullptr` when missing or type-mismatched.
     *
     * This overload discards any refreshed runtime identity. Callers that want to
     * rehydrate the handle cache should use `ResolveFastOrFallback()` instead.
     */
    template<typename T>
    T* ResolveFast(const Uuid& Id,
        uint32_t RuntimePoolToken,
        uint32_t RuntimeIndex,
        uint32_t RuntimeGeneration) const
    {
        if (Id.is_nil())
        {
            return nullptr;
        }

        RuntimeIdentity Identity{};
        return ResolveFastOrFallback<T>(
            Id,
            RuntimePoolToken,
            RuntimeIndex,
            RuntimeGeneration,
            &Identity);
    }

    /**
     * @brief Resolve an object by runtime key, then fall back to UUID lookup and report a refreshed runtime identity.
     * @tparam T Expected type.
     * @param Id UUID for fallback path.
     * @param RuntimePoolToken Runtime pool token.
     * @param RuntimeIndex Runtime slot index.
     * @param RuntimeGeneration Runtime slot generation.
     * @param OutIdentity Optional refreshed runtime identity when resolved.
     * @return Borrowed pointer to the object, or `nullptr` when missing or type-mismatched.
     *
     * Semantics:
     * - First tries the runtime slot table.
     * - If that misses, falls back to UUID lookup.
     * - When the fallback succeeds, emits a rate-limited warning to `stderr`.
     * - When `OutIdentity` is non-null and the entry has a runtime identity, the refreshed
     *   identity is written back for handle rehydration.
     *
     * @warning Passing handles by value and then ignoring `OutIdentity` can leave code on
     *          the slow UUID-fallback path indefinitely.
     */
    template<typename T>
    T* ResolveFastOrFallback(const Uuid& Id,
        uint32_t RuntimePoolToken,
        uint32_t RuntimeIndex,
        uint32_t RuntimeGeneration,
        RuntimeIdentity* OutIdentity) const
    {
        if (Id.is_nil())
        {
            if (OutIdentity)
            {
                *OutIdentity = RuntimeIdentity{};
            }
            return nullptr;
        }

        struct WarningInfo
        {
            bool Emit = false;
            Uuid Id{};
            uint64_t Count = 0;
            bool HasRuntimeIdentity = false;
            EObjectKind Kind = EObjectKind::Other;
        };
        WarningInfo Warning{};

        T* Resolved = nullptr;
        RuntimeIdentity ResolvedIdentity{};

        {
            GameLockGuard Lock(m_mutex);
            if (RuntimePoolToken != kInvalidRuntimePoolToken
                && RuntimeIndex != kInvalidRuntimeIndex
                && RuntimePoolToken < m_runtimeSlotsByPool.size())
            {
                const auto& PoolSlots = m_runtimeSlotsByPool[RuntimePoolToken];
                if (RuntimeIndex < PoolSlots.size())
                {
                    const RuntimeSlot& Slot = PoolSlots[RuntimeIndex];
                    if (Slot.Occupied && Slot.Generation == RuntimeGeneration && Slot.Id == Id)
                    {
                        Resolved = ResolveFromRuntimeSlotLocked<T>(Slot);
                        if (Resolved)
                        {
                            ResolvedIdentity.RuntimePoolToken = RuntimePoolToken;
                            ResolvedIdentity.RuntimeIndex = RuntimeIndex;
                            ResolvedIdentity.RuntimeGeneration = RuntimeGeneration;
                        }
                    }
                }
            }

            if (!Resolved)
            {
                auto It = m_entries.find(Id);
                if (It != m_entries.end())
                {
                    const Entry& EntryRef = It->second;
                    Resolved = ResolveFromEntryLocked<T>(EntryRef);
                    if (Resolved)
                    {
                        if (HasRuntimeKey(EntryRef))
                        {
                            ResolvedIdentity.RuntimePoolToken = EntryRef.RuntimePoolToken;
                            ResolvedIdentity.RuntimeIndex = EntryRef.RuntimeIndex;
                            ResolvedIdentity.RuntimeGeneration = EntryRef.RuntimeGeneration;
                        }

                        auto& Count = m_fastPathFallbackCounts[Id];
                        ++Count;
                        if ((Count <= 4u) || ((Count & (Count - 1u)) == 0u))
                        {
                            Warning.Emit = true;
                            Warning.Id = Id;
                            Warning.Count = Count;
                            Warning.HasRuntimeIdentity = HasRuntimeKey(EntryRef);
                            Warning.Kind = EntryRef.Kind;
                        }
                    }
                }
            }
        }

        if (Warning.Emit)
        {
            const char* KindLabel = "object";
            switch (Warning.Kind)
            {
            case EObjectKind::Node:
                KindLabel = "node";
                break;
            case EObjectKind::Component:
                KindLabel = "component";
                break;
            case EObjectKind::Other:
            default:
                KindLabel = "object";
                break;
            }

            std::fprintf(
                stderr,
                "[SnAPI][HandleFallback] Fast runtime-key lookup missed for %s %s; UUID fallback used (%llu hit%s). Runtime identity %s.\n",
                KindLabel,
                ToString(Warning.Id).c_str(),
                static_cast<unsigned long long>(Warning.Count),
                Warning.Count == 1u ? "" : "s",
                Warning.HasRuntimeIdentity ? "available (handle will be rehydrated)" : "not available");
        }

        if (OutIdentity)
        {
            *OutIdentity = ResolvedIdentity;
        }

        return Resolved;
    }

    /** @brief Check whether a UUID resolves to a live object of type `T`. */
    template<typename T>
    bool IsValid(const Uuid& Id) const
    {
        return Resolve<T>(Id) != nullptr;
    }

    /** @brief Check whether runtime-key lookup resolves to a live object of type `T`. */
    template<typename T>
    bool IsValidFast(const Uuid& Id,
        uint32_t RuntimePoolToken,
        uint32_t RuntimeIndex,
        uint32_t RuntimeGeneration) const
    {
        return ResolveFast<T>(Id, RuntimePoolToken, RuntimeIndex, RuntimeGeneration) != nullptr;
    }

private:
    struct Entry
    {
        Uuid Id{};
        EObjectKind Kind = EObjectKind::Other;
        BaseNode* Node = nullptr;
        BaseComponent* Component = nullptr;
        void* Other = nullptr;
        std::type_index Type = std::type_index(typeid(void));
        uint32_t RuntimePoolToken = kInvalidRuntimePoolToken;
        uint32_t RuntimeIndex = kInvalidRuntimeIndex;
        uint32_t RuntimeGeneration = 0;
    };

    struct RuntimeSlot
    {
        Uuid Id{};
        uint32_t Generation = 0;
        EObjectKind Kind = EObjectKind::Other;
        BaseNode* Node = nullptr;
        BaseComponent* Component = nullptr;
        void* Other = nullptr;
        std::type_index Type = std::type_index(typeid(void));
        bool Occupied = false;
    };

    static bool HasRuntimeKey(uint32_t PoolToken, uint32_t Index)
    {
        return PoolToken != kInvalidRuntimePoolToken && Index != kInvalidRuntimeIndex;
    }

    static bool HasRuntimeKey(const Entry& EntryRef)
    {
        return HasRuntimeKey(EntryRef.RuntimePoolToken, EntryRef.RuntimeIndex);
    }

    template<typename T>
    T* ResolveFromEntryLocked(const Entry& EntryRef) const
    {
        if constexpr (std::is_same_v<T, BaseNode>)
        {
            return (EntryRef.Kind == EObjectKind::Node) ? EntryRef.Node : nullptr;
        }
        else if constexpr (std::is_same_v<T, BaseComponent>)
        {
            return (EntryRef.Kind == EObjectKind::Component) ? EntryRef.Component : nullptr;
        }
        else
        {
            if (EntryRef.Kind != EObjectKind::Other || EntryRef.Type != std::type_index(typeid(T)))
            {
                return nullptr;
            }
            return static_cast<T*>(EntryRef.Other);
        }
    }

    template<typename T>
    T* ResolveFromRuntimeSlotLocked(const RuntimeSlot& Slot) const
    {
        if constexpr (std::is_same_v<T, BaseNode>)
        {
            return (Slot.Kind == EObjectKind::Node) ? Slot.Node : nullptr;
        }
        else if constexpr (std::is_same_v<T, BaseComponent>)
        {
            return (Slot.Kind == EObjectKind::Component) ? Slot.Component : nullptr;
        }
        else
        {
            if (Slot.Kind != EObjectKind::Other || Slot.Type != std::type_index(typeid(T)))
            {
                return nullptr;
            }
            return static_cast<T*>(Slot.Other);
        }
    }

    template<typename T>
    T* ResolveByIdLocked(const Uuid& Id) const
    {
        auto It = m_entries.find(Id);
        if (It == m_entries.end())
        {
            return nullptr;
        }
        return ResolveFromEntryLocked<T>(It->second);
    }

    void EnsureRuntimeSlotLocked(uint32_t PoolToken, uint32_t RuntimeIndex)
    {
        if (PoolToken >= m_runtimeSlotsByPool.size())
        {
            m_runtimeSlotsByPool.resize(static_cast<size_t>(PoolToken) + 1u);
        }
        auto& PoolSlots = m_runtimeSlotsByPool[PoolToken];
        if (RuntimeIndex >= PoolSlots.size())
        {
            PoolSlots.resize(static_cast<size_t>(RuntimeIndex) + 1u);
        }
    }

    void BindRuntimeSlotLocked(const Entry& EntryRef)
    {
        if (!HasRuntimeKey(EntryRef))
        {
            return;
        }

        EnsureRuntimeSlotLocked(EntryRef.RuntimePoolToken, EntryRef.RuntimeIndex);
        RuntimeSlot& Slot = m_runtimeSlotsByPool[EntryRef.RuntimePoolToken][EntryRef.RuntimeIndex];
        Slot.Id = EntryRef.Id;
        Slot.Generation = EntryRef.RuntimeGeneration;
        Slot.Kind = EntryRef.Kind;
        Slot.Node = EntryRef.Node;
        Slot.Component = EntryRef.Component;
        Slot.Other = EntryRef.Other;
        Slot.Type = EntryRef.Type;
        Slot.Occupied = true;
    }

    void ClearRuntimeSlotLocked(const Entry& EntryRef)
    {
        if (!HasRuntimeKey(EntryRef))
        {
            return;
        }
        if (EntryRef.RuntimePoolToken >= m_runtimeSlotsByPool.size())
        {
            return;
        }

        auto& PoolSlots = m_runtimeSlotsByPool[EntryRef.RuntimePoolToken];
        if (EntryRef.RuntimeIndex >= PoolSlots.size())
        {
            return;
        }

        RuntimeSlot& Slot = PoolSlots[EntryRef.RuntimeIndex];
        if (Slot.Occupied && Slot.Generation == EntryRef.RuntimeGeneration)
        {
            Slot = RuntimeSlot{};
        }
    }

    void RegisterInternal(const Uuid& Id,
        EObjectKind Kind,
        BaseNode* Node,
        BaseComponent* Component,
        void* Other,
        std::type_index Type,
        uint32_t RuntimePoolToken,
        uint32_t RuntimeIndex,
        uint32_t RuntimeGeneration)
    {
        if (Id.is_nil())
        {
            DEBUG_ASSERT(false, "Cannot register nil uuid");
            return;
        }

        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Id);
        if (It != m_entries.end())
        {
            ClearRuntimeSlotLocked(It->second);
            It->second = Entry{
                Id,
                Kind,
                Node,
                Component,
                Other,
                Type,
                RuntimePoolToken,
                RuntimeIndex,
                RuntimeGeneration
            };
            BindRuntimeSlotLocked(It->second);
            return;
        }

        auto Inserted = m_entries.emplace(Id,
            Entry{
                Id,
                Kind,
                Node,
                Component,
                Other,
                Type,
                RuntimePoolToken,
                RuntimeIndex,
                RuntimeGeneration
            });
        if (Inserted.second)
        {
            BindRuntimeSlotLocked(Inserted.first->second);
        }
    }

    mutable GameMutex m_mutex{}; /**< @brief Protects registry state. */
    std::unordered_map<Uuid, Entry, UuidHash> m_entries{}; /**< @brief UUID -> entry map (fallback path). */
    std::vector<std::vector<RuntimeSlot>> m_runtimeSlotsByPool{{}}; /**< @brief Runtime pool token -> runtime slots (fast path). */
    mutable std::unordered_map<Uuid, uint64_t, UuidHash> m_fastPathFallbackCounts{}; /**< @brief Per-object fast-path miss counters for fallback diagnostics. */
};

} // namespace SnAPI::GameFramework
