#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include "GameThreading.h"
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "BaseComponent.h"
#include "ObjectPool.h"
#include "ObjectRegistry.h"
#include "Profiling.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class TypeRegistry;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Global allocator for compact component-type bit indices.
 *
 * `ComponentTypeRegistry` turns arbitrary reflected component `TypeId`s into dense bit
 * positions used by masks and query acceleration structures. The assigned index for a
 * type remains stable for the lifetime of the process.
 *
 * Threading:
 * - Not generally thread-safe.
 * - Internal `GameMutex` use validates affinity only.
 */
class ComponentTypeRegistry
{
public:
    /**
     * @brief Get the existing bit index for a component type, or assign a new one.
     * @param Id Component type id.
     * @return Bit index for the type.
     * @remarks `Version()` is incremented only when a previously unseen type is added.
     */
    static uint32_t TypeIndex(const TypeId& Id)
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_typeToIndex.find(Id);
        if (It != m_typeToIndex.end())
        {
            return It->second;
        }
        uint32_t Index = static_cast<uint32_t>(m_typeToIndex.size());
        m_typeToIndex.emplace(Id, Index);
        ++m_version;
        return Index;
    }

    /**
     * @brief Get the current mutation version of the registry.
     * @return Version counter.
     * @remarks Useful for invalidating cached masks sized from `WordCount()`.
     */
    static uint32_t Version()
    {
        GameLockGuard Lock(m_mutex);
        return m_version;
    }

    /** @brief Get the number of 64-bit words needed to represent the current type set. */
    static size_t WordCount()
    {
        GameLockGuard Lock(m_mutex);
        size_t BitCount = m_typeToIndex.size();
        return (BitCount + 63u) / 64u;
    }

private:
    static inline GameMutex m_mutex{}; /**< @brief Protects registry state. */
    static inline std::unordered_map<TypeId, uint32_t, UuidHash> m_typeToIndex{}; /**< @brief TypeId -> bit index. */
    static inline uint32_t m_version = 0; /**< @brief Version counter. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Type-erased storage interface for one component type.
 *
 * `ComponentStorageView` is the cold-path abstraction used when world/level code needs
 * to work with "a component storage" without statically knowing `T`. Concrete hot-path
 * iteration still happens in `TComponentStorage<T>`.
 *
 * Ownership and lifetime:
 * - The storage owns component instances.
 * - Borrowed pointers returned from `Borrowed()` remain valid only until that component
 *   is removed, the storage reaches `EndFrame()`, or `Clear()` is called.
 *
 * Threading:
 * - Main-thread only unless an outer system guarantees exclusive access.
 *
 * @note Handle parameters are `const&` by design. Handle resolution may refresh
 * runtime-key fields on the caller-owned handle instance; passing by value would
 * drop that refresh and can force repeated UUID fallback lookups.
 */
class ComponentStorageView
{
public:
    /**
     * @brief Node-activity callback signature used by storage-driven ticking.
     * @param UserData Opaque pointer provided by caller.
     * @param Node Owner node candidate.
     * @return True when the node should execute component tick hooks.
     */
    using NodeActivePredicate = bool(*)(void* UserData, const BaseNode& Node);

