#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetSession.h"
#endif

using namespace SnAPI::GameFramework;

namespace
{

struct TestPolicyGame final : IGame
{
    bool AllowJoinRequests = true;
    bool AllowLeaveRequests = true;
    bool AllowLoadRequests = true;
    bool AllowUnloadRequests = true;

    int JoinPolicyCalls = 0;
    int LeavePolicyCalls = 0;
    int LoadPolicyCalls = 0;
    int UnloadPolicyCalls = 0;
    int SelectPossessionCalls = 0;
    int ConnectionAddedCalls = 0;
    int ConnectionRemovedCalls = 0;

    std::uint64_t LastOwnerConnectionId = 0;
    std::uint64_t LastConnectionAdded = 0;
    std::uint64_t LastConnectionRemoved = 0;

    NodeHandle InitialPossessionTarget{};

    [[nodiscard]] std::string_view Name() const override
    {
        return "TestPolicyGame";
    }

    Result Initialize(GameplayHost& Host) override
    {
        (void)Host;
        return Ok();
    }

    bool AllowPlayerJoinRequest(GameplayHost& Host,
                                const std::uint64_t OwnerConnectionId,
                                const std::string& RequestedName,
                                const std::optional<unsigned int> PreferredPlayerIndex,
                                const bool ReplicatedPlayer) override
    {
        (void)Host;
        (void)RequestedName;
        (void)PreferredPlayerIndex;
        (void)ReplicatedPlayer;
        ++JoinPolicyCalls;
        LastOwnerConnectionId = OwnerConnectionId;
        return AllowJoinRequests;
    }

    bool AllowPlayerLeaveRequest(GameplayHost& Host,
                                 const std::uint64_t OwnerConnectionId,
                                 const std::optional<unsigned int> PlayerIndex) override
    {
        (void)Host;
        (void)PlayerIndex;
        ++LeavePolicyCalls;
        LastOwnerConnectionId = OwnerConnectionId;
        return AllowLeaveRequests;
    }

    bool AllowLevelLoadRequest(GameplayHost& Host,
                               const std::uint64_t OwnerConnectionId,
                               const std::string& RequestedName) override
    {
        (void)Host;
        (void)RequestedName;
        ++LoadPolicyCalls;
        LastOwnerConnectionId = OwnerConnectionId;
        return AllowLoadRequests;
    }

    bool AllowLevelUnloadRequest(GameplayHost& Host, const std::uint64_t OwnerConnectionId, const Uuid& LevelId) override
    {
        (void)Host;
        (void)LevelId;
        ++UnloadPolicyCalls;
        LastOwnerConnectionId = OwnerConnectionId;
        return AllowUnloadRequests;
    }

    NodeHandle SelectInitialPossessionTarget(GameplayHost& Host, LocalPlayer& Player) override
    {
        (void)Host;
        (void)Player;
        ++SelectPossessionCalls;
        return InitialPossessionTarget;
    }

    void OnConnectionAdded(GameplayHost& Host, const std::uint64_t OwnerConnectionId) override
    {
        (void)Host;
        ++ConnectionAddedCalls;
        LastConnectionAdded = OwnerConnectionId;
    }

    void OnConnectionRemoved(GameplayHost& Host, const std::uint64_t OwnerConnectionId) override
    {
        (void)Host;
        ++ConnectionRemovedCalls;
        LastConnectionRemoved = OwnerConnectionId;
    }

    void Shutdown(GameplayHost& Host) override
    {
        (void)Host;
    }
};

struct TestPolicyMode final : IGameMode
{
    bool AllowJoinRequests = true;
    int JoinPolicyCalls = 0;

    [[nodiscard]] std::string_view Name() const override
    {
        return "TestPolicyMode";
    }

    Result Initialize(GameplayHost& Host) override
    {
        (void)Host;
        return Ok();
    }

    bool AllowPlayerJoinRequest(GameplayHost& Host,
                                const std::uint64_t OwnerConnectionId,
                                const std::string& RequestedName,
                                const std::optional<unsigned int> PreferredPlayerIndex,
                                const bool ReplicatedPlayer) override
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)RequestedName;
        (void)PreferredPlayerIndex;
        (void)ReplicatedPlayer;
        ++JoinPolicyCalls;
        return AllowJoinRequests;
    }

    void Shutdown(GameplayHost& Host) override
    {
        (void)Host;
    }
};

struct TestPolicyService final : IGameService
{
    bool AllowJoinRequests = true;
    int JoinPolicyCalls = 0;

    [[nodiscard]] std::string_view Name() const override
    {
        return "TestPolicyService";
    }

    Result Initialize(GameplayHost& Host) override
    {
        (void)Host;
        return Ok();
    }

