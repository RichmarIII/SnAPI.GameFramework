#include "NetworkSystem.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)

namespace SnAPI::GameFramework
{

NetworkSystem::NetworkSystem(NodeGraph& Graph)
    : m_graph(&Graph)
    , m_replicationBridge(std::make_unique<NetReplicationBridge>(Graph))
    , m_rpcBridge(std::make_unique<NetRpcBridge>(&Graph))
{
}

bool NetworkSystem::AttachSession(SnAPI::Networking::NetSession& Session,
                                  SnAPI::Networking::RpcTargetId TargetIdValue)
{
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

bool NetworkSystem::IsServer() const
{
    if (!m_session)
    {
        return true;
    }
    return m_session->IsServer();
}

bool NetworkSystem::IsClient() const
{
    if (!m_session)
    {
        return false;
    }
    return m_session->IsClient();
}

bool NetworkSystem::IsListenServer() const
{
    if (!m_session)
    {
        return false;
    }
    return m_session->IsServerAndClient();
}

std::vector<SnAPI::Networking::NetConnectionHandle> NetworkSystem::Connections() const
{
    if (!m_session)
    {
        return {};
    }
    return m_session->Connections();
}

std::optional<SnAPI::Networking::NetConnectionHandle> NetworkSystem::PrimaryConnection() const
{
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

