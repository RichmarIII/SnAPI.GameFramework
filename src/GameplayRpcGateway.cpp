#include "GameplayRpcGateway.h"

#include "GameplayHost.h"
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetRpc.h"
#endif
#include "Profiling.h"
#include "World.h"

#include <optional>

namespace SnAPI::GameFramework
{

GameplayRpcGateway::GameplayRpcGateway()
{
    TypeKey(StaticTypeId<GameplayRpcGateway>());
    Replicated(false);
}

GameplayRpcGateway::GameplayRpcGateway(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<GameplayRpcGateway>());
    Replicated(false);
}

const Uuid& GameplayRpcGateway::GatewayNodeId()
{
    static const Uuid GatewayId = [] {
        const auto Parsed = Uuid::from_string("5a6bb5f2-6484-47f8-a863-a5f4c103f8c1");
        return Parsed.value_or(Uuid{});
    }();
    return GatewayId;
}

void GameplayRpcGateway::ServerRequestJoinPlayer(std::string RequestedName,
                                                 const int PreferredPlayerIndex,
                                                 const bool ReplicatedPlayer)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return;
    }

#if !defined(SNAPI_GF_ENABLE_NETWORKING)
    (void)RequestedName;
    (void)PreferredPlayerIndex;
    (void)ReplicatedPlayer;
    return;
#else
    const auto CallerConnection = NetRpcInvocationContext::CurrentConnection();
    if (!CallerConnection)
    {
        return;
    }

    GameplayHost* Host = ResolveGameplayHost();
    if (!Host)
    {
        return;
    }

    std::optional<unsigned int> PreferredIndex{};
    if (PreferredPlayerIndex >= 0)
    {
        PreferredIndex = static_cast<unsigned int>(PreferredPlayerIndex);
    }

    (void)Host->HandleJoinPlayerRequest(static_cast<std::uint64_t>(*CallerConnection),
                                        std::move(RequestedName),
                                        PreferredIndex,
                                        ReplicatedPlayer);
#endif
}

void GameplayRpcGateway::ServerRequestLeavePlayer(const int PlayerIndex)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return;
    }

#if !defined(SNAPI_GF_ENABLE_NETWORKING)
    (void)PlayerIndex;
    return;
#else
    const auto CallerConnection = NetRpcInvocationContext::CurrentConnection();
    if (!CallerConnection)
    {
        return;
    }

    GameplayHost* Host = ResolveGameplayHost();
    if (!Host)
    {
        return;
    }

    const std::uint64_t OwnerConnectionId = static_cast<std::uint64_t>(*CallerConnection);
    const std::optional<unsigned int> RequestedIndex =
        (PlayerIndex >= 0) ? std::optional<unsigned int>(static_cast<unsigned int>(PlayerIndex)) : std::nullopt;
    (void)Host->HandleLeavePlayerRequest(OwnerConnectionId, RequestedIndex);
#endif
}

void GameplayRpcGateway::ServerRequestLoadLevel(std::string RequestedName)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return;
    }

#if !defined(SNAPI_GF_ENABLE_NETWORKING)
    (void)RequestedName;
    return;
#else
    const auto CallerConnection = NetRpcInvocationContext::CurrentConnection();
    if (!CallerConnection)
    {
        return;
    }

    GameplayHost* Host = ResolveGameplayHost();
    if (!Host)
    {
        return;
    }

    (void)Host->HandleLoadLevelRequest(static_cast<std::uint64_t>(*CallerConnection), std::move(RequestedName));
#endif
}

void GameplayRpcGateway::ServerRequestUnloadLevel(std::string LevelIdText)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return;
    }

#if !defined(SNAPI_GF_ENABLE_NETWORKING)
    (void)LevelIdText;
    return;
#else
    const auto CallerConnection = NetRpcInvocationContext::CurrentConnection();
    if (!CallerConnection)
    {
        return;
    }

    GameplayHost* Host = ResolveGameplayHost();
    if (!Host)
    {
        return;
    }

    const auto Parsed = Uuid::from_string(LevelIdText);
    if (!Parsed.has_value())
    {
        return;
    }

    (void)Host->HandleUnloadLevelRequest(static_cast<std::uint64_t>(*CallerConnection), *Parsed);
#endif
}

GameplayHost* GameplayRpcGateway::ResolveGameplayHost() const
{
    auto* WorldInterface = World();
    auto* ConcreteWorld = dynamic_cast<GameFramework::World*>(WorldInterface);
    if (!ConcreteWorld)
    {
        return nullptr;
    }
    return ConcreteWorld->GameplayHostPtr();
}

} // namespace SnAPI::GameFramework
