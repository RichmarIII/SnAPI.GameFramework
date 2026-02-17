#include "NetworkSystem.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)

#include "Profiling.h"

namespace SnAPI::GameFramework
{

NetworkSystem::NetworkSystem(NodeGraph& Graph)
    : m_graph(&Graph)
    , m_replicationBridge(std::make_unique<NetReplicationBridge>(Graph))
    , m_rpcBridge(std::make_unique<NetRpcBridge>(&Graph))
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
}

TaskHandle NetworkSystem::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void NetworkSystem::EnqueueThreadTask(std::function<void()> InTask)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void NetworkSystem::ExecuteQueuedTasks()
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    m_taskQueue.ExecuteQueuedTasks(*this, m_threadMutex);
}

Result NetworkSystem::InitializeOwnedSession(const NetworkBootstrapSettings& Settings)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_threadMutex);
    ShutdownOwnedSession();

    m_ownedSession = std::make_unique<SnAPI::Networking::NetSession>(Settings.Net);
    m_ownedSession->Role(Settings.Role);
    for (auto* Listener : Settings.SessionListeners)
    {
        if (Listener)
        {
            m_ownedSession->AddListener(Listener);
        }
    }

    m_transport = std::make_shared<SnAPI::Networking::UdpTransportAsio>(Settings.Transport);
    if (!m_transport->Open(SnAPI::Networking::NetEndpoint{Settings.BindAddress, Settings.BindPort}))
    {
        ShutdownOwnedSession();
        return std::unexpected(MakeError(EErrorCode::NotReady, "Failed to open UDP transport"));
    }
    m_ownedSession->RegisterTransport(m_transport);

    if (Settings.AutoConnect
        && (Settings.Role == SnAPI::Networking::ESessionRole::Client
            || Settings.Role == SnAPI::Networking::ESessionRole::ServerAndClient))
    {
        m_ownedSession->OpenConnection(m_transport->Handle(),
                                       SnAPI::Networking::NetEndpoint{Settings.ConnectAddress, Settings.ConnectPort});
    }

    if (!WireSession(*m_ownedSession, Settings.RpcTargetId))
    {
        ShutdownOwnedSession();
        return std::unexpected(MakeError(EErrorCode::NotReady, "Failed to wire owned session into world networking"));
    }

    return Ok();
}

bool NetworkSystem::WireSession(SnAPI::Networking::NetSession& Session,
                                SnAPI::Networking::RpcTargetId TargetIdValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    GameLockGuard Lock(m_threadMutex);
    if (m_session && m_session != &Session)
    {
        return false;
    }

    if (!m_session)
    {
        m_session = &Session;
        m_replication = SnAPI::Networking::ReplicationService::Create(Session);
        m_rpc = SnAPI::Networking::RpcService::Create(Session);
        if (!m_replication || !m_rpc)
        {
            m_session = nullptr;
            m_replication.reset();
            m_rpc.reset();
            return false;
        }
        if (m_graph)
        {
            m_rpcBridge = std::make_unique<NetRpcBridge>(m_graph);
        }
    }

    if (!m_replicationBridge || !m_rpcBridge || !m_replication || !m_rpc)
    {
        return false;
    }

    m_replication->EntityProvider(m_replicationBridge.get());
    m_replication->InterestProvider(m_replicationBridge.get());
    m_replication->PriorityProvider(m_replicationBridge.get());
    m_replication->Receiver(m_replicationBridge.get());

    m_rpcTargetId = TargetIdValue;
    if (!m_rpcBridge->Bind(*m_rpc, m_rpcTargetId))
    {
        return false;
    }

    return true;
}

void NetworkSystem::ShutdownOwnedSession()
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_threadMutex);
    m_session = nullptr;
    m_replication.reset();
    m_rpc.reset();
    m_transport.reset();
    m_ownedSession.reset();

    // Reset RPC bridge binding state so a new session can bind cleanly.
    if (m_graph)
    {
        m_rpcBridge = std::make_unique<NetRpcBridge>(m_graph);
    }
    else
    {
        m_rpcBridge.reset();
    }
}

bool NetworkSystem::IsServer() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    GameLockGuard Lock(m_threadMutex);
    if (!m_session)
    {
        return true;
    }
    return m_session->IsServer();
}

bool NetworkSystem::IsClient() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    GameLockGuard Lock(m_threadMutex);
    if (!m_session)
    {
        return false;
    }
    return m_session->IsClient();
}

bool NetworkSystem::IsListenServer() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    GameLockGuard Lock(m_threadMutex);
    if (!m_session)
    {
        return false;
    }
    return m_session->IsServerAndClient();
}

std::vector<SnAPI::Networking::NetConnectionHandle> NetworkSystem::Connections() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    GameLockGuard Lock(m_threadMutex);
    if (!m_session)
    {
        return {};
    }
    return m_session->Connections();
}

std::optional<SnAPI::Networking::NetConnectionHandle> NetworkSystem::PrimaryConnection() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    GameLockGuard Lock(m_threadMutex);
    if (!m_session)
    {
        return std::nullopt;
    }
    const auto Handles = m_session->Connections();
    if (Handles.empty())
    {
        return std::nullopt;
    }
    return Handles.front();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_NETWORKING
