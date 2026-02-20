#include "LocalPlayer.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetRpc.h"
#endif

#include "Profiling.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

LocalPlayer::LocalPlayer()
{
    TypeKey(StaticTypeId<LocalPlayer>());
    Replicated(true);
}

LocalPlayer::LocalPlayer(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<LocalPlayer>());
    Replicated(true);
}

unsigned int& LocalPlayer::EditPlayerIndex()
{
    return m_playerIndex;
}

const unsigned int& LocalPlayer::GetPlayerIndex() const
{
    return m_playerIndex;
}

NodeHandle& LocalPlayer::EditPossessedNode()
{
    return m_possessedNode;
}

const NodeHandle& LocalPlayer::GetPossessedNode() const
{
    return m_possessedNode;
}

bool& LocalPlayer::EditAcceptInput()
{
    return m_acceptInput;
}

const bool& LocalPlayer::GetAcceptInput() const
{
    return m_acceptInput;
}

std::uint64_t& LocalPlayer::EditOwnerConnectionId()
{
    return m_ownerConnectionId;
}

const std::uint64_t& LocalPlayer::GetOwnerConnectionId() const
{
    return m_ownerConnectionId;
}

#if defined(SNAPI_GF_ENABLE_INPUT)
SnAPI::Input::DeviceId& LocalPlayer::EditAssignedInputDevice()
{
    return m_assignedInputDevice;
}

const SnAPI::Input::DeviceId& LocalPlayer::GetAssignedInputDevice() const
{
    return m_assignedInputDevice;
}

bool& LocalPlayer::EditUseAssignedInputDevice()
{
    return m_useAssignedInputDevice;
}

const bool& LocalPlayer::GetUseAssignedInputDevice() const
{
    return m_useAssignedInputDevice;
}
#endif

void LocalPlayer::RequestPossess(const NodeHandle Target)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (CallRPC("ServerRequestPossess", {Variant::FromValue(Target)}))
    {
        return;
    }
    ServerRequestPossess(Target);
}

void LocalPlayer::RequestUnpossess()
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (CallRPC("ServerRequestUnpossess"))
    {
        return;
    }
    ServerRequestUnpossess();
}

void LocalPlayer::ServerRequestPossess(const NodeHandle Target)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return;
    }
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (const auto Caller = NetRpcInvocationContext::CurrentConnection())
    {
        if (m_ownerConnectionId != 0 && m_ownerConnectionId != static_cast<std::uint64_t>(*Caller))
        {
            return;
        }
    }
#endif
    if (!CanPossessTarget(Target))
    {
        return;
    }
    m_possessedNode = Target;
}

void LocalPlayer::ServerRequestUnpossess()
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return;
    }
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (const auto Caller = NetRpcInvocationContext::CurrentConnection())
    {
        if (m_ownerConnectionId != 0 && m_ownerConnectionId != static_cast<std::uint64_t>(*Caller))
        {
            return;
        }
    }
#endif
    m_possessedNode = {};
}

bool LocalPlayer::CanPossessTarget(const NodeHandle Target) const
{
    if (Target.IsNull())
    {
        return true;
    }

    const auto* SelfWorld = World();
    auto* TargetNode = Target.Borrowed();
    if (!SelfWorld || !TargetNode)
    {
        return false;
    }

    return TargetNode->World() == SelfWorld;
}

} // namespace SnAPI::GameFramework
