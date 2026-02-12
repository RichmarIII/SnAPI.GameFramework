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
 * @remarks Used by THandle to resolve UUIDs to objects at runtime.
 * @note Objects must be registered/unregistered by their owning systems.
 * @note Registry stores non-owning pointers; lifetime is managed externally.
 */
class ObjectRegistry
{
public:
    /**
     * @brief Access the singleton registry instance.
     * @return Reference to the registry.
     * @remarks Thread-safe via internal mutex.
     */
    static ObjectRegistry& Instance()
    {
        static ObjectRegistry Registry;
        return Registry;
    }

    /**
     * @brief Register a node with the registry.
     * @param Id UUID of the node.
     * @param Node Pointer to the node.
     * @remarks Overwrites any existing entry with the same UUID.
     * @note Id must not be nil.
     */
    void RegisterNode(const Uuid& Id, BaseNode* Node)
    {
        DEBUG_ASSERT(Node != nullptr, "RegisterNode requires a node");
        RegisterInternal(Id, EObjectKind::Node, Node, nullptr, nullptr, std::type_index(typeid(void)));
    }

    /**
     * @brief Register a component with the registry.
     * @param Id UUID of the component.
     * @param Component Pointer to the component.
     * @remarks Overwrites any existing entry with the same UUID.
     * @note Id must not be nil.
     */
    void RegisterComponent(const Uuid& Id, IComponent* Component)
    {
        DEBUG_ASSERT(Component != nullptr, "RegisterComponent requires a component");
        RegisterInternal(Id, EObjectKind::Component, nullptr, Component, nullptr, std::type_index(typeid(void)));
    }

    /**
     * @brief Register an arbitrary object with the registry.
     * @tparam T Object type.
     * @param Id UUID of the object.
     * @param Object Pointer to the object.
     * @remarks Type identity for `Other` entries is exact `type_index` (no inheritance matching).
     */
    template<typename T>
    void Register(const Uuid& Id, T* Object)
    {
        RegisterInternal(Id, EObjectKind::Other, nullptr, nullptr, static_cast<void*>(Object), typeid(T));
    }

    /**
     * @brief Unregister an object by UUID.
     * @param Id UUID to remove.
     * @remarks Safe to call even if the UUID is not present.
     */
    void Unregister(const Uuid& Id)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        m_entries.erase(Id);
    }

    /**
     * @brief Resolve a UUID to a typed pointer.
     * @tparam T Expected type.
     * @param Id UUID to resolve.
     * @return Pointer to the object, or nullptr if not found/type mismatch.
     * @remarks Node/Component entries resolve by object kind; Other entries require exact type match.
     */
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

    /**
     * @brief Check if a UUID resolves to a live object of type T.
     * @tparam T Expected type.
     * @param Id UUID to check.
     * @return True when Resolve<T> returns non-null.
     */
    template<typename T>
    bool IsValid(const Uuid& Id) const
    {
        return Resolve<T>(Id) != nullptr;
    }

private:
    /**
     * @brief Registry entry payload.
     * @remarks Only one of Node/Component/Other is valid based on Kind.
     */
    struct Entry
    {
        EObjectKind Kind = EObjectKind::Other;       /**< @brief Object kind. */
        BaseNode* Node = nullptr;                    /**< @brief Node pointer if Kind == Node. */
        IComponent* Component = nullptr;             /**< @brief Component pointer if Kind == Component. */
        void* Other = nullptr;                       /**< @brief Opaque pointer if Kind == Other. */
        std::type_index Type = std::type_index(typeid(void)); /**< @brief Concrete type for Other. */
    };

    /**
     * @brief Internal insert/update for registry entries.
     * @param Id UUID key.
     * @param Kind Object kind.
     * @param Node Node pointer.
     * @param Component Component pointer.
     * @param Other Opaque pointer.
     * @param Type type_index for Other.
     * @remarks Overwrites existing entries with the same UUID.
     * @note Id must not be nil.
     */
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

    mutable std::mutex m_mutex{}; /**< @brief Protects the registry map. */
    std::unordered_map<Uuid, Entry, UuidHash> m_entries{}; /**< @brief UUID -> Entry map. */
};

} // namespace SnAPI::GameFramework