    /** @brief Virtual destructor. */
    virtual ~ComponentStorageView() = default;
    /**
     * @brief Get the component type id stored by this storage.
     * @return TypeId value.
     */
    virtual TypeId TypeKey() const = 0;
    /**
     * @brief Check if a node has this component.
     * @param Owner Node handle.
     * @return True if the component exists.
     */
    virtual bool Has(const NodeHandle& Owner) const = 0;
    /**
     * @brief Remove a component from a node.
     * @param Owner Node handle.
     * @remarks Removal is deferred until EndFrame.
     */
    virtual void Remove(const NodeHandle& Owner) = 0;
    /**
     * @brief Tick a component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void TickComponent(const NodeHandle& Owner, float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick a component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTickComponent(const NodeHandle& Owner, float DeltaSeconds) = 0;
    /**
     * @brief Late tick a component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTickComponent(const NodeHandle& Owner, float DeltaSeconds) = 0;
    /**
     * @brief Tick all stored components in dense storage order.
     * @param NodeIsActive Callback used to gate owner-node activity/relevance.
     * @param UserData Opaque callback context.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void TickAll(NodeActivePredicate NodeIsActive, void* UserData, float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick all stored components in dense storage order.
     * @param NodeIsActive Callback used to gate owner-node activity/relevance.
     * @param UserData Opaque callback context.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTickAll(NodeActivePredicate NodeIsActive, void* UserData, float DeltaSeconds) = 0;
    /**
     * @brief Late tick all stored components in dense storage order.
     * @param NodeIsActive Callback used to gate owner-node activity/relevance.
     * @param UserData Opaque callback context.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTickAll(NodeActivePredicate NodeIsActive, void* UserData, float DeltaSeconds) = 0;
    /**
     * @brief Borrow a component instance (mutable).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     * @note Borrowed pointers must not be cached.
     */
    virtual void* Borrowed(const NodeHandle& Owner) = 0;
    /**
     * @brief Borrow a component instance (const).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     */
    virtual const void* Borrowed(const NodeHandle& Owner) const = 0;
    /**
     * @brief Process pending destruction at end-of-frame.
     */
    virtual void EndFrame() = 0;
    /**
     * @brief Clear all components immediately.
     */
    virtual void Clear() = 0;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Dense one-component-per-node storage for a specific component type.
 * @tparam T Component type.
 *
 * This storage is the bridge between object-like component lifetime and data-oriented
 * ticking. It maintains:
 * - a deferred-destroy object pool for the component instances
 * - an owner UUID map for slow-path lookup
 * - a sparse runtime-owner map for fast-path lookup
 * - a dense linear array for cache-friendly iteration
 *
 * Core semantics:
 * - A node may own at most one `T`.
 * - `Add*()` immediately inserts into the dense set and object registry.
 * - `Remove()` detaches the component from the owner immediately, but physical
 *   destruction and `OnDestroy()` are deferred until `EndFrame()`.
 * - Dense order is unstable; removals use swap-pop compaction.
 *
 * Threading:
 * - Main-thread only.
 *
 * @see ComponentStorageView
 * @see TObjectPool
 */
template<typename T>
class TComponentStorage final : public ComponentStorageView
{
public:
    static_assert(std::is_base_of_v<BaseComponent, T>, "Components must derive from BaseComponent");

    /**
     * @brief Get the component type id.
     * @return TypeId value.
     */
    TypeId TypeKey() const override
    {
        return m_typeId;
    }

    /** @brief Add a default-constructed component with a generated UUID. */
    TExpectedRef<T> Add(const NodeHandle& Owner)
    {
        return AddWithId(Owner, NewUuid());
    }

    /**
     * @brief Add a component constructed from caller-provided arguments.
     * @param Owner Owner node handle.
     * @param args Constructor arguments.
     * @return Borrowed reference to the attached component, or an error on failure.
     */
    template<typename... Args>
    TExpectedRef<T> Add(const NodeHandle& Owner, Args&&... args)
    {
        return AddWithId(Owner, NewUuid(), std::forward<Args>(args)...);
    }

