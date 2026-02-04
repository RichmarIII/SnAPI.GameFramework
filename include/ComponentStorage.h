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

class ComponentTypeRegistry
{
public:
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

    static uint32_t Version()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        return m_version;
    }

    static size_t WordCount()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        size_t BitCount = m_typeToIndex.size();
        return (BitCount + 63u) / 64u;
    }

private:
    static inline std::mutex m_mutex{};
    static inline std::unordered_map<TypeId, uint32_t, UuidHash> m_typeToIndex{};
    static inline uint32_t m_version = 0;
};

class IComponentStorage
{
public:
    virtual ~IComponentStorage() = default;
    virtual TypeId TypeKey() const = 0;
    virtual bool Has(NodeHandle Owner) const = 0;
    virtual void Remove(NodeHandle Owner) = 0;
    virtual void TickComponent(NodeHandle Owner, float DeltaSeconds) = 0;
    virtual void FixedTickComponent(NodeHandle Owner, float DeltaSeconds) = 0;
    virtual void LateTickComponent(NodeHandle Owner, float DeltaSeconds) = 0;
    virtual void* Borrowed(NodeHandle Owner) = 0;
    virtual const void* Borrowed(NodeHandle Owner) const = 0;
    virtual void EndFrame() = 0;
    virtual void Clear() = 0;
};

template<typename T>
class TComponentStorage final : public IComponentStorage
{
public:
    static_assert(std::is_base_of_v<IComponent, T>, "Components must derive from IComponent");

    TypeId TypeKey() const override
    {
        return m_typeId;
    }

    TExpectedRef<T> Add(NodeHandle Owner)
    {
        return AddWithId(Owner, NewUuid());
    }

    template<typename... Args>
    TExpectedRef<T> Add(NodeHandle Owner, Args&&... args)
    {
        return AddWithId(Owner, NewUuid(), std::forward<Args>(args)...);
    }

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

    bool Has(NodeHandle Owner) const override
    {
        return m_index.find(Owner) != m_index.end();
    }

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

    void* Borrowed(NodeHandle Owner) override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return nullptr;
        }
        return m_pool.Borrowed(It->second);
    }

    const void* Borrowed(NodeHandle Owner) const override
    {
        auto It = m_index.find(Owner);
        if (It == m_index.end())
        {
            return nullptr;
        }
        return m_pool.Borrowed(It->second);
    }

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
    TypeId m_typeId = TypeIdFromName(TTypeNameV<T>);
    TObjectPool<T> m_pool{};
    std::unordered_map<NodeHandle, Uuid, HandleHash> m_index{};
    std::vector<Uuid> m_pendingDestroy{};
};

} // namespace SnAPI::GameFramework