    bool AllowPlayerJoinRequest(GameplayHost& Host,
                                const std::uint64_t OwnerConnectionId,
                                const std::string& RequestedName,
                                const std::optional<unsigned int> PreferredPlayerIndex,
                                const bool ReplicatedPlayer) override
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)RequestedName;
        (void)PreferredPlayerIndex;
        (void)ReplicatedPlayer;
        ++JoinPolicyCalls;
        return AllowJoinRequests;
    }

    void Shutdown(GameplayHost& Host) override
    {
        (void)Host;
    }
};

GameRuntimeSettings MakeGameplaySettings()
{
    GameRuntimeSettings Settings{};
    Settings.WorldName = "GameplayHostTestsWorld";
    Settings.RegisterBuiltins = true;

    GameRuntimeGameplaySettings Gameplay{};
    Gameplay.AutoCreateLocalPlayer = false;
    Gameplay.RegisterDefaultLocalPlayerService = false;
    Gameplay.AutoCreateRemotePlayerOnConnection = false;
    Gameplay.AutoDestroyRemotePlayersOnDisconnect = false;
    Settings.Gameplay = Gameplay;

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    Settings.Networking.reset();
#endif

    return Settings;
}

#if defined(SNAPI_GF_ENABLE_NETWORKING)
GameRuntimeSettings MakeNetworkedGameplaySettings(const SnAPI::Networking::ESessionRole Role)
{
    GameRuntimeSettings Settings = MakeGameplaySettings();

    GameRuntimeNetworkingSettings Net{};
    Net.Role = Role;
    Net.Net.Threading.UseInternalThreads = false;
    Net.Net.KeepAlive.Interval = SnAPI::Networking::Milliseconds{50};
    Net.Net.KeepAlive.Timeout = SnAPI::Networking::Milliseconds{2500};
    Net.Net.Reliability.ResendTimeout = SnAPI::Networking::Milliseconds{50};
    Net.BindAddress = "127.0.0.1";
    Net.BindPort = 0;
    Net.AutoConnect = false;
    Settings.Networking = Net;
    return Settings;
}
#endif

} // namespace

TEST_CASE("GameplayHost join/leave is owner-aware and index-stable")
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings = MakeGameplaySettings();
    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.Gameplay() != nullptr);

    auto* Host = Runtime.Gameplay();
    REQUIRE(Host->HandleJoinPlayerRequest(11, "Owner11P0", std::optional<unsigned int>{0U}, true));
    REQUIRE(Host->HandleJoinPlayerRequest(12, "Owner12P0", std::optional<unsigned int>{0U}, true));
    REQUIRE(Host->HandleJoinPlayerRequest(11, "Owner11P0Duplicate", std::optional<unsigned int>{0U}, true));

    REQUIRE(Host->LocalPlayers().size() == 2);
    REQUIRE(Host->LocalPlayersForConnection(11).size() == 1);
    REQUIRE(Host->LocalPlayersForConnection(12).size() == 1);

    REQUIRE(Host->HandleLeavePlayerRequest(11, std::optional<unsigned int>{0U}));
    REQUIRE(Host->LocalPlayers().size() == 1);
    REQUIRE(Host->LocalPlayersForConnection(11).empty());
    REQUIRE(Host->LocalPlayersForConnection(12).size() == 1);

    REQUIRE(Host->HandleLeavePlayerRequest(12, std::nullopt));
    REQUIRE(Host->LocalPlayers().empty());
}

TEST_CASE("GameplayHost applies game/mode/service join policy hooks")
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings = MakeGameplaySettings();
    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.Gameplay() != nullptr);
    auto* Host = Runtime.Gameplay();

    auto Game = std::make_unique<TestPolicyGame>();
    auto* GamePtr = Game.get();
    REQUIRE(Host->SetGame(std::move(Game)));

    auto Mode = std::make_unique<TestPolicyMode>();
    auto* ModePtr = Mode.get();
    REQUIRE(Host->SetServerGameMode(std::move(Mode)));

    auto Service = std::make_unique<TestPolicyService>();
    auto* ServicePtr = Service.get();
    REQUIRE(Host->RegisterService(std::move(Service)));

    ServicePtr->AllowJoinRequests = false;
    const Result JoinDeniedByService = Host->HandleJoinPlayerRequest(0, "BlockedByService", std::nullopt, true);
    REQUIRE_FALSE(JoinDeniedByService);
    REQUIRE(JoinDeniedByService.error().Code == EErrorCode::InvalidArgument);
    REQUIRE(GamePtr->JoinPolicyCalls == 1);
    REQUIRE(ModePtr->JoinPolicyCalls == 1);
    REQUIRE(ServicePtr->JoinPolicyCalls == 1);
    REQUIRE(Host->LocalPlayers().empty());

    ServicePtr->AllowJoinRequests = true;
    ModePtr->AllowJoinRequests = false;
    const Result JoinDeniedByMode = Host->HandleJoinPlayerRequest(0, "BlockedByMode", std::nullopt, true);
    REQUIRE_FALSE(JoinDeniedByMode);
    REQUIRE(JoinDeniedByMode.error().Code == EErrorCode::InvalidArgument);
    REQUIRE(GamePtr->JoinPolicyCalls == 2);
    REQUIRE(ModePtr->JoinPolicyCalls == 2);
    REQUIRE(ServicePtr->JoinPolicyCalls == 1);
    REQUIRE(Host->LocalPlayers().empty());

    ModePtr->AllowJoinRequests = true;
    GamePtr->AllowLoadRequests = false;
    const Result LoadDeniedByGame = Host->HandleLoadLevelRequest(0, "DeniedLevel");
    REQUIRE_FALSE(LoadDeniedByGame);
    REQUIRE(LoadDeniedByGame.error().Code == EErrorCode::InvalidArgument);
    REQUIRE(GamePtr->LoadPolicyCalls == 1);
}

