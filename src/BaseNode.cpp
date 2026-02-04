#include "BaseNode.h"

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

} // namespace SnAPI::GameFramework