    /**
     * @brief Add a component under an explicit UUID.
     * @param Owner Owner node handle.
     * @param Id Component UUID.
     * @param args Constructor arguments.
     * @return Borrowed reference to the attached component, or an error on failure.
     *
     * Semantics:
     * - Fails when the owner already has a `T`.
     * - Sets owner/id/runtime identity/type key fields on the component.
     * - Registers the component in `ObjectRegistry`.
     * - Invokes `OnCreate()` immediately unless component `OnCreate` is currently
     *   suppressed by `ScopedComponentOnCreateSuppression`.
     *
     * This is the entry point used by deserialization and replication paths that need
     * identity continuity rather than a fresh UUID.
     */
    template<typename... Args>
    TExpectedRef<T> AddWithId(const NodeHandle& Owner, const Uuid& Id, Args&&... args)
    {
        if (Has(Owner))
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Component already exists on node"));
        }
        auto HandleResult = m_pool.template CreateWithId<T>(Id, std::forward<Args>(args)...);
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }
        auto Handle = HandleResult.value();
        auto* Component = m_pool.Borrowed(Handle);
        if (!Component)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Component creation failed"));
        }
        Component->Owner(Owner);
        Component->Id(Id);
        Component->RuntimeIdentity(Handle.RuntimePoolToken, Handle.RuntimeIndex, Handle.RuntimeGeneration);
        Component->TypeKey(StaticTypeId<T>());
        ObjectRegistry::Instance().RegisterComponent(
            Id,
            Component,
            Handle.RuntimePoolToken,
            Handle.RuntimeIndex,
            Handle.RuntimeGeneration);
        const std::size_t DenseIndex = m_dense.size();
        m_ownerToDense.emplace(Owner.Id, DenseIndex);
        m_dense.push_back(ComponentEntry{Owner, Owner.Borrowed(), Id, Component});
        SetSparseOwnerIndex(Owner, DenseIndex);
        if (!IsComponentOnCreateSuppressed())
        {
            Component->OnCreate();
        }
        return *Component;
    }

    /**
     * @brief Resolve the component currently attached to an owner node.
     * @param Owner Owner node handle.
     * @return Borrowed reference to the component, or `NotFound` when the owner does not
     *         currently have this component.
     */
    TExpectedRef<T> Component(const NodeHandle& Owner)
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Component not found"));
        }
        if (DenseIndex >= m_dense.size())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Component index is out of range"));
        }

        auto& Entry = m_dense[DenseIndex];
        auto* Component = ResolveComponent(Entry);
        if (!Component)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Component missing"));
        }
        return *Component;
    }

    /**
     * @brief Check if a node has this component.
     * @param Owner Node handle.
     * @return True if present.
     */
    bool Has(const NodeHandle& Owner) const override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        return ResolveDenseIndex(Owner, DenseIndex) && DenseIndex < m_dense.size();
    }

    /**
     * @brief Detach a component from its owner and schedule it for deferred destruction.
     * @param Owner Node handle.
     *
     * Semantics:
     * - Owner lookup tables are updated immediately.
     * - Dense storage is compacted immediately with swap-pop.
     * - The component instance remains alive until `EndFrame()`.
     * - `OnDestroy()` and `ObjectRegistry` unregistration happen during `EndFrame()`.
     */
    void Remove(const NodeHandle& Owner) override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex))
        {
            return;
        }

        if (DenseIndex >= m_dense.size())
        {
            m_ownerToDense.erase(Owner.Id);
            ClearSparseOwnerIndex(Owner);
            return;
        }

        ComponentEntry RemovedEntry = m_dense[DenseIndex];
        const Uuid Id = RemovedEntry.Id;
        m_ownerToDense.erase(RemovedEntry.Owner.Id);
        ClearSparseOwnerIndex(RemovedEntry.Owner);

        if (DenseIndex + 1 < m_dense.size())
        {
            m_dense[DenseIndex] = std::move(m_dense.back());
            m_ownerToDense[m_dense[DenseIndex].Owner.Id] = DenseIndex;
            SetSparseOwnerIndex(m_dense[DenseIndex].Owner, DenseIndex);
        }
        m_dense.pop_back();

        if (m_pool.DestroyLater(Id))
        {
            m_pendingDestroy.push_back(PendingDestroyEntry{Id, RemovedEntry.Component});
        }
    }

    /**
     * @brief Tick the component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    void TickComponent(const NodeHandle& Owner, float DeltaSeconds) override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex) || DenseIndex >= m_dense.size())
        {
            return;
        }

        auto* Component = ResolveComponent(m_dense[DenseIndex]);
        if (Component && Component->Active())
        {
            Component->Tick(DeltaSeconds);
        }
    }

    /**
     * @brief Fixed-step tick the component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTickComponent(const NodeHandle& Owner, float DeltaSeconds) override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex) || DenseIndex >= m_dense.size())
        {
            return;
        }
        T* Component = ResolveComponent(m_dense[DenseIndex]);
        if (!Component)
        {
            return;
        }
        if (!Component->Active())
        {
            return;
        }
        Component->FixedTick(DeltaSeconds);
    }

    /**
     * @brief Late tick the component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTickComponent(const NodeHandle& Owner, float DeltaSeconds) override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex) || DenseIndex >= m_dense.size())
        {
            return;
        }

        auto* Component = ResolveComponent(m_dense[DenseIndex]);
        if (Component && Component->Active())
        {
            Component->LateTick(DeltaSeconds);
        }
    }

    /**
     * @brief Tick all active components in dense storage order.
     * @param NodeIsActive Owner-node activity predicate.
     * @param UserData Opaque predicate context.
     * @param DeltaSeconds Time since last tick.
     *
     * The owner node is also gated through `NodeIsActive` when that callback is
     * provided. Owner-node pointers are lazily cached per dense entry.
     */
    void TickAll(NodeActivePredicate NodeIsActive, void* UserData, float DeltaSeconds) override
    {
        for (auto& Entry : m_dense)
        {
            T* Component = ResolveComponent(Entry);
            if (!Component || !Component->Active())
            {
                continue;
            }

            BaseNode* OwnerNode = Entry.OwnerNode;
            if (!OwnerNode)
            {
                OwnerNode = Entry.Owner.Borrowed();
                Entry.OwnerNode = OwnerNode;
            }
            if (!OwnerNode)
            {
                continue;
            }
            if (NodeIsActive && !NodeIsActive(UserData, *OwnerNode))
            {
                continue;
            }

            Component->Tick(DeltaSeconds);
        }
    }

    /**
     * @brief Fixed-step tick all components in dense storage order.
     * @param NodeIsActive Owner-node activity predicate.
     * @param UserData Opaque predicate context.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTickAll(NodeActivePredicate NodeIsActive, void* UserData, float DeltaSeconds) override
    {
        for (auto& Entry : m_dense)
        {
            T* Component = ResolveComponent(Entry);
            if (!Component || !Component->Active())
            {
                continue;
            }

            BaseNode* OwnerNode = Entry.OwnerNode;
            if (!OwnerNode)
            {
                OwnerNode = Entry.Owner.Borrowed();
                Entry.OwnerNode = OwnerNode;
            }
            if (!OwnerNode)
            {
                continue;
            }
            if (NodeIsActive && !NodeIsActive(UserData, *OwnerNode))
            {
                continue;
            }

            Component->FixedTick(DeltaSeconds);
        }
    }

    /**
     * @brief Late tick all components in dense storage order.
     * @param NodeIsActive Owner-node activity predicate.
     * @param UserData Opaque predicate context.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTickAll(NodeActivePredicate NodeIsActive, void* UserData, float DeltaSeconds) override
    {
        for (auto& Entry : m_dense)
        {
            T* Component = ResolveComponent(Entry);
            if (!Component || !Component->Active())
            {
                continue;
            }

            BaseNode* OwnerNode = Entry.OwnerNode;
            if (!OwnerNode)
            {
                OwnerNode = Entry.Owner.Borrowed();
                Entry.OwnerNode = OwnerNode;
            }
            if (!OwnerNode)
            {
                continue;
            }
            if (NodeIsActive && !NodeIsActive(UserData, *OwnerNode))
            {
                continue;
            }

            Component->LateTick(DeltaSeconds);
        }
    }

    /**
     * @brief Borrow the attached component instance.
     * @param Owner Node handle.
     * @return Non-owning component pointer, or `nullptr` when the owner has no `T`.
     * @warning Borrowed pointers must not be cached across removal, `EndFrame()`, or `Clear()`.
     */
    void* Borrowed(const NodeHandle& Owner) override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex) || DenseIndex >= m_dense.size())
        {
            return nullptr;
        }
        return ResolveComponent(m_dense[DenseIndex]);
    }

    /**
     * @brief Borrow the component instance (const).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     */
    const void* Borrowed(const NodeHandle& Owner) const override
    {
        std::size_t DenseIndex = kInvalidDenseIndex;
        if (!ResolveDenseIndex(Owner, DenseIndex) || DenseIndex >= m_dense.size())
        {
            return nullptr;
        }
        return ResolveComponent(m_dense[DenseIndex]);
    }

    /**
     * @brief Finalize all removals that were deferred earlier in the frame.
     * @remarks
     * Destruction order matches the order components were queued in `m_pendingDestroy`.
     * `OnDestroy()` runs before `ObjectRegistry` unregistration and before the pool drops
     * the underlying object.
     */
    void EndFrame() override
    {
        for (const auto& Pending : m_pendingDestroy)
        {
            if (Pending.Component)
            {
                Pending.Component->OnDestroy();
            }
        }

        for (const auto& Pending : m_pendingDestroy)
        {
            ObjectRegistry::Instance().Unregister(Pending.Id);
        }
        m_pendingDestroy.clear();
        m_pool.EndFrame();
    }

    /**
     * @brief Destroy every stored component immediately and reset the storage to empty.
     * @remarks
     * This bypasses deferred-destroy semantics and invalidates all borrowed component
     * pointers immediately.
     */
    void Clear() override
    {
        m_pool.ForEachAll([&](const THandle<T>& Handle, T& Component) {
            Component.OnDestroy();
            ObjectRegistry::Instance().Unregister(Handle.Id);
        });
        m_ownerToDense.clear();
        m_sparseOwnerToDense.clear();
        m_sparseOwnerGeneration.clear();
        m_dense.clear();
        m_pendingDestroy.clear();
        m_pool.Clear();
    }

    /** @brief Get the current dense entry count. */
    std::size_t DenseSize() const
    {
        return m_dense.size();
    }

    /**
     * @brief Read the owner handle stored at a dense index.
     * @param Index Dense index.
     * @return Copy of the stored owner handle, or a null handle when out of range.
     * @warning Dense indices are not stable across removals.
     */
    NodeHandle DenseOwner(std::size_t Index) const
    {
        if (Index >= m_dense.size())
        {
            return NodeHandle{};
        }
        return m_dense[Index].Owner;
    }

    /**
     * @brief Borrow the component pointer stored at a dense index.
     * @param Index Dense index.
     * @return Non-owning component pointer or `nullptr` when out of range.
     * @warning Dense indices are unstable and should not be persisted.
     */
    T* DenseComponent(std::size_t Index)
    {
        if (Index >= m_dense.size())
        {
            return nullptr;
        }
        return ResolveComponent(m_dense[Index]);
    }

