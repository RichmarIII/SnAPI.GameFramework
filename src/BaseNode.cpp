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
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    if (!m_ownerGraph)
    {
        Tick(DeltaSeconds);
        return;
    }
    const bool IsActive = [&]() {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.Tick.ActiveCheck", "SceneGraph");
        return m_ownerGraph->IsNodeActive(*this);
    }();
    if (!IsActive)
    {
        return;
    }

    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.Tick", "SceneGraph");
    Tick(DeltaSeconds);
    }
    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.TickComponents", "SceneGraph");
    m_ownerGraph->TickComponents(m_self, DeltaSeconds);
    }

    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.TickChildTraversal", "SceneGraph");
        for (const auto& Child : m_children)
        {
            BaseNode* ChildNode = nullptr;
            {
                SNAPI_GF_PROFILE_SCOPE("BaseNode.TickChild.Resolve", "SceneGraph");
                ChildNode = m_ownerGraph->NodePool().Borrowed(Child);
            }
            if (!ChildNode)
            {
                continue;
            }

            SNAPI_GF_PROFILE_SCOPE("BaseNode.TickChild", "SceneGraph");
            ChildNode->TickTree(DeltaSeconds);
        }
    }
}

void BaseNode::FixedTickTree(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    if (!m_ownerGraph)
    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTick.NoGraph", "SceneGraph");
        FixedTick(DeltaSeconds);
        return;
    }

    const bool IsActive = [&]() {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTick.ActiveCheck", "SceneGraph");
        return m_ownerGraph->IsNodeActive(*this);
    }();
    if (!IsActive)
    {
        return;
    }

    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTick", "SceneGraph");
        FixedTick(DeltaSeconds);
    }
    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTickComponents", "SceneGraph");
        m_ownerGraph->FixedTickComponents(m_self, DeltaSeconds);
    }

    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTickChildTraversal", "SceneGraph");
        for (const auto& Child : m_children)
        {
            BaseNode* ChildNode = nullptr;
            {
                SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTickChild.Resolve", "SceneGraph");
                ChildNode = m_ownerGraph->NodePool().Borrowed(Child);
            }
            if (ChildNode)
            {
                SNAPI_GF_PROFILE_SCOPE("BaseNode.FixedTickChild", "SceneGraph");
                ChildNode->FixedTickTree(DeltaSeconds);
            }
        }
    }
}

void BaseNode::LateTickTree(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    if (!m_ownerGraph)
    {
        LateTick(DeltaSeconds);
        return;
    }

    const bool IsActive = [&]() {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.LateTick.ActiveCheck", "SceneGraph");
        return m_ownerGraph->IsNodeActive(*this);
    }();
    if (!IsActive)
    {
        return;
    }

    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.LateTick", "SceneGraph");
    LateTick(DeltaSeconds);
    }
    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.LateTickComponents", "SceneGraph");
    m_ownerGraph->LateTickComponents(m_self, DeltaSeconds);
    }

    {
        SNAPI_GF_PROFILE_SCOPE("BaseNode.LateTickChildTraversal", "SceneGraph");
        for (const auto& Child : m_children)
        {
            BaseNode* ChildNode = nullptr;
            {
                SNAPI_GF_PROFILE_SCOPE("BaseNode.LateTickChild.Resolve", "SceneGraph");
                ChildNode = m_ownerGraph->NodePool().Borrowed(Child);
            }
            if (!ChildNode)
            {
                continue;
            }

            SNAPI_GF_PROFILE_SCOPE("BaseNode.LateTickChild", "SceneGraph");
            ChildNode->LateTickTree(DeltaSeconds);
        }
    }
}

bool BaseNode::IsServer() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
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
    SNAPI_GF_PROFILE_FUNCTION("Networking");
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
    SNAPI_GF_PROFILE_FUNCTION("Networking");
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsListenServer();
    }
#endif
    return false;
}

} // namespace SnAPI::GameFramework
