#include "BaseNode.h"

#include "IWorld.h"
#include "Profiling.h"
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
    const bool IsActive = [&]() {
        return m_ownerGraph->IsNodeActive(*this);
    }();
    if (!IsActive)
    {
        return;
    }

    {
    Tick(DeltaSeconds);
    }

    {
        for (size_t ChildIndex = 0; ChildIndex < m_children.size(); ++ChildIndex)
        {
            BaseNode* ChildNode = (ChildIndex < m_childNodes.size()) ? m_childNodes[ChildIndex] : nullptr;
            if (!ChildNode)
            {
                ChildNode = m_ownerGraph->NodePool().Borrowed(m_children[ChildIndex]);
                if (m_childNodes.size() < m_children.size())
                {
                    m_childNodes.resize(m_children.size(), nullptr);
                }
                m_childNodes[ChildIndex] = ChildNode;
            }
            if (!ChildNode)
            {
                continue;
            }

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

    const bool IsActive = [&]() {
        return m_ownerGraph->IsNodeActive(*this);
    }();
    if (!IsActive)
    {
        return;
    }

    {
        FixedTick(DeltaSeconds);
    }

    {
        for (size_t ChildIndex = 0; ChildIndex < m_children.size(); ++ChildIndex)
        {
            BaseNode* ChildNode = (ChildIndex < m_childNodes.size()) ? m_childNodes[ChildIndex] : nullptr;
            if (!ChildNode)
            {
                ChildNode = m_ownerGraph->NodePool().Borrowed(m_children[ChildIndex]);
                if (m_childNodes.size() < m_children.size())
                {
                    m_childNodes.resize(m_children.size(), nullptr);
                }
                m_childNodes[ChildIndex] = ChildNode;
            }
            if (ChildNode)
            {
                ChildNode->FixedTickTree(DeltaSeconds);
            }
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

    const bool IsActive = [&]() {
        return m_ownerGraph->IsNodeActive(*this);
    }();
    if (!IsActive)
    {
        return;
    }

    {
    LateTick(DeltaSeconds);
    }

    {
        for (size_t ChildIndex = 0; ChildIndex < m_children.size(); ++ChildIndex)
        {
            BaseNode* ChildNode = (ChildIndex < m_childNodes.size()) ? m_childNodes[ChildIndex] : nullptr;
            if (!ChildNode)
            {
                ChildNode = m_ownerGraph->NodePool().Borrowed(m_children[ChildIndex]);
                if (m_childNodes.size() < m_children.size())
                {
                    m_childNodes.resize(m_children.size(), nullptr);
                }
                m_childNodes[ChildIndex] = ChildNode;
            }
            if (!ChildNode)
            {
                continue;
            }

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
