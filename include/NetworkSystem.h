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
 * @ingroup SnAPI_GameFramework
 * @brief Settings used when `NetworkSystem` owns the session and UDP transport.
 *
 * `NetworkBootstrapSettings` describes the self-contained bootstrap path where
 * GameFramework creates the transport, session, replication bridge, and RPC bridge
 * on behalf of a `World`. This is the standard path used by `GameRuntime`.
 *
 * Core semantics:
 * - `Role` determines whether the resulting session behaves as server, client, or listen server
 * - bind/connect fields are only consumed by `InitializeOwnedSession(...)`
 * - session listeners are borrowed only; ownership stays with the caller
 * - `RpcTargetId` namespaces reflection RPC binding for the world bridge
 *
 * @see NetworkSystem
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
 * @ingroup SnAPI_GameFramework
 * @brief World-owned networking subsystem for replication and reflection RPC.
 *
 * `NetworkSystem` is the world-level adapter between the scene graph/ECS model and
 * SnAPI.Networking session services. It wires replication and reflection RPC to world
 * nodes/components so higher-level gameplay code can reason in terms of `World`,
 * `NodeHandle`, and reflected methods instead of packet formats.
 *
 * Why this abstraction exists:
 * - to keep transport/session lifetime aligned with world lifetime
 * - to centralize bridge setup for replication and RPC
 * - to expose simple authority queries and connection snapshots to gameplay systems
 *
 * Core semantics:
 * - `InitializeOwnedSession(...)` tears down any previous owned session before creating a new one
 * - the system owns the bootstrap path transport/session/services it creates
 * - `Session()`, `Transport()`, `Replication()`, `Rpc()`, and bridge accessors return borrowed handles
 * - when no session is attached, `IsServer()` defaults to `true` so offline worlds behave authoritatively
 *
 * Ownership and lifetime:
 * - Owned by `World`.
 * - Owns the `NetSession` and `UdpTransportAsio` created by the owned-session bootstrap path.
 * - Owns replication/RPC bridge objects.
 * - Listener pointers in `NetworkBootstrapSettings` are borrowed and must outlive session initialization.
 *
 * Threading model:
 * - Main-thread oriented for setup/teardown.
 * - Cross-thread work should be marshaled via `EnqueueTask(...)`.
 *
 * @see World
 * @see NetworkBootstrapSettings
 * @see NetReplicationBridge
 * @see NetRpcBridge
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
     * @brief Initialize and own a session plus UDP transport for this world.
     * @param Settings Bootstrap settings copied during initialization.
     * @return Success or error.
     * @post On success, replication and RPC bridges are bound to the created session.
     * @warning Replaces any previously owned session state.
     */
    Result InitializeOwnedSession(const NetworkBootstrapSettings& Settings);

    /**
     * @brief Shutdown owned session/transport and clear attachment state.
     * @remarks Borrowed session, transport, service, and bridge pointers may change or become null after this call.
     */
    void ShutdownOwnedSession();

    /**
     * @brief Access the attached session.
     * @return Non-owning session pointer or `nullptr` when detached.
     * @remarks Session is owned by this subsystem in the owned-session bootstrap path.
     */
    SnAPI::Networking::NetSession* Session() const
    {
        return m_session;
    }

    /**
     * @brief Access owned UDP transport.
     * @return Shared transport pointer or null.
     * @remarks Non-null only when initialized through the owned-session bootstrap path.
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
     * @return Non-owning bridge pointer.
     * @remarks Null until session wiring completes.
     */
    NetReplicationBridge* ReplicationBridge() const
    {
        return m_replicationBridge.get();
    }

    /**
     * @brief Access RPC bridge.
     * @return Non-owning bridge pointer.
     * @remarks Null until session wiring completes.
     */
    NetRpcBridge* RpcBridge() const
    {
        return m_rpcBridge.get();
    }

    /**
     * @brief Check whether the attached session currently has server authority.
     * @return `true` when the session is server-capable, or when no session is attached.
     */
    bool IsServer() const;
    /**
     * @brief Check whether the attached session currently has client role.
     * @return `true` when the attached session is client-capable.
     */
    bool IsClient() const;
    /**
     * @brief Check whether the attached session is a listen server.
     * @return `true` when the session has both server and client roles.
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
