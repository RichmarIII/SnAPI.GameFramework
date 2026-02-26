#include "LocalPlayer.h"

#include <array>

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetRpc.h"
#endif

#include "BaseNode.h"
#include "Profiling.h"
#include "TypeRegistry.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{
namespace
{
const MethodInfo* FindPossessionCallback(const TypeId& Type,
                                         const std::string_view MethodName,
                                         const std::span<const Variant> Args)
{
    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return nullptr;
    }

    for (const auto& Method : Info->Methods)
    {
        if (Method.Name != MethodName || Method.ParamTypes.size() != Args.size())
        {
            continue;
        }

        bool Match = true;
        for (std::size_t Index = 0; Index < Args.size(); ++Index)
        {
            if (Method.ParamTypes[Index] != Args[Index].Type())
            {
                Match = false;
                break;
            }
        }

        if (Match)
        {
            return &Method;
        }
    }

    for (const TypeId& BaseType : Info->BaseTypes)
    {
        if (const MethodInfo* Method = FindPossessionCallback(BaseType, MethodName, Args))
        {
            return Method;
        }
    }

    return nullptr;
}

void InvokePossessionCallback(BaseNode* Node, const std::string_view MethodName, const NodeHandle& PlayerHandle)
{
    if (!Node)
    {
        return;
    }

    const std::array<Variant, 1> Args{Variant::FromValue(PlayerHandle)};
    const std::span<const Variant> ArgsSpan{Args};
    const MethodInfo* Method = FindPossessionCallback(Node->TypeKey(), MethodName, ArgsSpan);
    if (!Method)
    {
        return;
    }

    (void)Method->Invoke(Node, ArgsSpan);
}
} // namespace

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

void LocalPlayer::SetPossessedNode(const NodeHandle& Target)
{
    if (Target == m_possessedNode)
    {
        return;
    }

    const NodeHandle PreviousTarget = m_possessedNode;
    m_possessedNode = Target;
    DispatchPossessionTransition(PreviousTarget, m_possessedNode);
}

void LocalPlayer::SyncPossessionCallbacks()
{
    if (m_lastNotifiedPossessedNode == m_possessedNode)
    {
        return;
    }

    DispatchPossessionTransition(m_lastNotifiedPossessedNode, m_possessedNode);
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

void LocalPlayer::RequestPossess(const NodeHandle& Target)
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

void LocalPlayer::ServerRequestPossess(const NodeHandle& Target)
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
    SetPossessedNode(Target);
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
    SetPossessedNode({});
}

bool LocalPlayer::CanPossessTarget(const NodeHandle& Target) const
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

void LocalPlayer::DispatchPossessionTransition(const NodeHandle& PreviousTarget, const NodeHandle& NewTarget)
{
    if (PreviousTarget == NewTarget)
    {
        return;
    }

    if (BaseNode* PreviousNode = PreviousTarget.Borrowed(); PreviousNode && PreviousNode->World() == World())
    {
        InvokePossessionCallback(PreviousNode, "OnUnpossess", Handle());
    }

    if (BaseNode* NewNode = NewTarget.Borrowed(); NewNode && NewNode->World() == World())
    {
        InvokePossessionCallback(NewNode, "OnPossess", Handle());
    }

    m_lastNotifiedPossessedNode = m_possessedNode;
}

} // namespace SnAPI::GameFramework
