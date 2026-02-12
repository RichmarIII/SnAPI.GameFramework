#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "INode.h"
#include "StaticTypeId.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;
class IWorld;

/**
 * @brief Canonical concrete node implementation used by NodeGraph.
 * @remarks
 * `BaseNode` provides:
 * - hierarchy bookkeeping (`Parent` / `Children`)
 * - identity and reflection identity (`Handle` / `TypeKey`)
 * - runtime role helpers (`IsServer` / `IsClient` / `IsListenServer`)
 * - component convenience APIs (`Add<T>`, `Component<T>`, `Has<T>`, `Remove<T>`)
 *
 * Ownership model:
 * - Node storage and lifetime are owned externally by `NodeGraph`/`TObjectPool`.
 * - `m_ownerGraph` and `m_world` are non-owning pointers updated by graph/world code.
 * - Pointer stability is tied to pool lifetime; handles remain the public identity boundary.
 *
 * Tick model:
 * - Tree traversal is implemented in `TickTree`/`FixedTickTree`/`LateTickTree`.
 * - Actual gameplay behavior lives in overridden hook methods (`Tick`, `FixedTick`, `LateTick`)
 *   and attached components.
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
        : m_typeId(StaticTypeId<BaseNode>())
    {
    }
    /**
     * @brief Construct a node with a custom name.
     * @param InName Node name.
     */
    explicit BaseNode(std::string InName)
        : m_name(std::move(InName))
        , m_typeId(StaticTypeId<BaseNode>())
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
     * @remarks
     * Typically assigned exactly once by `NodeGraph` at creation.
     * Reassigning on a live registered object can invalidate external handle references.
     */
    void Handle(NodeHandle Handle) override
    {
        m_self = Handle;
    }

    /**
     * @brief Get the node UUID.
     * @return UUID value.
     */
    const Uuid& Id() const override
    {
        return m_self.Id;
    }

    /**
     * @brief Set the node UUID.
     * @param Id UUID value.
     * @remarks
     * Mutates identity by replacing the internal handle payload.
     * Callers must synchronize `ObjectRegistry` and any external references when using this.
     */
    void Id(Uuid Id) override
    {
        m_self = NodeHandle(std::move(Id));
    }

    /**
     * @brief Get the reflected type id for this node.
     * @return TypeId value.
     */
    const TypeId& TypeKey() const override
    {
        return m_typeId;
    }

    /**
     * @brief Set the reflected type id for this node.
     * @param Id TypeId value.
     * @remarks
     * Reflection systems (serialization, RPC lookup, replication metadata queries) depend on
     * this value being accurate for the concrete node type.
     */
    void TypeKey(const TypeId& Id) override
    {
        m_typeId = Id;
    }

    /**
     * @brief Get the parent node handle.
     * @return Parent handle or null handle if root.
     */
    NodeHandle Parent() const override
    {
        return m_parent;
    }

    /**
     * @brief Set the parent node handle.
     * @param Parent Parent handle.
     * @remarks
     * Local assignment only. Correct hierarchy updates should also mutate the parent's
     * child list and root-node membership (`NodeGraph::AttachChild` / `DetachChild`).
     */
    void Parent(NodeHandle Parent) override
    {
        m_parent = Parent;
    }

    /**
     * @brief Get the list of child handles.
     * @return Vector of child handles.
     */
    const std::vector<NodeHandle>& Children() const override
    {
        return m_children;
    }

    /**
     * @brief Add a child handle to the node.
     * @param Child Child handle.
     * @remarks
     * This appends only to local child bookkeeping; it does not enforce uniqueness and does
     * not modify child-side ownership/parent pointers.
     */
    void AddChild(NodeHandle Child) override
    {
        m_children.push_back(Child);
    }

    /**
     * @brief Remove a child handle from the node.
     * @param Child Child handle to remove.
     * @remarks
     * Performs first-match erase. If duplicate child handles were inserted, later duplicates
     * remain until explicitly removed.
     */
    void RemoveChild(NodeHandle Child) override
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
    bool Active() const override
    {
        return m_active;
    }

    /**
     * @brief Set the active state for the node.
     * @param Active New active state.
     * @remarks
     * Active=false suppresses this node's tick hooks during traversal.
     * This is an execution-state toggle, not a destruction or detachment operation.
     */
    void Active(bool Active) override
    {
        m_active = Active;
    }

    /**
     * @brief Check if the node is replicated over the network.
     * @return True if replicated.
     */
    bool Replicated() const override
    {
        return m_replicated;
    }

    /**
     * @brief Set whether the node is replicated over the network.
     * @param Replicated New replicated state.
     * @remarks
     * Runtime replication gate: node snapshots/spawns are skipped unless true.
     * Field-level replication flags are evaluated only after this object-level gate passes.
     */
    void Replicated(bool Replicated) override
    {
        m_replicated = Replicated;
    }

    /**
     * @brief True when this node executes with server authority.
     * @remarks Derived from world networking role; false when unbound to a world/session.
     */
    bool IsServer() const override;
    /**
     * @brief True when this node executes in client context.
     * @remarks Derived from world networking role; false when unbound to a world/session.
     */
    bool IsClient() const override;
    /**
     * @brief True when this node executes as listen-server.
     * @remarks True when both server and client roles are active in the attached session.
     */
    bool IsListenServer() const override;

    /**
     * @brief Access the list of component type ids.
     * @return Mutable reference to the type id list.
     * @remarks Maintained by graph storage bookkeeping; external direct edits are discouraged.
     */
    std::vector<TypeId>& ComponentTypes() override
    {
        return m_componentTypes;
    }

    /**
     * @brief Access the list of component type ids (const).
     * @return Const reference to the type id list.
     */
    const std::vector<TypeId>& ComponentTypes() const override
    {
        return m_componentTypes;
    }

    /**
     * @brief Access the component bitmask storage.
     * @return Mutable reference to the component mask.
     * @remarks Used for fast type queries.
     */
    std::vector<uint64_t>& ComponentMask() override
    {
        return m_componentMask;
    }

    /**
     * @brief Access the component bitmask storage (const).
     * @return Const reference to the component mask.
     */
    const std::vector<uint64_t>& ComponentMask() const override
    {
        return m_componentMask;
    }

    /**
     * @brief Get the component mask version.
     * @return Version id.
     * @remarks Used to resize masks when type registry grows.
     */
    uint32_t MaskVersion() const override
    {
        return m_maskVersion;
    }

    /**
     * @brief Set the component mask version.
     * @param Version New version id.
     * @remarks Used alongside `ComponentTypeRegistry::Version()` to detect stale masks.
     */
    void MaskVersion(uint32_t Version) override
    {
        m_maskVersion = Version;
    }

    /**
     * @brief Get the owning graph.
     * @return Pointer to owner graph or nullptr if unowned.
     */
    NodeGraph* OwnerGraph() const override
    {
        return m_ownerGraph;
    }

    /**
     * @brief Set the owning graph.
     * @param Graph Owner graph pointer.
     * @remarks Non-owning pointer updated by graph move/attach operations.
     */
    void OwnerGraph(NodeGraph* Graph) override
    {
        m_ownerGraph = Graph;
    }

    /**
     * @brief Get the owning world for this node.
     * @return Pointer to the world interface or nullptr if unowned.
     */
    IWorld* World() const override
    {
        return m_world;
    }

    /**
     * @brief Set the owning world for this node.
     * @param InWorld World interface pointer.
     * @remarks
     * Non-owning pointer propagated by world/graph attachment. Null world is valid for
     * detached graphs/prefabs.
     */
    void World(IWorld* InWorld) override
    {
        m_world = InWorld;
    }

    /**
     * @brief Add a component of type T to this node.
     * @tparam T Component type.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     * @remarks
     * Delegates to the owning graph storage. Fails when node is detached from a graph.
     * Reflection for `T` is ensured on first use before construction.
     */
    template<typename T, typename... Args>
    TExpectedRef<T> Add(Args&&... args);

    /**
     * @brief Get a component of type T from this node.
     * @tparam T Component type.
     * @return Reference wrapper or error.
     * @remarks Requires graph ownership; returns `NotReady` when detached.
     */
    template<typename T>
    TExpectedRef<T> Component();

    /**
     * @brief Check if a component of type T exists on this node.
     * @tparam T Component type.
     * @return True if present.
     * @remarks Safe on detached nodes (returns false).
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
     * @remarks
     * Traversal semantics:
     * 1. if node is active, run `Tick` and attached component tick
     * 2. recurse into child nodes in stored order
     */
    void TickTree(float DeltaSeconds) override;
    /**
     * @brief Fixed-step tick for this node and its subtree.
     * @param DeltaSeconds Fixed time step.
     * @remarks Uses same traversal ordering as `TickTree`.
     */
    void FixedTickTree(float DeltaSeconds) override;
    /**
     * @brief Late tick for this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     * @remarks Uses same traversal ordering as `TickTree`.
     */
    void LateTickTree(float DeltaSeconds) override;

