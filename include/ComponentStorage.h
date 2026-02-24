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
#include "IComponent.h"
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
 * @brief Global registry for component type indices and masks.
 * @remarks Provides stable bit positions for fast component queries.
 */
class ComponentTypeRegistry
{
public:
    /**
     * @brief Get or assign a bit index for a component type.
     * @param Id Component type id.
     * @return Bit index for the type.
     * @remarks Increments the version when a new type is added.
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
     * @brief Get the current registry version.
     * @return Version counter.
     * @remarks Incremented when new types are registered.
     */
    static uint32_t Version()
    {
        GameLockGuard Lock(m_mutex);
        return m_version;
    }

    /**
     * @brief Get the number of 64-bit words required for the mask.
     * @return Word count for the current type set.
     */
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
 * @brief Type-erased interface for component storage.
 * @remarks NodeGraph uses this to manage components generically.
 * @note Handle parameters are `const&` by design. Handle resolution may refresh
 * runtime-key fields on the caller-owned handle instance; passing by value would
 * drop that refresh and can force repeated UUID fallback lookups.
 */
class IComponentStorage
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
    virtual ~IComponentStorage() = default;
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
 * @brief Typed component storage for a specific component type.
 * @tparam T Component type.
 * @remarks
 * Maintains one-component-per-owner invariant for type `T` and coordinates:
 * - pool allocation/deferred destroy
 * - owner-node to component-id indexing
 * - object registry registration/unregistration
 * - lifecycle callbacks (`OnCreate`/`OnDestroy`)
 */
template<typename T>
class TComponentStorage final : public IComponentStorage
{
public:
    static_assert(std::is_base_of_v<IComponent, T>, "Components must derive from IComponent");

    /**
     * @brief Get the component type id.
     * @return TypeId value.
     */
    TypeId TypeKey() const override
    {
        return m_typeId;
    }

    /**
     * @brief Add a component with a generated UUID.
     * @param Owner Owner node handle.
     * @return Reference wrapper or error.
     */
    TExpectedRef<T> Add(NodeHandle Owner)
    {
        return AddWithId(Owner, NewUuid());
    }

    /**
     * @brief Add a component with constructor arguments.
     * @param Owner Owner node handle.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     */
    template<typename... Args>
    TExpectedRef<T> Add(NodeHandle Owner, Args&&... args)
    {
        return AddWithId(Owner, NewUuid(), std::forward<Args>(args)...);
    }

    /**
     * @brief Add a component with an explicit UUID.
     * @param Owner Owner node handle.
     * @param Id Component UUID.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     * @remarks Used by deserialization/replication restore paths to preserve identity continuity.
     */
    template<typename... Args>
    TExpectedRef<T> AddWithId(NodeHandle Owner, const Uuid& Id, Args&&... args)
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
        Component->OnCreate();
        return *Component;
    }

    /**
     * @brief Get a component by owner.
     * @param Owner Owner node handle.
     * @return Reference wrapper or error.
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
     * @brief Remove a component from a node.
     * @param Owner Node handle.
     * @remarks
     * Index mapping is removed immediately; physical destruction and OnDestroy happen during EndFrame.
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
     * @brief Tick all components in dense storage order.
     * @param NodeIsActive Owner-node activity predicate.
     * @param UserData Opaque predicate context.
     * @param DeltaSeconds Time since last tick.
     * @remarks
     * This is the hot-path runtime update mode: dense linear traversal with
     * no per-owner hash lookup once entries are built.
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
     * @brief Borrow the component instance (mutable).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
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
     * @brief Process pending destruction at end-of-frame.
     * @remarks Calls OnDestroy and unregisters components.
     * @note Ordering is deterministic by pending queue insertion order.
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
     * @brief Clear all components immediately.
     * @remarks Calls OnDestroy and clears internal mappings.
     * @note Immediate path bypasses deferred destroy semantics.
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

    /**
     * @brief Number of dense entries currently stored.
     */
    std::size_t DenseSize() const
    {
        return m_dense.size();
    }

    /**
     * @brief Borrow owner handle at dense index.
     * @param Index Dense index.
     * @return Owner handle or null handle when out-of-range.
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
     * @brief Borrow component pointer at dense index.
     * @param Index Dense index.
     * @return Component pointer or nullptr when out-of-range/missing.
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