TEST_CASE("GameplayHost uses game possession selector for newly joined players")
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings = MakeGameplaySettings();
    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.Gameplay() != nullptr);
    auto* Host = Runtime.Gameplay();

    auto PawnResult = Runtime.World().CreateNode("PossessionPawn");
    REQUIRE(PawnResult);
    const NodeHandle PawnHandle = PawnResult.value();

    auto Game = std::make_unique<TestPolicyGame>();
    auto* GamePtr = Game.get();
    GamePtr->InitialPossessionTarget = PawnHandle;
    REQUIRE(Host->SetGame(std::move(Game)));

    REQUIRE(Host->HandleJoinPlayerRequest(0, "Player", std::nullopt, true));
    REQUIRE(Host->LocalPlayers().size() == 1);
    REQUIRE(GamePtr->SelectPossessionCalls >= 1);

    const NodeHandle PlayerHandle = Host->LocalPlayers().front();
    auto* Player = dynamic_cast<LocalPlayer*>(PlayerHandle.Borrowed());
    REQUIRE(Player != nullptr);
    REQUIRE_FALSE(Player->GetPossessedNode().IsNull());
    REQUIRE(Player->GetPossessedNode().Id == PawnHandle.Id);
}

#if defined(SNAPI_GF_ENABLE_NETWORKING)
TEST_CASE("GameplayHost client role rejects direct authoritative handlers")
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings = MakeNetworkedGameplaySettings(SnAPI::Networking::ESessionRole::Client);
    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.Gameplay() != nullptr);
    auto* Host = Runtime.Gameplay();

    REQUIRE(Host->IsClient());
    REQUIRE_FALSE(Host->IsServer());

    const Result DirectJoin = Host->HandleJoinPlayerRequest(0, "ClientLocal", std::nullopt, true);
    REQUIRE_FALSE(DirectJoin);
    REQUIRE(DirectJoin.error().Code == EErrorCode::InvalidArgument);

    const Result RequestJoin = Host->RequestJoinPlayer("ClientLocal", std::nullopt, true);
    REQUIRE_FALSE(RequestJoin);
    REQUIRE(Host->LocalPlayers().empty());
}

TEST_CASE("GameplayHost emits connection lifecycle callbacks from networking session changes")
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings = MakeNetworkedGameplaySettings(SnAPI::Networking::ESessionRole::Server);
    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.Gameplay() != nullptr);
    auto* Host = Runtime.Gameplay();

    auto Game = std::make_unique<TestPolicyGame>();
    auto* GamePtr = Game.get();
    REQUIRE(Host->SetGame(std::move(Game)));

    auto* Session = Runtime.World().Networking().Session();
    auto Transport = Runtime.World().Networking().Transport();
    REQUIRE(Session != nullptr);
    REQUIRE(Transport != nullptr);

    constexpr SnAPI::Networking::NetConnectionHandle kConnectionHandle = 1337;
    const auto OpenedHandle = Session->OpenConnection(
        Transport->Handle(), SnAPI::Networking::NetEndpoint{"127.0.0.1", 49152}, kConnectionHandle);
    REQUIRE(OpenedHandle == kConnectionHandle);

    Host->Tick(0.0f);
    REQUIRE(GamePtr->ConnectionAddedCalls == 1);
    REQUIRE(GamePtr->LastConnectionAdded == kConnectionHandle);

    REQUIRE(Session->CloseConnection(kConnectionHandle));
    Host->Tick(0.0f);
    REQUIRE(GamePtr->ConnectionRemovedCalls == 1);
    REQUIRE(GamePtr->LastConnectionRemoved == kConnectionHandle);
}
#endif
