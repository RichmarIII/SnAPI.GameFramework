#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "Handle.h"
#include "Handles.h"
#include "NodeComponentContracts.h"
#include "Uuid.h"
#include "WorldEcsRuntime.h"

namespace SnAPI::GameFramework
{

class Level;
class BaseNode;
class IWorld;
class Variant;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical base type for runtime components attached to nodes.
 *
 * A `BaseComponent` models attachable gameplay data or behavior that participates in the
 * lifecycle of an owning node. Components are identified independently from their node so
 * they can be serialized, replicated, and targeted by reflection/RPC systems without using
 * raw pointers as the public contract.
 *
 * Why this type exists:
 * - to keep reusable behavior/data separate from hierarchy objects
 * - to give each attachment a stable identity and reflected type
 * - to let the world own lifecycle, replication, and tick dispatch uniformly
 *
 * Ownership and lifetime:
 * - Components are owned by world-managed runtime storage, not by callers.
 * - `Owner()` and `OwnerNode()` are non-owning links back to the attaching node.
 * - Destruction is typically deferred until end-of-frame so handles remain stable during the active frame.
 * - `OnCreate()` delivery may be temporarily suppressed during bootstrap and replayed once the world is ready.
 *
 * Threading model:
 * - Main-thread only for attachment, mutation, and lifecycle callbacks unless a derived type documents otherwise.
 * - Borrowed owner/world pointers are not synchronized for concurrent use.
 *
 * Invariants:
 * - `TypeKey()` must identify the concrete reflected component type.
 * - `Id()` is the stable public identity used by handles, replication, and serialization.
 * - `Owner()` is null only when the component is detached or not yet fully initialized.
 *
 * @see BaseNode
 * @see ComponentHandle
 * @see IWorld
 */
class BaseComponent
{
public:
    /**
     * @brief Construct a component in an inert default state.
     * @remarks Constructors must stay side-effect free; world/backend setup belongs in `OnCreate()`.
     */
    BaseComponent() = default;
    BaseComponent(const BaseComponent&) = delete;
    BaseComponent& operator=(const BaseComponent&) = delete;
    BaseComponent(BaseComponent&&) noexcept = default;
    BaseComponent& operator=(BaseComponent&&) noexcept = default;
    /**
     * @brief Destructor.
     * @remarks Runtime/backend teardown belongs in `OnDestroy()`, not here.
     */
    ~BaseComponent() = default;

    /**
     * @brief Component construction lifecycle hook.
     * @remarks
     * Runs after owner identity, reflected type, and registry linkage have been established.
     * Worlds may suppress immediate delivery during bootstrap and flush it later once dependent
     * subsystems are ready.
     */
    void OnCreate() {}
    /**
     * @brief Component destruction lifecycle hook.
     * @remarks Runs during end-of-frame destroy flush or immediate clear paths while world context is still valid.
     */
    void OnDestroy() {}
    /**
     * @brief Early variable-step update hook.
     * @param DeltaSeconds Time since last tick in seconds.
     * @remarks Called from owning node traversal when node and component are both active.
     */
    void PreTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Primary variable-step update hook.
     * @param DeltaSeconds Time since last tick in seconds.
     * @remarks Called from owning node traversal when node and component are both active.
     */
    void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Fixed-step update hook.
     * @param DeltaSeconds Fixed time step in seconds.
     * @remarks Intended for deterministic simulation work.
     */
    void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Late update hook.
     * @param DeltaSeconds Time since last tick in seconds.
     * @remarks Invoked after regular per-frame tick traversal.
     */
    void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Post update hook.
     * @param DeltaSeconds Time since last tick in seconds.
     * @remarks Invoked after regular variable-step and late phases.
     */
    void PostTick(float DeltaSeconds) { (void)DeltaSeconds; }
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only update hook used when the owning world is executing in editor mode. @param DeltaSeconds Time since last tick in seconds. */
    void EditorTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Editor-only callback fired after a reflected property changes. @param Name Reflected property name. */
    void EditorOnPropertyChanged(std::string_view Name) { (void)Name; }
#endif

    /**
     * @brief Set the owning node handle.
     * @param InOwner Owner node handle.
     * @remarks Storage-managed setter; identity linkage should generally be mutated only by graph/storage code.
     */
    void Owner(const NodeHandle& InOwner)
    {
        m_owner = InOwner;
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
     * erased `BaseComponent` pointers.
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
     * @remarks Resolves through the stored owner handle each time and may rehydrate it back to the fast path.
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
    Uuid m_id{}; /**< @brief Stable component identity used for handles/replication/serialization. */
    uint32_t m_runtimePoolToken = ComponentHandle::kInvalidRuntimePoolToken; /**< @brief Runtime pool token for fast handle resolution. */
    uint32_t m_runtimeIndex = ComponentHandle::kInvalidRuntimeIndex; /**< @brief Runtime pool slot index for fast handle resolution. */
    uint32_t m_runtimeGeneration = 0; /**< @brief Runtime pool slot generation for stale-handle rejection. */
    TypeId m_typeId{}; /**< @brief Reflected concrete component type id used by RPC/serialization paths. */
    bool m_active = true; /**< @brief Runtime tick gate for this component instance. */
    bool m_replicated = false; /**< @brief Runtime replication gate for this component instance. */
};

static_assert(ComponentContractConcept<BaseComponent>);

} // namespace SnAPI::GameFramework