private:
    NodeHandle m_self{}; /**< @brief Stable runtime identity handle for this node. */
    NodeHandle m_parent{}; /**< @brief Parent identity; null indicates this node is a root in its graph. */
    std::vector<NodeHandle> m_children{}; /**< @brief Ordered child identity list used for deterministic traversal. */
    std::string m_name{"Node"}; /**< @brief Human-readable/debug name (not required to be unique). */
    bool m_active = true; /**< @brief Local execution gate used by tree traversal. */
    bool m_replicated = false; /**< @brief Runtime replication gate for networking bridges. */
    std::vector<TypeId> m_componentTypes{}; /**< @brief Attached component type ids for introspection and fast feature checks. */
    std::vector<uint64_t> m_componentMask{}; /**< @brief Dense bitmask mirror of `m_componentTypes` for fast `Has<T>` checks. */
    uint32_t m_maskVersion = 0; /**< @brief Last component-type-registry version this mask was synchronized against. */
    NodeGraph* m_ownerGraph = nullptr; /**< @brief Non-owning pointer to the graph that stores and ticks this node. */
    IWorld* m_world = nullptr; /**< @brief Non-owning pointer to world context for subsystem access and role queries. */
    TypeId m_typeId{}; /**< @brief Reflected type identity used by serialization/rpc/replication metadata lookups. */
};

} // namespace SnAPI::GameFramework
