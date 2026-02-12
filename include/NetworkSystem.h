#pragma once

#if defined(SNAPI_GF_ENABLE_NETWORKING)

#include <memory>
#include <optional>
#include <vector>

#include "NetSession.h"
#include "Services/ReplicationService.h"
#include "Services/RpcService.h"

#include "NetReplication.h"
#include "NetRpc.h"

namespace SnAPI::GameFramework
{

class NodeGraph;

/**
 * @brief World-owned networking subsystem for replication and reflection RPC.
 * @remarks Owns bridges and service handles bound to a single NetSession.
 */
class NetworkSystem final
{
public:
    /**
     * @brief Construct the system for a graph/world.
     * @param Graph Graph used for replication and RPC target resolution.
     */
    explicit NetworkSystem(NodeGraph& Graph);

    /**
     * @brief Attach a NetSession and wire replication/RPC bridges.
     * @param Session Session to attach.
     * @param TargetIdValue Rpc target id used by NetRpcBridge.
     * @return True on success.
     * @remarks Re-attaching to a different session is not supported.
     */
    bool AttachSession(SnAPI::Networking::NetSession& Session,
                       SnAPI::Networking::RpcTargetId TargetIdValue = 1);

    /**
     * @brief Access the attached session.
     * @return Session pointer or nullptr when detached.
     */
    SnAPI::Networking::NetSession* Session() const
    {
        return m_session;
    }

    /**
     * @brief Access the replication service.
     * @return Shared pointer to ReplicationService.
     */
    const std::shared_ptr<SnAPI::Networking::ReplicationService>& Replication() const
    {
        return m_replication;
    }

    /**
     * @brief Access the RPC service.
     * @return Shared pointer to RpcService.
     */
    const std::shared_ptr<SnAPI::Networking::RpcService>& Rpc() const
    {
        return m_rpc;
    }

    /**
     * @brief Access replication bridge.
     * @return Bridge pointer.
     */
    NetReplicationBridge* ReplicationBridge() const
    {
        return m_replicationBridge.get();
    }

    /**
     * @brief Access RPC bridge.
     * @return Bridge pointer.
     */
    NetRpcBridge* RpcBridge() const
    {
        return m_rpcBridge.get();
    }

    /**
     * @brief True when acting as server.
     */
    bool IsServer() const;
    /**
     * @brief True when acting as client.
     */
    bool IsClient() const;
    /**
     * @brief True when acting as listen-server (server+client role).
     */
    bool IsListenServer() const;

    /**
     * @brief Get current connection handles.
     * @return Connection handles or empty when session is detached.
     */
    std::vector<SnAPI::Networking::NetConnectionHandle> Connections() const;

    /**
     * @brief Get the first active connection handle.
     * @return Primary connection or nullopt.
     */
    std::optional<SnAPI::Networking::NetConnectionHandle> PrimaryConnection() const;

private:
    NodeGraph* m_graph = nullptr;
    SnAPI::Networking::NetSession* m_session = nullptr;
    std::shared_ptr<SnAPI::Networking::ReplicationService> m_replication{};
    std::shared_ptr<SnAPI::Networking::RpcService> m_rpc{};
    std::unique_ptr<NetReplicationBridge> m_replicationBridge{};
    std::unique_ptr<NetRpcBridge> m_rpcBridge{};
    SnAPI::Networking::RpcTargetId m_rpcTargetId = 1;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_NETWORKING

