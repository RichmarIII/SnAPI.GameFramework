#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "Handle.h"
#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;
class BaseNode;
class IWorld;
class Variant;

/**
 * @brief Runtime contract for attachable node behavior/data units.
 * @remarks
 * Components are identity-bearing objects with independent lifecycle hooks that are
 * attached to nodes by graph-managed storage.
 *
 * Ownership and lifetime:
 * - Stored/owned by typed component storages (`TComponentStorage<T>`).
 * - Addressable by UUID (`ComponentHandle`) through `ObjectRegistry`.
 * - Destruction is deferred to end-of-frame to keep handles stable within a frame.
 *
 * Execution context:
 * - `OwnerNode()` and `World()` are resolved dynamically through handle/graph links.
 * - Role helpers (`IsServer`/`IsClient`/`IsListenServer`) proxy world networking state.
 */
class IComponent
{
public:
    /**
     * @brief Virtual destructor for interface.
     */
    virtual ~IComponent() = default;

    /**
     * @brief Called immediately after component creation.
     * @remarks Runs once after storage/owner identity is assigned and registration is complete.
     */
    virtual void OnCreate() {}
    /**
     * @brief Called just before component destruction.
     * @remarks Runs during end-of-frame destroy flush or immediate clear path.
     */
    virtual void OnDestroy() {}
    /**
     * @brief Per-frame update hook.
     * @param DeltaSeconds Time since last tick.
     * @remarks Called from owning node traversal when node/component are active.
     */
    virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Fixed-step update hook.
     * @param DeltaSeconds Fixed time step.
     * @remarks Intended for deterministic simulation work.
     */
    virtual void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Late update hook.
     * @param DeltaSeconds Time since last tick.
     * @remarks Invoked after regular per-frame tick traversal.
     */
    virtual void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }

    /**
     * @brief Set the owning node handle.
     * @param InOwner Owner node handle.
     * @remarks Storage-managed setter; identity linkage should generally be mutated only by graph/storage code.
     */
    void Owner(NodeHandle InOwner)
    {
        m_owner = InOwner;
        m_ownerNode = InOwner.IsNull() ? nullptr : InOwner.Borrowed();
    }

    /**
     * @brief Get the owning node handle.
     * @return Owner node handle.
     */
    NodeHandle Owner() const
    {
        return m_owner;
    }

    /**
     * @brief Check if this component is active for tick execution.
     * @return True when tick hooks are enabled.
     */
    bool Active() const
    {
        return m_active;
    }

    /**
     * @brief Set component active state for tick execution.
     * @param ActiveValue New active state.
     * @remarks
     * Active=false suppresses Tick/FixedTick/LateTick dispatch while the
     * component remains attached and replicated/serializable.
     */
    void Active(bool ActiveValue)
    {
        m_active = ActiveValue;
    }

    /**
     * @brief Check if the component is replicated over the network.
     * @return True if replicated.
     */
    bool Replicated() const
    {
        return m_replicated;
    }

    /**
     * @brief Set whether the component is replicated over the network.
     * @param Replicated New replicated state.
     * @remarks Runtime gate: even replicated fields are skipped when false.
     */
    void Replicated(bool Replicated)
    {
        m_replicated = Replicated;
    }

    /**
     * @brief Get the component UUID.
     * @return UUID of this component.
     */
    const Uuid& Id() const
    {
        return m_id;
    }

    /**
     * @brief Set the component UUID.
     * @param Id New UUID value.
     * @remarks Identity mutation; component registry/bookkeeping must stay in sync.
     */
    void Id(Uuid Id)
    {
        m_id = Id;
    }

    /**
     * @brief Get the reflected type id for this component.
     * @return TypeId value.
     * @remarks
     * Required for reflection RPC/serialization lookup when working through
     * erased `IComponent` pointers.
     */
    const TypeId& TypeKey() const
    {
        return m_typeId;
    }

    /**
     * @brief Set the reflected type id for this component.
     * @param Id Reflected component type id.
     */
    void TypeKey(const TypeId& Id)
    {
        m_typeId = Id;
    }

    /**
     * @brief Get a handle for this component.
     * @return ComponentHandle wrapping the UUID.
     */
    ComponentHandle Handle() const
    {
        return ComponentHandle(m_id, m_runtimePoolToken, m_runtimeIndex, m_runtimeGeneration);
    }

    /**
     * @brief Set runtime slot identity for fast handle resolution.
     * @param RuntimePoolToken Runtime pool token.
     * @param RuntimeIndex Runtime slot index.
     * @param RuntimeGeneration Runtime slot generation.
     * @remarks Managed by component storage/pool integration code.
     */
    void RuntimeIdentity(uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration)
    {
        m_runtimePoolToken = RuntimePoolToken;
        m_runtimeIndex = RuntimeIndex;
        m_runtimeGeneration = RuntimeGeneration;
    }

    /**
     * @brief Resolve the owning node pointer.
     * @return Owning BaseNode pointer or nullptr.
     * @remarks Uses cached owner pointer and falls back to handle resolution if needed.
     */
    BaseNode* OwnerNode() const;

    /**
     * @brief Resolve the owning world pointer.
     * @return Owning world or nullptr.
     * @remarks Returns null for detached/prefab graphs not currently world-attached.
     */
    IWorld* World() const;

    /**
     * @brief Check whether this component executes with server authority.
     * @return True when server-authoritative.
     */
    bool IsServer() const;

    /**
     * @brief Check whether this component executes in a client context.
     * @return True when client-side.
     */
    bool IsClient() const;

    /**
     * @brief Check whether this component executes as listen-server.
     * @return True when both server and client role are active.
     */
    bool IsListenServer() const;

    /**
     * @brief Dispatch a reflected RPC method for this component.
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

private:
    NodeHandle m_owner{}; /**< @brief Owning node identity; resolved via ObjectRegistry when needed. */
    mutable BaseNode* m_ownerNode = nullptr; /**< @brief Cached owner node pointer to avoid repeated registry resolution. */
    Uuid m_id{}; /**< @brief Stable component identity used for handles/replication/serialization. */
    uint32_t m_runtimePoolToken = ComponentHandle::kInvalidRuntimePoolToken; /**< @brief Runtime pool token for fast handle resolution. */
    uint32_t m_runtimeIndex = ComponentHandle::kInvalidRuntimeIndex; /**< @brief Runtime pool slot index for fast handle resolution. */
    uint32_t m_runtimeGeneration = 0; /**< @brief Runtime pool slot generation for stale-handle rejection. */
    TypeId m_typeId{}; /**< @brief Reflected concrete component type id used by RPC/serialization paths. */
    bool m_active = true; /**< @brief Runtime tick gate for this component instance. */
    bool m_replicated = false; /**< @brief Runtime replication gate for this component instance. */
};

} // namespace SnAPI::GameFramework
