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
 * @remarks
 * Binds SnAPI.Networking session/services to a graph-aware runtime bridge layer:
 * - `NetReplicationBridge` for spawn/update/despawn reflection replication
 * - `NetRpcBridge` for reflection-driven RPC routing on nodes/components
 *
 * Lifecycle and ownership:
 * - Owned by `World`.
 * - Session pointer is non-owning; caller owns session lifetime.
 * - Service/bridge objects are owned by this subsystem once attached.
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
     * @remarks
     * Intended to be called once per subsystem lifetime. Rebinding to a different session
     * is intentionally not part of the current contract.
     */
    bool AttachSession(SnAPI::Networking::NetSession& Session,
                       SnAPI::Networking::RpcTargetId TargetIdValue = 1);

    /**
     * @brief Access the attached session.
     * @return Session pointer or nullptr when detached.
     * @remarks Borrowed pointer only; no ownership transfer.
     */
    SnAPI::Networking::NetSession* Session() const
    {
        return m_session;
    }

    /**
     * @brief Access the replication service.
     * @return Shared pointer to ReplicationService.
     * @remarks Null until `AttachSession` succeeds.
     */
    const std::shared_ptr<SnAPI::Networking::ReplicationService>& Replication() const
    {
        return m_replication;
    }

    /**
     * @brief Access the RPC service.
     * @return Shared pointer to RpcService.
     * @remarks Null until `AttachSession` succeeds.
     */
    const std::shared_ptr<SnAPI::Networking::RpcService>& Rpc() const
    {
        return m_rpc;
    }

    /**
     * @brief Access replication bridge.
     * @return Bridge pointer.
     * @remarks Null until session attach wiring completes.
     */
    NetReplicationBridge* ReplicationBridge() const
    {
        return m_replicationBridge.get();
    }

    /**
     * @brief Access RPC bridge.
     * @return Bridge pointer.
     * @remarks Null until session attach wiring completes.
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
     * @remarks Snapshot of session-visible connections at call time.
     */
    std::vector<SnAPI::Networking::NetConnectionHandle> Connections() const;

    /**
     * @brief Get the first active connection handle.
     * @return Primary connection or nullopt.
     * @remarks Convenience helper for common single-remote client/server setups.
     */
    std::optional<SnAPI::Networking::NetConnectionHandle> PrimaryConnection() const;

private:
    NodeGraph* m_graph = nullptr; /**< @brief Non-owning graph context used by replication/rpc bridges. */
    SnAPI::Networking::NetSession* m_session = nullptr; /**< @brief Non-owning attached session pointer. */
    std::shared_ptr<SnAPI::Networking::ReplicationService> m_replication{}; /**< @brief Session replication service instance. */
    std::shared_ptr<SnAPI::Networking::RpcService> m_rpc{}; /**< @brief Session RPC service instance. */
    std::unique_ptr<NetReplicationBridge> m_replicationBridge{}; /**< @brief Graph replication adapter owned by subsystem. */
    std::unique_ptr<NetRpcBridge> m_rpcBridge{}; /**< @brief Graph RPC adapter owned by subsystem. */
    SnAPI::Networking::RpcTargetId m_rpcTargetId = 1; /**< @brief RPC target namespace/channel id used for bridge binding. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_NETWORKING
