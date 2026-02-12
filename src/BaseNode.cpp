#include "BaseNode.h"

#include "IWorld.h"
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

void BaseNode::TickTree(float DeltaSeconds)
{
    if (!m_ownerGraph)
    {
        Tick(DeltaSeconds);
        return;
    }
    if (!m_ownerGraph->IsNodeActive(m_self))
    {
        return;
    }

    Tick(DeltaSeconds);
    m_ownerGraph->TickComponents(m_self, DeltaSeconds);

    for (const auto& Child : m_children)
    {
        if (auto* ChildNode = Child.Borrowed())
        {
            ChildNode->TickTree(DeltaSeconds);
        }
    }
}

void BaseNode::FixedTickTree(float DeltaSeconds)
{
    if (!m_ownerGraph)
    {
        FixedTick(DeltaSeconds);
        return;
    }
    if (!m_ownerGraph->IsNodeActive(m_self))
    {
        return;
    }

    FixedTick(DeltaSeconds);
    m_ownerGraph->FixedTickComponents(m_self, DeltaSeconds);

    for (const auto& Child : m_children)
    {
        if (auto* ChildNode = Child.Borrowed())
        {
            ChildNode->FixedTickTree(DeltaSeconds);
        }
    }
}

void BaseNode::LateTickTree(float DeltaSeconds)
{
    if (!m_ownerGraph)
    {
        LateTick(DeltaSeconds);
        return;
    }
    if (!m_ownerGraph->IsNodeActive(m_self))
    {
        return;
    }

    LateTick(DeltaSeconds);
    m_ownerGraph->LateTickComponents(m_self, DeltaSeconds);

    for (const auto& Child : m_children)
    {
        if (auto* ChildNode = Child.Borrowed())
        {
            ChildNode->LateTickTree(DeltaSeconds);
        }
    }
}

bool BaseNode::IsServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsServer();
    }
#endif
    return true;
}

bool BaseNode::IsClient() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsClient();
    }
#endif
    return false;
}

bool BaseNode::IsListenServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsListenServer();
    }
#endif
    return false;
}

} // namespace SnAPI::GameFramework