private:
    static constexpr std::size_t kInvalidDenseIndex = std::numeric_limits<std::size_t>::max();

    struct ComponentEntry
    {
        NodeHandle Owner{};
        BaseNode* OwnerNode = nullptr;
        Uuid Id{};
        T* Component = nullptr;
    };

    struct PendingDestroyEntry
    {
        Uuid Id{};
        T* Component = nullptr;
    };

    bool ResolveDenseIndex(const NodeHandle& Owner, std::size_t& OutIndex) const
    {
        if (TryResolveDenseIndexFromSparse(Owner, OutIndex))
        {
            return true;
        }

        return TryResolveDenseIndexFromOwnerId(Owner, OutIndex);
    }

    bool ResolveDenseIndex(const NodeHandle& Owner, std::size_t& OutIndex)
    {
        if (TryResolveDenseIndexFromSparse(Owner, OutIndex))
        {
            return true;
        }

        if (!TryResolveDenseIndexFromOwnerId(Owner, OutIndex))
        {
            return false;
        }

        RehydrateOwnerRuntimeIdentity(Owner, OutIndex);
        return true;
    }

    bool TryResolveDenseIndexFromSparse(const NodeHandle& Owner, std::size_t& OutIndex) const
    {
        if (!Owner.HasRuntimeKey())
        {
            return false;
        }

        const std::size_t SparseIndex = static_cast<std::size_t>(Owner.RuntimeIndex);
        if (SparseIndex >= m_sparseOwnerToDense.size())
        {
            return false;
        }

        const std::size_t DenseIndex = m_sparseOwnerToDense[SparseIndex];
        if (DenseIndex == kInvalidDenseIndex || DenseIndex >= m_dense.size())
        {
            return false;
        }

        const auto& Entry = m_dense[DenseIndex];
        if (Entry.Owner.Id != Owner.Id)
        {
            return false;
        }
        if (SparseIndex < m_sparseOwnerGeneration.size()
            && m_sparseOwnerGeneration[SparseIndex] != Owner.RuntimeGeneration)
        {
            return false;
        }

        OutIndex = DenseIndex;
        return true;
    }

    bool TryResolveDenseIndexFromOwnerId(const NodeHandle& Owner, std::size_t& OutIndex) const
    {
        if (Owner.Id.is_nil())
        {
            return false;
        }

        const auto It = m_ownerToDense.find(Owner.Id);
        if (It == m_ownerToDense.end())
        {
            return false;
        }

        const std::size_t DenseIndex = It->second;
        if (DenseIndex >= m_dense.size())
        {
            return false;
        }

        const auto& Entry = m_dense[DenseIndex];
        if (Entry.Owner.Id != Owner.Id)
        {
            return false;
        }

        OutIndex = DenseIndex;
        return true;
    }

    void RehydrateOwnerRuntimeIdentity(const NodeHandle& LookupOwner, std::size_t DenseIndex)
    {
        if (DenseIndex >= m_dense.size())
        {
            return;
        }

        ComponentEntry& Entry = m_dense[DenseIndex];
        if (Entry.Owner.Id.is_nil())
        {
            return;
        }

        if (Entry.Owner.HasRuntimeKey())
        {
            SetSparseOwnerIndex(Entry.Owner, DenseIndex);
            LookupOwner.RuntimePoolToken = Entry.Owner.RuntimePoolToken;
            LookupOwner.RuntimeIndex = Entry.Owner.RuntimeIndex;
            LookupOwner.RuntimeGeneration = Entry.Owner.RuntimeGeneration;
            if (!Entry.OwnerNode)
            {
                Entry.OwnerNode = Entry.Owner.Borrowed();
            }
            return;
        }

        ObjectRegistry::RuntimeIdentity Identity{};
        BaseNode* OwnerNode = ObjectRegistry::Instance().ResolveFastOrFallback<BaseNode>(
            Entry.Owner.Id,
            Entry.Owner.RuntimePoolToken,
            Entry.Owner.RuntimeIndex,
            Entry.Owner.RuntimeGeneration,
            &Identity);
        if (OwnerNode)
        {
            Entry.OwnerNode = OwnerNode;
        }

        if (Identity.RuntimePoolToken == ObjectRegistry::kInvalidRuntimePoolToken
            || Identity.RuntimeIndex == ObjectRegistry::kInvalidRuntimeIndex)
        {
            return;
        }

        Entry.Owner.RuntimePoolToken = Identity.RuntimePoolToken;
        Entry.Owner.RuntimeIndex = Identity.RuntimeIndex;
        Entry.Owner.RuntimeGeneration = Identity.RuntimeGeneration;
        SetSparseOwnerIndex(Entry.Owner, DenseIndex);

        LookupOwner.RuntimePoolToken = Identity.RuntimePoolToken;
        LookupOwner.RuntimeIndex = Identity.RuntimeIndex;
        LookupOwner.RuntimeGeneration = Identity.RuntimeGeneration;
    }

    void SetSparseOwnerIndex(const NodeHandle& Owner, std::size_t DenseIndex)
    {
        if (!Owner.HasRuntimeKey())
        {
            return;
        }

        const std::size_t SparseIndex = static_cast<std::size_t>(Owner.RuntimeIndex);
        if (SparseIndex >= m_sparseOwnerToDense.size())
        {
            m_sparseOwnerToDense.resize(SparseIndex + 1, kInvalidDenseIndex);
            m_sparseOwnerGeneration.resize(SparseIndex + 1, 0);
        }

        m_sparseOwnerToDense[SparseIndex] = DenseIndex;
        m_sparseOwnerGeneration[SparseIndex] = Owner.RuntimeGeneration;
    }

    void ClearSparseOwnerIndex(const NodeHandle& Owner)
    {
        if (!Owner.HasRuntimeKey())
        {
            return;
        }

        const std::size_t SparseIndex = static_cast<std::size_t>(Owner.RuntimeIndex);
        if (SparseIndex >= m_sparseOwnerToDense.size())
        {
            return;
        }
        m_sparseOwnerToDense[SparseIndex] = kInvalidDenseIndex;
        if (SparseIndex < m_sparseOwnerGeneration.size())
        {
            m_sparseOwnerGeneration[SparseIndex] = 0;
        }
    }

    T* ResolveComponent(ComponentEntry& Entry)
    {
        return Entry.Component;
    }

    const T* ResolveComponent(const ComponentEntry& Entry) const
    {
        return Entry.Component;
    }

    TypeId m_typeId = StaticTypeId<T>(); /**< @brief Reflected type id for this storage specialization. */
    TObjectPool<T> m_pool{}; /**< @brief Underlying component object pool with deferred destroy support. */
    std::unordered_map<Uuid, std::size_t, UuidHash> m_ownerToDense{}; /**< @brief Owner-node UUID -> dense-entry index. */
    std::vector<std::size_t> m_sparseOwnerToDense{}; /**< @brief Runtime owner slot index -> dense-entry index (sparse-set style fast path). */
    std::vector<uint32_t> m_sparseOwnerGeneration{}; /**< @brief Generation mirror for sparse owner slots to reject stale handles. */
    std::vector<ComponentEntry> m_dense{}; /**< @brief Dense component entries for cache-friendly linear traversal. */
    std::vector<PendingDestroyEntry> m_pendingDestroy{}; /**< @brief Components scheduled for end-of-frame destroy flush. */
};

} // namespace SnAPI::GameFramework
