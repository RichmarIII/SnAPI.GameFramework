#pragma once

#include <cstdint>
#include <string>

#include "BaseNode.h"
#include "Export.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Deterministically-addressable RPC gateway node for gameplay authority requests.
 *
 * `GameplayRpcGateway` is the narrow RPC surface used by clients to ask the authoritative
 * side to perform gameplay-host operations such as joining/leaving players or loading/unloading
 * levels. The node is created with a deterministic UUID so all runtimes can target it without
 * discovery or replication of a random identity.
 *
 * Core semantics:
 * - It does not own gameplay logic itself; it forwards validated requests into `GameplayHost`.
 * - It is intentionally non-replicated and resolved by deterministic UUID.
 * - Server endpoint methods are no-ops when called without server authority.
 *
 * @see GameplayHost
 */
class SNAPI_GAMEFRAMEWORK_API GameplayRpcGateway final : public BaseNode, public NodeCRTP<GameplayRpcGateway>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::GameplayRpcGateway";

    GameplayRpcGateway();
    explicit GameplayRpcGateway(std::string Name);

    /**
     * @brief Deterministic UUID used by all runtimes for the gateway node.
     * @return Stable UUID shared by all gameplay runtimes.
     */
    [[nodiscard]] static const Uuid& GatewayNodeId();

    /**
     * @brief Server-authoritative player join request endpoint.
     * @param RequestedName Optional preferred player node name.
     * @param PreferredPlayerIndex Player index, or `-1` for auto-assignment.
     * @param ReplicatedPlayer Replication state for the created local-player.
     * @remarks
     * The caller connection is taken from the active RPC invocation context rather than
     * being trusted from the wire payload.
     */
    void ServerRequestJoinPlayer(std::string RequestedName, int PreferredPlayerIndex, bool ReplicatedPlayer);

    /**
     * @brief Server-authoritative player leave request endpoint.
     * @param PlayerIndex Player index to remove, or `-1` for all caller-owned players.
     * @remarks The caller connection is taken from the active RPC invocation context.
     */
    void ServerRequestLeavePlayer(int PlayerIndex);

    /**
     * @brief Server-authoritative level load request endpoint.
     * @param RequestedName Optional level node name.
     * @remarks The caller connection is taken from the active RPC invocation context.
     */
    void ServerRequestLoadLevel(std::string RequestedName);

    /**
     * @brief Server-authoritative level unload request endpoint.
     * @param LevelIdText UUID string of the target level node.
     * @remarks Invalid UUID text is ignored.
     */
    void ServerRequestUnloadLevel(std::string LevelIdText);

private:
    class GameplayHost* ResolveGameplayHost() const;
};

} // namespace SnAPI::GameFramework
