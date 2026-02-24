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
class IComponent;

/**
 * @brief Kind of object stored in the registry.
 * @remarks Node and Component are handled specially for handle resolution.
 */
enum class EObjectKind
{
    Node,      /**< @brief BaseNode-derived object. */
    Component, /**< @brief IComponent-derived object. */
    Other      /**< @brief Arbitrary registered type. */
};

/**
 * @brief Global registry mapping UUIDs to live object pointers.
 * @remarks Used by `THandle` to resolve runtime handles to live objects.
 * @note Objects must be registered/unregistered by their owning systems.
 * @note Registry stores non-owning pointers; lifetime is managed externally.
 */
class ObjectRegistry
{
public:
    /** @brief Runtime pool token sentinel meaning "no runtime pool". */
    static constexpr uint32_t kInvalidRuntimePoolToken = 0;
    /** @brief Runtime slot index sentinel meaning "no runtime slot". */
    static constexpr uint32_t kInvalidRuntimeIndex = std::numeric_limits<uint32_t>::max();

    /**
     * @brief Runtime-key identity tuple used to rehydrate handles after UUID fallback.
     */
    struct RuntimeIdentity
    {
        uint32_t RuntimePoolToken = kInvalidRuntimePoolToken;
        uint32_t RuntimeIndex = kInvalidRuntimeIndex;
        uint32_t RuntimeGeneration = 0;
    };

    /**
     * @brief Access the singleton registry instance.
     * @return Reference to the registry.
     */
    static ObjectRegistry& Instance()
    {
        static ObjectRegistry Registry;
        return Registry;
    }

    /**
     * @brief Acquire a unique runtime pool token.
     * @return Token used to bind runtime index lookups for a pool instance.
     * @remarks
     * Tokens are never reused to avoid stale-handle aliasing between destroyed and
     * newly-created pools.
     */
    uint32_t AcquireRuntimePoolToken()
    {
        GameLockGuard Lock(m_mutex);
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
     * @brief Release a runtime pool token.
     * @param PoolToken Token to release.
     * @remarks
     * Release is a cleanup signal only. Tokens are not reused and stale handles
     * stay invalid forever.
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
     * @brief Register a node with UUID-only lookup.
     * @param Id UUID of the node.
     * @param Node Pointer to the node.
     * @remarks Overwrites any existing entry with the same UUID.
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
     * @brief Register a node with runtime-key lookup.
     * @param Id UUID of the node.
     * @param Node Pointer to the node.
     * @param RuntimePoolToken Runtime pool token from owning pool.
     * @param RuntimeIndex Runtime slot index in owning pool.
     * @param RuntimeGeneration Runtime slot generation in owning pool.
     * @remarks Enables direct runtime-key resolution without UUID hash lookup.
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

    /**
     * @brief Register a component with UUID-only lookup.
     * @param Id UUID of the component.
     * @param Component Pointer to the component.
     */
    void RegisterComponent(const Uuid& Id, IComponent* Component)
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
     * @brief Register a component with runtime-key lookup.
     * @param Id UUID of the component.
     * @param Component Pointer to the component.
     * @param RuntimePoolToken Runtime pool token from owning pool.
     * @param RuntimeIndex Runtime slot index in owning pool.
     * @param RuntimeGeneration Runtime slot generation in owning pool.
     */
    void RegisterComponent(const Uuid& Id,
        IComponent* Component,
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
     * @brief Register an arbitrary object with UUID-only lookup.
     * @tparam T Object type.
     * @param Id UUID of the object.
     * @param Object Pointer to object.
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
     * @brief Unregister an object by UUID.
     * @param Id UUID to remove.
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
     * @brief Resolve UUID-only lookup path.
     * @tparam T Expected type.
     * @param Id UUID to resolve.
     * @return Pointer to object, or nullptr when missing/type mismatch.
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
     * @brief Resolve runtime-key fast path, with UUID fallback.
     * @tparam T Expected type.
     * @param Id UUID for safety/fallback.
     * @param RuntimePoolToken Runtime pool token.
     * @param RuntimeIndex Runtime slot index.
     * @param RuntimeGeneration Runtime slot generation.
     * @return Pointer to object, or nullptr when missing/type mismatch.
     * @remarks
     * Fast path avoids UUID hashing entirely when runtime key is valid and hot.
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
     * @brief Resolve runtime-key fast path with UUID fallback and runtime identity refresh.
     * @tparam T Expected type.
     * @param Id UUID for fallback path.
     * @param RuntimePoolToken Runtime pool token.
     * @param RuntimeIndex Runtime slot index.
     * @param RuntimeGeneration Runtime slot generation.
     * @param OutIdentity Optional refreshed runtime identity when resolved.
     * @return Pointer to object, or nullptr when missing/type mismatch.
     * @remarks
     * Fallback is intentionally available for runtime migrations/rehydration boundaries.
     * When fallback is used, a warning is emitted (rate-limited by per-object hit count).
     * Callers should persist `OutIdentity` back into the same handle instance; passing
     * handles by value prevents cache refresh and can trigger repeated fallback.
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

    /**
     * @brief Check whether UUID-only lookup resolves.
     * @tparam T Expected type.
     * @param Id UUID to check.
     * @return True when object resolves.
     */
    template<typename T>
    bool IsValid(const Uuid& Id) const
    {
        return Resolve<T>(Id) != nullptr;
    }

    /**
     * @brief Check whether runtime-key lookup resolves.
     * @tparam T Expected type.
     * @param Id UUID for safety/fallback.
     * @param RuntimePoolToken Runtime pool token.
     * @param RuntimeIndex Runtime slot index.
     * @param RuntimeGeneration Runtime slot generation.
     * @return True when object resolves.
     */
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
        IComponent* Component = nullptr;
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
        IComponent* Component = nullptr;
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
        else if constexpr (std::is_same_v<T, IComponent>)
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
        else if constexpr (std::is_same_v<T, IComponent>)
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
        IComponent* Component,
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
