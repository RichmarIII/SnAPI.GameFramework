#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "INode.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;

/**
 * @brief Default node implementation for the scene graph.
 * @remarks Owns child relationships and component bookkeeping.
 * @note Nodes are addressed by NodeHandle (UUID).
 */
class BaseNode : public INode
{
public:
    /** @brief Stable type name used for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BaseNode";

    /**
     * @brief Construct a node with default name.
     */
    BaseNode()
        : m_typeId(TypeIdFromName(kTypeName))
    {
    }
    /**
     * @brief Construct a node with a custom name.
     * @param InName Node name.
     */
    explicit BaseNode(std::string InName)
        : m_name(std::move(InName))
        , m_typeId(TypeIdFromName(kTypeName))
    {
    }

    /**
     * @brief Get the node name.
     * @return Name string.
     */
    const std::string& Name() const override
    {
        return m_name;
    }

    /**
     * @brief Set the node name.
     * @param Name New name.
     */
    void Name(std::string Name) override
    {
        m_name = std::move(Name);
    }

    /**
     * @brief Get the node handle.
     * @return NodeHandle for this node.
     */
    NodeHandle Handle() const override
    {
        return m_self;
    }

    /**
     * @brief Set the node handle.
     * @param Handle New handle.
     * @remarks Set by NodeGraph when the node is created.
     */
    void Handle(NodeHandle Handle)
    {
        m_self = Handle;
    }

    /**
     * @brief Get the node UUID.
     * @return UUID value.
     */
    const Uuid& Id() const
    {
        return m_self.Id;
    }

    /**
     * @brief Set the node UUID.
     * @param Id UUID value.
     * @remarks Updates the internal handle.
     */
    void Id(Uuid Id)
    {
        m_self = NodeHandle(std::move(Id));
    }

    /**
     * @brief Get the reflected type id for this node.
     * @return TypeId value.
     */
    const TypeId& TypeKey() const
    {
        return m_typeId;
    }

    /**
     * @brief Set the reflected type id for this node.
     * @param Id TypeId value.
     * @remarks Set by NodeGraph when creating nodes by type.
     */
    void TypeKey(const TypeId& Id)
    {
        m_typeId = Id;
    }

    /**
     * @brief Get the parent node handle.
     * @return Parent handle or null handle if root.
     */
    NodeHandle Parent() const
    {
        return m_parent;
    }

    /**
     * @brief Set the parent node handle.
     * @param Parent Parent handle.
     * @remarks Used by NodeGraph to maintain hierarchy.
     */
    void Parent(NodeHandle Parent)
    {
        m_parent = Parent;
    }

    /**
     * @brief Get the list of child handles.
     * @return Vector of child handles.
     */
    const std::vector<NodeHandle>& Children() const
    {
        return m_children;
    }

    /**
     * @brief Add a child handle to the node.
     * @param Child Child handle.
     * @remarks Does not set the child's parent; NodeGraph manages this.
     */
    void AddChild(NodeHandle Child)
    {
        m_children.push_back(Child);
    }

    /**
     * @brief Remove a child handle from the node.
     * @param Child Child handle to remove.
     */
    void RemoveChild(NodeHandle Child)
    {
        for (auto It = m_children.begin(); It != m_children.end(); ++It)
        {
            if (*It == Child)
            {
                m_children.erase(It);
                return;
            }
        }
    }

    /**
     * @brief Check if the node is active.
     * @return True if active.
     * @remarks Inactive nodes are skipped during tick.
     */
    bool Active() const
    {
        return m_active;
    }

    /**
     * @brief Set the active state for the node.
     * @param Active New active state.
     */
    void Active(bool Active)
    {
        m_active = Active;
    }

    /**
     * @brief Access the list of component type ids.
     * @return Mutable reference to the type id list.
     */
    std::vector<TypeId>& ComponentTypes()
    {
        return m_componentTypes;
    }

    /**
     * @brief Access the list of component type ids (const).
     * @return Const reference to the type id list.
     */
    const std::vector<TypeId>& ComponentTypes() const
    {
        return m_componentTypes;
    }

    /**
     * @brief Access the component bitmask storage.
     * @return Mutable reference to the component mask.
     * @remarks Used for fast type queries.
     */
    std::vector<uint64_t>& ComponentMask()
    {
        return m_componentMask;
    }

    /**
     * @brief Access the component bitmask storage (const).
     * @return Const reference to the component mask.
     */
    const std::vector<uint64_t>& ComponentMask() const
    {
        return m_componentMask;
    }

    /**
     * @brief Get the component mask version.
     * @return Version id.
     * @remarks Used to resize masks when type registry grows.
     */
    uint32_t MaskVersion() const
    {
        return m_maskVersion;
    }

    /**
     * @brief Set the component mask version.
     * @param Version New version id.
     */
    void MaskVersion(uint32_t Version)
    {
        m_maskVersion = Version;
    }

    /**
     * @brief Get the owning graph.
     * @return Pointer to owner graph or nullptr if unowned.
     */
    NodeGraph* OwnerGraph() const
    {
        return m_ownerGraph;
    }

    /**
     * @brief Set the owning graph.
     * @param Graph Owner graph pointer.
     * @remarks Assigned by NodeGraph when the node is inserted.
     */
    void OwnerGraph(NodeGraph* Graph)
    {
        m_ownerGraph = Graph;
    }

    /**
     * @brief Add a component of type T to this node.
     * @tparam T Component type.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     * @remarks Delegates to the owner graph.
     */
    template<typename T, typename... Args>
    TExpectedRef<T> Add(Args&&... args);

    /**
     * @brief Get a component of type T from this node.
     * @tparam T Component type.
     * @return Reference wrapper or error.
     */
    template<typename T>
    TExpectedRef<T> Component();

    /**
     * @brief Check if a component of type T exists on this node.
     * @tparam T Component type.
     * @return True if present.
     */
    template<typename T>
    bool Has() const;

    /**
     * @brief Remove a component of type T from this node.
     * @tparam T Component type.
     * @remarks Removal is deferred until end-of-frame.
     */
    template<typename T>
    void Remove();

    /**
     * @brief Tick this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     * @remarks Checks relevance and active state.
     */
    void TickTree(float DeltaSeconds);
    /**
     * @brief Fixed-step tick for this node and its subtree.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTickTree(float DeltaSeconds);
    /**
     * @brief Late tick for this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTickTree(float DeltaSeconds);

private:
    NodeHandle m_self{}; /**< @brief Handle for this node. */
    NodeHandle m_parent{}; /**< @brief Parent handle (null if root). */
    std::vector<NodeHandle> m_children{}; /**< @brief Child handles. */
    std::string m_name{"Node"}; /**< @brief Display name. */
    bool m_active = true; /**< @brief Active state. */
    std::vector<TypeId> m_componentTypes{}; /**< @brief Component type ids present. */
    std::vector<uint64_t> m_componentMask{}; /**< @brief Bitmask for component queries. */
    uint32_t m_maskVersion = 0; /**< @brief Mask version for registry changes. */
    NodeGraph* m_ownerGraph = nullptr; /**< @brief Owning graph (non-owning). */
    TypeId m_typeId{}; /**< @brief Reflected type id. */
};

} // namespace SnAPI::GameFramework
