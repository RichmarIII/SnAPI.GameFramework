#pragma once

#include <mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include "Assert.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class IComponent;

enum class EObjectKind
{
    Node,
    Component,
    Other
};

class ObjectRegistry
{
public:
    static ObjectRegistry& Instance()
    {
        static ObjectRegistry Registry;
        return Registry;
    }

    void RegisterNode(const Uuid& Id, BaseNode* Node)
    {
        DEBUG_ASSERT(Node != nullptr, "RegisterNode requires a node");
        RegisterInternal(Id, EObjectKind::Node, Node, nullptr, nullptr, std::type_index(typeid(void)));
    }

    void RegisterComponent(const Uuid& Id, IComponent* Component)
    {
        DEBUG_ASSERT(Component != nullptr, "RegisterComponent requires a component");
        RegisterInternal(Id, EObjectKind::Component, nullptr, Component, nullptr, std::type_index(typeid(void)));
    }

    template<typename T>
    void Register(const Uuid& Id, T* Object)
    {
        RegisterInternal(Id, EObjectKind::Other, nullptr, nullptr, static_cast<void*>(Object), typeid(T));
    }

    void Unregister(const Uuid& Id)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        m_entries.erase(Id);
    }

    template<typename T>
    T* Resolve(const Uuid& Id) const
    {
        if (Id.is_nil())
        {
            return nullptr;
        }
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_entries.find(Id);
        if (It == m_entries.end())
        {
            return nullptr;
        }
        const Entry& EntryRef = It->second;
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
    bool IsValid(const Uuid& Id) const
    {
        return Resolve<T>(Id) != nullptr;
    }

private:
    struct Entry
    {
        EObjectKind Kind = EObjectKind::Other;
        BaseNode* Node = nullptr;
        IComponent* Component = nullptr;
        void* Other = nullptr;
        std::type_index Type = std::type_index(typeid(void));
    };

    void RegisterInternal(const Uuid& Id,
        EObjectKind Kind,
        BaseNode* Node,
        IComponent* Component,
        void* Other,
        std::type_index Type)
    {
        if (Id.is_nil())
        {
            DEBUG_ASSERT(false, "Cannot register nil uuid");
            return;
        }
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto [It, Inserted] = m_entries.emplace(Id, Entry{Kind, Node, Component, Other, Type});
        if (!Inserted)
        {
            It->second = Entry{Kind, Node, Component, Other, Type};
        }
    }

    mutable std::mutex m_mutex{};
    std::unordered_map<Uuid, Entry, UuidHash> m_entries{};
};

} // namespace SnAPI::GameFramework
