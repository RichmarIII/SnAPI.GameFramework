#pragma once

#if defined(SNAPI_GF_ENABLE_NETWORKING)

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Expected.h"
#include "GameThreading.h"
#include "NetSession.h"
#include "Services/ReplicationService.h"
#include "Services/RpcService.h"
#include "Transport/UdpTransportAsio.h"

#include "NetReplication.h"
#include "NetRpc.h"

namespace SnAPI::GameFramework
{

class IWorld;

/**
 * @brief Settings used when `NetworkSystem` owns the session/transport.
 * @remarks
 * This is the world-owned bootstrap path used by `GameRuntime`.
 */
struct NetworkBootstrapSettings
{
    SnAPI::Networking::ESessionRole Role = SnAPI::Networking::ESessionRole::Server; /**< @brief Session role (server/client/listen). */
    SnAPI::Networking::NetConfig Net{}; /**< @brief Session config used to construct `NetSession`. */
    SnAPI::Networking::UdpTransportConfig Transport{}; /**< @brief UDP transport config. */
    std::string BindAddress = "0.0.0.0"; /**< @brief Local bind address. */
    std::uint16_t BindPort = 7777; /**< @brief Local bind port. */
    std::string ConnectAddress = "127.0.0.1"; /**< @brief Remote server address for client/connect mode. */
    std::uint16_t ConnectPort = 7777; /**< @brief Remote server port for client/connect mode. */
    bool AutoConnect = true; /**< @brief Auto-open client connection for client/listen roles. */
    SnAPI::Networking::RpcTargetId RpcTargetId = 1; /**< @brief RPC target id namespace/channel. */
    std::vector<SnAPI::Networking::INetSessionListener*> SessionListeners{}; /**< @brief Optional listeners (not owned). */
};

/**
 * @brief World-owned networking subsystem for replication and reflection RPC.
 * @remarks
 * Binds SnAPI.Networking session/services to a graph-aware runtime bridge layer:
 * - `NetReplicationBridge` for spawn/update/despawn reflection replication
 * - `NetRpcBridge` for reflection-driven RPC routing on nodes/components
 *
 * Lifecycle and ownership:
 * - Owned by `World`.
 * - Owns networking session/transport lifecycle.
 * - Service/bridge objects are owned by this subsystem once attached.
 */
class NetworkSystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(NetworkSystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;
    /**
     * @brief Construct the system for a world context.
     * @param WorldRef World used for replication and RPC target resolution.
     */
    explicit NetworkSystem(IWorld& WorldRef);

    /**
     * @brief Enqueue work on the networking system thread.
     * @param InTask Work callback executed on networking-thread affinity.
     * @param OnComplete Optional completion callback marshaled to caller dispatcher.
     * @return Task handle for wait/cancel polling.
     */
    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});

    /**
     * @brief Enqueue a generic thread task for dispatcher marshalling.
     * @param InTask Callback to execute on this system thread.
     */
    void EnqueueThreadTask(std::function<void()> InTask) override;

    /**
     * @brief Execute all queued tasks on the networking thread.
     */
    void ExecuteQueuedTasks();

    /**
     * @brief Initialize and own a session + UDP transport for this world.
     * @param Settings Bootstrap settings.
     * @return Success or error.
     */
    Result InitializeOwnedSession(const NetworkBootstrapSettings& Settings);

    /**
     * @brief Shutdown owned session/transport and clear attachment state.
     */
    void ShutdownOwnedSession();

    /**
     * @brief Access the attached session.
     * @return Session pointer or nullptr when detached.
     * @remarks Session is owned by this subsystem.
     */
    SnAPI::Networking::NetSession* Session() const
    {
        return m_session;
    }

    /**
     * @brief Access owned UDP transport (if initialized via owned-session path).
     * @return Shared transport pointer or null.
     */
    std::shared_ptr<SnAPI::Networking::UdpTransportAsio> Transport() const
    {
        return m_transport;
    }

    /**
     * @brief Access the replication service.
     * @return Shared pointer to ReplicationService.
     * @remarks Null until networking is initialized.
     */
    const std::shared_ptr<SnAPI::Networking::ReplicationService>& Replication() const
    {
        return m_replication;
    }

    /**
     * @brief Access the RPC service.
     * @return Shared pointer to RpcService.
     * @remarks Null until networking is initialized.
     */
    const std::shared_ptr<SnAPI::Networking::RpcService>& Rpc() const
    {
        return m_rpc;
    }

    /**
     * @brief Access replication bridge.
     * @return Bridge pointer.
     * @remarks Null until wiring completes.
     */
    NetReplicationBridge* ReplicationBridge() const
    {
        return m_replicationBridge.get();
    }

    /**
     * @brief Access RPC bridge.
     * @return Bridge pointer.
     * @remarks Null until wiring completes.
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
    bool WireSession(SnAPI::Networking::NetSession& Session,
                     SnAPI::Networking::RpcTargetId TargetIdValue);

    mutable GameMutex m_threadMutex{}; /**< @brief Networking-system thread affinity guard. */
    TSystemTaskQueue<NetworkSystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    IWorld* m_world = nullptr; /**< @brief Non-owning world context used by replication/rpc bridges. */
    SnAPI::Networking::NetSession* m_session = nullptr; /**< @brief Attached session pointer (owned). */
    std::unique_ptr<SnAPI::Networking::NetSession> m_ownedSession{}; /**< @brief Owned session for bootstrap path. */
    std::shared_ptr<SnAPI::Networking::UdpTransportAsio> m_transport{}; /**< @brief Owned UDP transport for bootstrap path. */
    std::shared_ptr<SnAPI::Networking::ReplicationService> m_replication{}; /**< @brief Session replication service instance. */
    std::shared_ptr<SnAPI::Networking::RpcService> m_rpc{}; /**< @brief Session RPC service instance. */
    std::unique_ptr<NetReplicationBridge> m_replicationBridge{}; /**< @brief Graph replication adapter owned by subsystem. */
    std::unique_ptr<NetRpcBridge> m_rpcBridge{}; /**< @brief Graph RPC adapter owned by subsystem. */
    SnAPI::Networking::RpcTargetId m_rpcTargetId = 1; /**< @brief RPC target namespace/channel id used for bridge binding. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_NETWORKING
