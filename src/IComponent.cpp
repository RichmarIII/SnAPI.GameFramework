#include "IComponent.h"

#include "BaseNode.h"
#include "IWorld.h"
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif

namespace SnAPI::GameFramework
{

BaseNode* IComponent::OwnerNode() const
{
    return m_owner.Borrowed();
}

IWorld* IComponent::World() const
{
    auto* Node = OwnerNode();
    if (!Node)
    {
        return nullptr;
    }
    return Node->World();
}

bool IComponent::IsServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (auto* WorldPtr = World())
    {
        return WorldPtr->Networking().IsServer();
    }
#endif
    return true;
}

bool IComponent::IsClient() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (auto* WorldPtr = World())
    {
        return WorldPtr->Networking().IsClient();
    }
#endif
    return false;
}

bool IComponent::IsListenServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (auto* WorldPtr = World())
    {
        return WorldPtr->Networking().IsListenServer();
    }
#endif
    return false;
}

} // namespace SnAPI::GameFramework
