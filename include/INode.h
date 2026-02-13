#pragma once

#include <initializer_list>
#include <span>
#include <string_view>
#include <string>
#include <vector>

#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;
class IWorld;
class Variant;

/**
 * @brief Abstract runtime contract for graph nodes.
 * @remarks
 * `INode` intentionally defines only the non-template, runtime-facing surface that
 * graph/world systems need to traverse, serialize, replicate, and inspect nodes.
 *
 * Semantics:
 * - A node is identity-first (`Handle` / `Id`) and hierarchy-aware (`Parent` / `Children`).
 * - Runtime ownership is external: `NodeGraph` controls insertion/removal and lifecycle.
 * - `World` association is optional for detached/prefab graphs, but world-backed behavior
 *   (networking/audio subsystems, authoritative role queries, tick-tree participation)
 *   depends on a valid `World()` pointer.
 *
 * Implementers:
 * - `BaseNode` is the canonical implementation and should be preferred.
 * - Implementing `INode` directly is valid but requires preserving all invariants described
 *   on each accessor/mutator below.
 */
class INode
{
public:
    /**
     * @brief Virtual destructor for interface.
     */
    virtual ~INode() = default;

    /**
     * @brief Per-frame update hook.
     * @param DeltaSeconds Time since last tick.
     * @remarks
     * Called by `TickTree` when the node is active and relevant.
     * `DeltaSeconds` is variable-step and should be treated as frame time.
     */
    virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Fixed-step update hook.
     * @param DeltaSeconds Fixed time step.
     * @remarks
     * Called by `FixedTickTree` for deterministic simulation style updates.
     * Expected to run with a stable step chosen by the caller/system.
     */
    virtual void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Late update hook.
     * @param DeltaSeconds Time since last tick.
     * @remarks
     * Called after regular `Tick` traversal for post-update work (camera follow,
     * deferred transform propagation, etc).
     */
    virtual void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }

    /**
     * @brief Get the display name of the node.
     * @return Node name.
     */
    virtual const std::string& Name() const = 0;
    /**
     * @brief Set the display name of the node.
     * @param Name New name.
     */
    virtual void Name(std::string Name) = 0;

    /**
     * @brief Get the handle for this node.
     * @return Node handle.
     * @remarks Handles are UUID-based and resolve via ObjectRegistry.
     */
    virtual NodeHandle Handle() const = 0;
    /**
     * @brief Set the node handle.
     * @param Handle New handle.
     * @remarks
     * Handle identity is usually assigned by the owning graph/pool.
     * Reassigning identity on a live registered node can break registry references.
     */
    virtual void Handle(NodeHandle Handle) = 0;

    /**
     * @brief Get the node UUID.
     * @return UUID value.
     */
    virtual const Uuid& Id() const = 0;
    /**
     * @brief Set the node UUID.
     * @param Id UUID value.
     * @remarks
     * Mutating UUID is identity mutation; callers must keep object registry and handle
     * maps coherent when using this directly.
     */
    virtual void Id(Uuid Id) = 0;

    /**
     * @brief Get the reflected type id for this node.
     * @return TypeId value.
     */
    virtual const TypeId& TypeKey() const = 0;
    /**
     * @brief Set the reflected type id for this node.
     * @param Id TypeId value.
     * @remarks
     * This is runtime reflection identity, not C++ RTTI. Serialization and replication
     * rely on this value to resolve constructors/fields/methods remotely.
     */
    virtual void TypeKey(const TypeId& Id) = 0;

    /**
     * @brief Get the parent node handle.
     * @return Parent handle or null handle if root.
     */
    virtual NodeHandle Parent() const = 0;
    /**
     * @brief Set the parent node handle.
     * @param Parent Parent handle.
     * @remarks
     * Parent assignment should remain consistent with the parent node's child list.
     * `NodeGraph::AttachChild/DetachChild` is the authoritative API for hierarchy edits.
     */
    virtual void Parent(NodeHandle Parent) = 0;

    /**
     * @brief Get the list of child handles.
     * @return Vector of child handles.
     */
    virtual const std::vector<NodeHandle>& Children() const = 0;
    /**
     * @brief Add a child handle to the node.
     * @param Child Child handle.
     * @remarks
     * This mutates only local child bookkeeping. It does not auto-set child parent/world;
     * graph orchestration code is responsible for keeping both sides consistent.
     */
    virtual void AddChild(NodeHandle Child) = 0;
    /**
     * @brief Remove a child handle from the node.
     * @param Child Child handle to remove.
     * @remarks
     * Removal here affects only this node's child list. Graph code should clear the
     * child's `Parent()` and root-list membership as needed.
     */
    virtual void RemoveChild(NodeHandle Child) = 0;

    /**
     * @brief Check if the node is active.
     * @return True if active.
     */
    virtual bool Active() const = 0;
    /**
     * @brief Set the active state for the node.
     * @param Active New active state.
     * @remarks
     * Active=false suppresses tick hooks for this node in tree traversal.
     * This flag does not destroy, detach, or unregister the node.
     */
    virtual void Active(bool Active) = 0;

    /**
     * @brief Check if the node is replicated over the network.
     * @return True if replicated.
     */
    virtual bool Replicated() const = 0;
    /**
     * @brief Set whether the node is replicated over the network.
     * @param Replicated New replicated state.
     * @remarks
     * Replication bridges skip nodes with `Replicated()==false` regardless of field flags.
     * This is a runtime gate in addition to reflection metadata.
     */
    virtual void Replicated(bool Replicated) = 0;

    /**
     * @brief Check whether this node is executing with server authority.
     * @return True when server-authoritative.
     */
    virtual bool IsServer() const = 0;
    /**
     * @brief Check whether this node is executing in a client context.
     * @return True when client-side.
     */
    virtual bool IsClient() const = 0;
    /**
     * @brief Check whether this node is executing as a listen-server.
     * @return True when both server and client role are active.
     */
    virtual bool IsListenServer() const = 0;

    /**
     * @brief Dispatch a reflected RPC method for this node.
     * @param MethodName Reflected method name.
     * @param Args Variant-packed arguments.
     * @return True when dispatch succeeded (local invoke or queued network call).
     * @remarks
     * Routing is derived from reflected method flags:
     * - `RpcNetServer`: server invokes locally; clients forward to server.
     * - `RpcNetClient`: clients invoke locally; server forwards to one client.
     * - `RpcNetMulticast`: server forwards to multicast channel; clients invoke locally.
     */
    bool CallRPC(std::string_view MethodName, std::span<const Variant> Args = {});

    /**
     * @brief Initializer-list convenience overload for `CallRPC`.
     */
    bool CallRPC(std::string_view MethodName, std::initializer_list<Variant> Args);

    /**
     * @brief Access the list of component type ids.
     * @return Mutable reference to the type id list.
     * @remarks
     * The list models which component types are currently attached. It is maintained by
     * graph/component-storage code; direct external mutation should be avoided.
     */
    virtual std::vector<TypeId>& ComponentTypes() = 0;
    /**
     * @brief Access the list of component type ids (const).
     * @return Const reference to the type id list.
     */
    virtual const std::vector<TypeId>& ComponentTypes() const = 0;

    /**
     * @brief Access the component bitmask storage.
     * @return Mutable reference to the component mask.
     * @remarks
     * Mask bits are indexed by `ComponentTypeRegistry`. This enables fast `Has<T>()` style
     * queries without probing every storage map.
     */
    virtual std::vector<uint64_t>& ComponentMask() = 0;
    /**
     * @brief Access the component bitmask storage (const).
     * @return Const reference to the component mask.
     */
    virtual const std::vector<uint64_t>& ComponentMask() const = 0;

    /**
     * @brief Get the component mask version.
     * @return Version id.
     */
    virtual uint32_t MaskVersion() const = 0;
    /**
     * @brief Set the component mask version.
     * @param Version New version id.
     * @remarks
     * Version tracks when mask layout might be stale due to new component types entering
     * the global registry.
     */
    virtual void MaskVersion(uint32_t Version) = 0;

    /**
     * @brief Get the owning graph.
     * @return Pointer to owner graph or nullptr if unowned.
     */
    virtual NodeGraph* OwnerGraph() const = 0;
    /**
     * @brief Set the owning graph.
     * @param Graph Owner graph pointer.
     * @remarks
     * Non-owning pointer. Graph ownership changes (move, attach into other graph, etc)
     * must keep this pointer up to date.
     */
    virtual void OwnerGraph(NodeGraph* Graph) = 0;

    /**
     * @brief Get the owning world for this node.
     * @return Pointer to the world interface or nullptr if unowned.
     */
    virtual IWorld* World() const = 0;
    /**
     * @brief Set the owning world for this node.
     * @param InWorld World interface pointer.
     * @remarks
     * Non-owning pointer. A null world means detached/prefab-style existence with no
     * world subsystem access and no world-driven tick-tree participation.
     */
    virtual void World(IWorld* InWorld) = 0;

    /**
     * @brief Tick this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     * @remarks
     * Expected traversal contract:
     * 1. execute this node's `Tick`
     * 2. tick attached components
     * 3. recurse into children
     */
    virtual void TickTree(float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick for this node and its subtree.
     * @param DeltaSeconds Fixed time step.
     * @remarks Uses the same traversal ordering contract as `TickTree`.
     */
    virtual void FixedTickTree(float DeltaSeconds) = 0;
    /**
     * @brief Late tick for this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     * @remarks Uses the same traversal ordering contract as `TickTree`.
     */
    virtual void LateTickTree(float DeltaSeconds) = 0;
};

} // namespace SnAPI::GameFramework
