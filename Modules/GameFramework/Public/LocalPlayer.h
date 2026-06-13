#pragma once

#include <cstdint>
#include <string>

#include "BaseNode.h"
#include "Export.h"
#include "LocalPlayer.generated.hpp"

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#include "ReflectionAnnotations.h"
#endif

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Replicable player-ownership node used for local, remote, and splitscreen player state.
 *
 * `LocalPlayer` is the world-owned representation of one player slot known to the gameplay
 * host. Despite the name, the type is used for both truly local players and remote players;
 * `OwnerConnectionId` distinguishes which connection owns the player. The node tracks the
 * player index, possession target, optional input-device assignment, and whether this player
 * should currently accept input.
 *
 * Core semantics:
 * - the node is replicated by default
 * - possession is server-authoritative
 * - direct setters mutate local state immediately, while `RequestPossess()` / `RequestUnpossess()`
 *   use the RPC path when available
 *
 * Ownership and lifetime:
 * - Owned by `World` like any other node.
 * - Usually created and destroyed by `GameplayHost`.
 * - `PossessedNode` is a non-owning handle to another world-owned node.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameplayHost
 * @see LocalPlayerService
 * @see PawnBase
 */
SnType()
class SNAPI_GAMEFRAMEWORK_API LocalPlayer : public BaseNode, public NodeCRTP<LocalPlayer>
{
public:
    SnGenerated()

    LocalPlayer();
    explicit LocalPlayer(std::string Name);

    /** @brief Access the player's slot index within its owning connection. @return Mutable player-index field. */
    SnField(SnKey("PlayerIndex"), SnReplicated, SnConstGetter(GetPlayerIndex))
    unsigned int& EditPlayerIndex();
    /** @brief Access the player's slot index within its owning connection. @return Const player-index field. */
    const unsigned int& GetPlayerIndex() const;

    /** @brief Access the currently possessed node handle. @return Mutable possession handle field. */
    SnField(SnKey("PossessedNode"), SnReplicated, SnConstGetter(GetPossessedNode))
    NodeHandle& EditPossessedNode();
    /** @brief Access the currently possessed node handle. @return Const possession handle field. */
    const NodeHandle& GetPossessedNode() const;
    /**
     * @brief Set the currently possessed node immediately.
     * @param Target Desired possession target, or null handle to clear possession.
     * @remarks
     * This is a direct state mutation API. It invokes possession transition callbacks locally.
     * In multiplayer gameplay flows, clients should generally prefer `RequestPossess()` or
     * `RequestUnpossess()` so the server remains authoritative.
     */
    void SetPossessedNode(const NodeHandle& Target);
    /**
     * @brief Reconcile possession callbacks with the currently replicated possession state.
     * @remarks
     * Used when possession state changes without the local callback cache having observed
     * the transition yet, for example after replication updates.
     */
    void SyncPossessionCallbacks();

    /** @brief Access the flag that allows this player to consume input. @return Mutable input-acceptance field. */
    SnField(SnKey("AcceptInput"), SnReplicated, SnConstGetter(GetAcceptInput))
    bool& EditAcceptInput();
    /** @brief Access the flag that allows this player to consume input. @return Const input-acceptance field. */
    const bool& GetAcceptInput() const;

    /** @brief Access the owning network connection id. `0` represents local authority. @return Mutable connection-id field. */
    SnField(SnKey("OwnerConnectionId"), SnReplicated, SnConstGetter(GetOwnerConnectionId))
    std::uint64_t& EditOwnerConnectionId();
    /** @brief Access the owning network connection id. `0` represents local authority. @return Const connection-id field. */
    const std::uint64_t& GetOwnerConnectionId() const;

#if defined(SNAPI_GF_ENABLE_INPUT)
    /** @brief Access the currently assigned local input device, if any. @return Mutable device-id field. */
    SnField(SnKey("AssignedInputDevice"), SnConstGetter(GetAssignedInputDevice))
    SnAPI::Input::DeviceId& EditAssignedInputDevice();
    /** @brief Access the currently assigned local input device, if any. @return Const device-id field. */
    const SnAPI::Input::DeviceId& GetAssignedInputDevice() const;

    /** @brief Access the flag that forces input consumption from the assigned device only. @return Mutable device-filter field. */
    SnField(SnKey("UseAssignedInputDevice"), SnConstGetter(GetUseAssignedInputDevice))
    bool& EditUseAssignedInputDevice();
    /** @brief Access the flag that forces input consumption from the assigned device only. @return Const device-filter field. */
    const bool& GetUseAssignedInputDevice() const;
#endif

    /**
     * @brief Request possession of target node.
     * @param Target Desired possession target, or null handle to clear possession.
     * @remarks Clients forward to authority automatically; server executes local possession logic through `RequestPossessImpl()`.
     */
    SnFunction(SnKey("RequestPossess"), SnRpc(SnReliable, SnServer))
    void RequestPossess(const NodeHandle& Target);

    /**
     * @brief Request possession clear.
     * @remarks Clients forward to authority automatically; server executes local unpossession logic through `RequestUnpossessImpl()`.
     */
    SnFunction(SnKey("RequestUnpossess"), SnRpc(SnReliable, SnServer))
    void RequestUnpossess();

private:
    void DispatchPossessionTransition(const NodeHandle& PreviousTarget, const NodeHandle& NewTarget);

    bool CanPossessTarget(const NodeHandle& Target) const;

    unsigned int m_playerIndex = 0;
    NodeHandle m_possessedNode{};
    NodeHandle m_lastNotifiedPossessedNode{};
    bool m_acceptInput = true;

#if defined(SNAPI_GF_ENABLE_INPUT)
    SnAPI::Input::DeviceId m_assignedInputDevice{};
    bool m_useAssignedInputDevice = false;
#endif
};

} // namespace SnAPI::GameFramework
