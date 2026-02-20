#pragma once

#include <cstdint>
#include <string>

#include "BaseNode.h"
#include "Export.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @brief RPC gateway node for client-requested gameplay authority actions.
 * @remarks
 * Created with a deterministic UUID so clients can always target the same node
 * id when issuing server RPC join/leave requests.
 */
class SNAPI_GAMEFRAMEWORK_API GameplayRpcGateway final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::GameplayRpcGateway";

    GameplayRpcGateway();
    explicit GameplayRpcGateway(std::string Name);

    /**
     * @brief Deterministic UUID used by all runtimes for the gateway node.
     */
    [[nodiscard]] static const Uuid& GatewayNodeId();

    /**
     * @brief Server-authoritative player join request endpoint.
     * @param RequestedName Optional preferred player node name.
     * @param PreferredPlayerIndex Player index, or `-1` for auto-assignment.
     * @param ReplicatedPlayer Replication state for the created local-player.
     */
    void ServerRequestJoinPlayer(std::string RequestedName, int PreferredPlayerIndex, bool ReplicatedPlayer);

    /**
     * @brief Server-authoritative player leave request endpoint.
     * @param PlayerIndex Player index to remove, or `-1` for all caller-owned players.
     */
    void ServerRequestLeavePlayer(int PlayerIndex);

    /**
     * @brief Server-authoritative level load request endpoint.
     * @param RequestedName Optional level node name.
     */
    void ServerRequestLoadLevel(std::string RequestedName);

    /**
     * @brief Server-authoritative level unload request endpoint.
     * @param LevelIdText UUID string of the target level node.
     */
    void ServerRequestUnloadLevel(std::string LevelIdText);

private:
    class GameplayHost* ResolveGameplayHost() const;
};

} // namespace SnAPI::GameFramework
