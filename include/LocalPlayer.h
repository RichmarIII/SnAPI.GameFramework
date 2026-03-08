#pragma once

#include <cstdint>
#include <string>

#include "BaseNode.h"
#include "Export.h"

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
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
class SNAPI_GAMEFRAMEWORK_API LocalPlayer : public BaseNode
{
public:
    static constexpr auto kTypeName = "SnAPI::GameFramework::LocalPlayer";

    LocalPlayer();
    explicit LocalPlayer(std::string Name);

    /** @brief Access the player's slot index within its owning connection. @return Mutable player-index field. */
    unsigned int& EditPlayerIndex();
    /** @brief Access the player's slot index within its owning connection. @return Const player-index field. */
    const unsigned int& GetPlayerIndex() const;

    /** @brief Access the currently possessed node handle. @return Mutable possession handle field. */
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
    bool& EditAcceptInput();
    /** @brief Access the flag that allows this player to consume input. @return Const input-acceptance field. */
    const bool& GetAcceptInput() const;

    /** @brief Access the owning network connection id. `0` represents local authority. @return Mutable connection-id field. */
    std::uint64_t& EditOwnerConnectionId();
    /** @brief Access the owning network connection id. `0` represents local authority. @return Const connection-id field. */
    const std::uint64_t& GetOwnerConnectionId() const;

#if defined(SNAPI_GF_ENABLE_INPUT)
    /** @brief Access the currently assigned local input device, if any. @return Mutable device-id field. */
    SnAPI::Input::DeviceId& EditAssignedInputDevice();
    /** @brief Access the currently assigned local input device, if any. @return Const device-id field. */
    const SnAPI::Input::DeviceId& GetAssignedInputDevice() const;

    /** @brief Access the flag that forces input consumption from the assigned device only. @return Mutable device-filter field. */
    bool& EditUseAssignedInputDevice();
    /** @brief Access the flag that forces input consumption from the assigned device only. @return Const device-filter field. */
    const bool& GetUseAssignedInputDevice() const;
#endif

    /**
     * @brief Request possession of target node.
     * @param Target Desired possession target, or null handle to clear possession.
     * @remarks Clients forward to `ServerRequestPossess`; server executes directly.
     */
    void RequestPossess(const NodeHandle& Target);

    /**
     * @brief Request possession clear.
     * @remarks Clients forward to `ServerRequestUnpossess`; server executes directly.
     */
    void RequestUnpossess();

    /**
     * @brief Server-authoritative possession RPC endpoint.
     * @param Target Desired possession target, or null handle to clear possession.
     * @warning Intended to be invoked by the RPC system or authoritative code. Client code should prefer `RequestPossess()`.
     */
    void ServerRequestPossess(const NodeHandle& Target);

    /**
     * @brief Server-authoritative unpossession RPC endpoint.
     * @warning Intended to be invoked by the RPC system or authoritative code. Client code should prefer `RequestUnpossess()`.
     */
    void ServerRequestUnpossess();

private:
    void DispatchPossessionTransition(const NodeHandle& PreviousTarget, const NodeHandle& NewTarget);

    bool CanPossessTarget(const NodeHandle& Target) const;

    unsigned int m_playerIndex = 0;
    NodeHandle m_possessedNode{};
    NodeHandle m_lastNotifiedPossessedNode{};
    bool m_acceptInput = true;
    std::uint64_t m_ownerConnectionId = 0;

#if defined(SNAPI_GF_ENABLE_INPUT)
    SnAPI::Input::DeviceId m_assignedInputDevice{};
    bool m_useAssignedInputDevice = false;
#endif
};

} // namespace SnAPI::GameFramework
