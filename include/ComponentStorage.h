#pragma once

#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "IComponent.h"
#include "ObjectPool.h"
#include "ObjectRegistry.h"
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
        std::lock_guard<std::mutex> Lock(m_mutex);
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
        std::lock_guard<std::mutex> Lock(m_mutex);
        return m_version;
    }

    /**
     * @brief Get the number of 64-bit words required for the mask.
     * @return Word count for the current type set.
     */
    static size_t WordCount()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        size_t BitCount = m_typeToIndex.size();
        return (BitCount + 63u) / 64u;
    }

private:
    static inline std::mutex m_mutex{}; /**< @brief Protects registry state. */
    static inline std::unordered_map<TypeId, uint32_t, UuidHash> m_typeToIndex{}; /**< @brief TypeId -> bit index. */
    static inline uint32_t m_version = 0; /**< @brief Version counter. */
};

/**
 * @brief Type-erased interface for component storage.
 * @remarks NodeGraph uses this to manage components generically.
 */
class IComponentStorage
{
public:
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
    virtual bool Has(NodeHandle Owner) const = 0;
    /**
     * @brief Remove a component from a node.
     * @param Owner Node handle.
     * @remarks Removal is deferred until EndFrame.
     */
    virtual void Remove(NodeHandle Owner) = 0;
    /**
     * @brief Tick a component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void TickComponent(NodeHandle Owner, float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick a component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTickComponent(NodeHandle Owner, float DeltaSeconds) = 0;
    /**
     * @brief Late tick a component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTickComponent(NodeHandle Owner, float DeltaSeconds) = 0;
    /**
     * @brief Borrow a component instance (mutable).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     * @note Borrowed pointers must not be cached.
     */
    virtual void* Borrowed(NodeHandle Owner) = 0;
    /**
     * @brief Borrow a component instance (const).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     */
    virtual const void* Borrowed(NodeHandle Owner) const = 0;
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
 * @remarks Manages component pool and owner mapping.
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
     * @remarks Used by serialization to preserve identity.
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
        auto* Component = m_pool.Borrowed(Handle.Id);
        if (!Component)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Component creation failed"));
        }
        Component->Owner(Owner);
        Component->Id(Id);
        ObjectRegistry::Instance().RegisterComponent(Id, Component);
        m_index.emplace(Owner, Id);
        Component->OnCreate();
        return *Component;
    }

    /**
     * @brief Get a component by owner.
     * @param Owner Owner node handle.
     * @return Reference wrapper or error.
     */
    TExpectedRef<T> Component(NodeHandle Owner)
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Component not found"));
        }
        auto* Component = m_pool.Borrowed(It->second);
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
    bool Has(NodeHandle Owner) const override
    {
        return m_index.find(Owner) != m_index.end();
    }

    /**
     * @brief Remove a component from a node.
     * @param Owner Node handle.
     * @remarks Removal is deferred until EndFrame.
     */
    void Remove(NodeHandle Owner) override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return;
        }
        Uuid Id = It->second;
        m_index.erase(It);
        if (!m_pool.DestroyLater(Id))
        {
            return;
        }
        m_pendingDestroy.push_back(Id);
    }

    /**
     * @brief Tick the component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    void TickComponent(NodeHandle Owner, float DeltaSeconds) override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return;
        }
        auto* Component = m_pool.Borrowed(It->second);
        if (Component)
        {
            Component->Tick(DeltaSeconds);
        }
    }

    /**
     * @brief Fixed-step tick the component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTickComponent(NodeHandle Owner, float DeltaSeconds) override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return;
        }
        auto* Component = m_pool.Borrowed(It->second);
        if (Component)
        {
            Component->FixedTick(DeltaSeconds);
        }
    }

    /**
     * @brief Late tick the component for a node.
     * @param Owner Node handle.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTickComponent(NodeHandle Owner, float DeltaSeconds) override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return;
        }
        auto* Component = m_pool.Borrowed(It->second);
        if (Component)
        {
            Component->LateTick(DeltaSeconds);
        }
    }

    /**
     * @brief Borrow the component instance (mutable).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     */
    void* Borrowed(NodeHandle Owner) override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return nullptr;
        }
        return m_pool.Borrowed(It->second);
    }

    /**
     * @brief Borrow the component instance (const).
     * @param Owner Node handle.
     * @return Pointer to component or nullptr.
     */
    const void* Borrowed(NodeHandle Owner) const override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return nullptr;
        }
        return m_pool.Borrowed(It->second);
    }

    /**
     * @brief Process pending destruction at end-of-frame.
     * @remarks Calls OnDestroy and unregisters components.
     */
    void EndFrame() override
    {
        for (const auto& Id : m_pendingDestroy)
        {
            if (auto* Component = m_pool.Borrowed(Id))
            {
                Component->OnDestroy();
            }
            ObjectRegistry::Instance().Unregister(Id);
        }
        m_pendingDestroy.clear();
        m_pool.EndFrame();
    }

    /**
     * @brief Clear all components immediately.
     * @remarks Calls OnDestroy and clears internal mappings.
     */
    void Clear() override
    {
        m_pool.ForEachAll([&](const THandle<T>& Handle, T& Component) {
            Component.OnDestroy();
            ObjectRegistry::Instance().Unregister(Handle.Id);
        });
        m_index.clear();
        m_pendingDestroy.clear();
        m_pool.Clear();
    }

private:
    TypeId m_typeId = TypeIdFromName(TTypeNameV<T>); /**< @brief Component type id. */
    TObjectPool<T> m_pool{}; /**< @brief Pool storing component instances. */
    std::unordered_map<NodeHandle, Uuid, HandleHash> m_index{}; /**< @brief Owner -> component UUID. */
    std::vector<Uuid> m_pendingDestroy{}; /**< @brief Components scheduled for deletion. */
};

} // namespace SnAPI::GameFramework
