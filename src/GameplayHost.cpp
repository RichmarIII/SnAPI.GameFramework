#include "GameplayHost.h"

#include "Assert.h"
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "CameraComponent.h"
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "CharacterMovementController.h"
#endif
#include "GameRuntime.h"
#include "GameplayRpcGateway.h"
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)
#include "InputComponent.h"
#endif
#include "Level.h"
#include "LocalPlayer.h"
#include "LocalPlayerService.h"
#include "Profiling.h"
#include "Variant.h"
#include "World.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif

#include <algorithm>
#include <limits>
#include <string>

namespace SnAPI::GameFramework
{

namespace
{
[[nodiscard]] bool HasComponentType(const BaseNode& Node, const TypeId& ComponentType)
{
    const auto& Types = Node.ComponentTypes();
    return std::find(Types.begin(), Types.end(), ComponentType) != Types.end();
}

[[nodiscard]] int ScoreAutoPossessTarget(const BaseNode& Node)
{
    if (dynamic_cast<const LocalPlayer*>(&Node) != nullptr)
    {
        return -1;
    }
    if (!Node.Active() || Node.PendingDestroy())
    {
        return -1;
    }

    int Score = 0;
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (HasComponentType(Node, StaticTypeId<CharacterMovementController>()))
    {
        Score += 100;
    }
#endif
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)
    if (HasComponentType(Node, StaticTypeId<InputComponent>()))
    {
        Score += 80;
    }
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (HasComponentType(Node, StaticTypeId<CameraComponent>()))
    {
        Score += 30;
    }
#endif
    return Score;
}

[[nodiscard]] bool IsValidPossessionTargetForPlayer(const LocalPlayer& Player, const NodeHandle Target)
{
    if (Target.IsNull())
    {
        return true;
    }

    const auto* PlayerWorld = Player.World();
    auto* TargetNode = Target.Borrowed();
    if (!PlayerWorld || !TargetNode)
    {
        return false;
    }

    return TargetNode->World() == PlayerWorld;
}
} // namespace

Result GameplayHost::Initialize(GameRuntime& RuntimeRef, const GameRuntimeGameplaySettings& SettingsValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    Shutdown();

    m_runtime = &RuntimeRef;
    m_settings = SettingsValue;

    if (!m_runtime->IsInitialized() || m_runtime->WorldPtr() == nullptr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime must be initialized before gameplay host"));
    }

    if (const Result GatewayResult = EnsureRpcGatewayNode(); !GatewayResult)
    {
        Shutdown();
        return GatewayResult;
    }

    if (m_settings.RegisterDefaultLocalPlayerService)
    {
        (void)RegisterService<LocalPlayerService>();
    }

    if (const Result ServicesResult = InitializeServices(); !ServicesResult)
    {
        Shutdown();
        return ServicesResult;
    }

    if (m_settings.CreateGame)
    {
        std::unique_ptr<IGame> GameInstance = m_settings.CreateGame();
        if (!GameInstance)
        {
            Shutdown();
            return std::unexpected(MakeError(EErrorCode::NotReady, "Game factory returned null"));
        }

        if (const Result GameInit = GameInstance->Initialize(*this); !GameInit)
        {
            Shutdown();
            return GameInit;
        }
        m_game = std::move(GameInstance);
    }

    if (m_settings.AutoCreateLocalPlayer)
    {
        if (const Result AutoPlayerResult = AutoCreateConfiguredLocalPlayer(); !AutoPlayerResult)
        {
            Shutdown();
            return AutoPlayerResult;
        }
    }

    if (IsServer())
    {
        std::unique_ptr<IGameMode> InitialMode{};
        if (m_settings.CreateServerGameMode)
        {
            InitialMode = m_settings.CreateServerGameMode();
        }
        else if (m_game)
        {
            InitialMode = m_game->CreateInitialGameMode(*this);
        }

        if (InitialMode)
        {
            if (const Result ModeResult = SetServerGameMode(std::move(InitialMode)); !ModeResult)
            {
                Shutdown();
                return ModeResult;
            }
        }
    }

    RefreshObservedConnectionState(true);
    if (IsServer() && m_settings.AutoCreateRemotePlayerOnConnection)
    {
        for (const std::uint64_t ConnectionId : m_knownConnectionIds)
        {
            NotifyConnectionAdded(ConnectionId);
        }
    }

    const auto CurrentLevels = World().Levels();
    const auto CurrentPlayers = LocalPlayers();

    if (m_game)
    {
        for (const NodeHandle LevelHandle : CurrentLevels)
        {
            m_game->OnLevelLoaded(*this, LevelHandle);
        }
        for (const NodeHandle PlayerHandle : CurrentPlayers)
        {
            m_game->OnLocalPlayerAdded(*this, PlayerHandle);
        }
    }

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        for (const NodeHandle LevelHandle : CurrentLevels)
        {
            Entry.Instance->OnLevelLoaded(*this, LevelHandle);
        }
        for (const NodeHandle PlayerHandle : CurrentPlayers)
        {
            Entry.Instance->OnLocalPlayerAdded(*this, PlayerHandle);
        }
    }

    RefreshObservedWorldState(true);
    m_initialized = true;
    return Ok();
}

void GameplayHost::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!m_runtime)
    {
        return;
    }

    ShutdownServices();

    if (m_gameMode)
    {
        m_gameMode->Shutdown(*this);
        m_gameMode.reset();
    }

    if (m_game)
    {
        m_game->Shutdown(*this);
        m_game.reset();
    }

    m_knownLevelIds.clear();
    m_knownLocalPlayerIds.clear();
    m_knownConnectionIds.clear();
    m_settings = {};
    m_initialized = false;
    m_runtime = nullptr;
}

bool GameplayHost::IsInitialized() const
{
    return m_initialized && m_runtime != nullptr && m_runtime->IsInitialized();
}

void GameplayHost::Tick(const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsInitialized())
    {
        return;
    }

    RefreshObservedWorldState(false);
    TickServices(DeltaSeconds);

    if (m_game)
    {
        m_game->Tick(*this, DeltaSeconds);
    }

    if (m_gameMode && IsServer())
    {
        m_gameMode->Tick(*this, DeltaSeconds);
    }
}

GameRuntime& GameplayHost::Runtime()
{
    DEBUG_ASSERT(m_runtime != nullptr, "GameplayHost has no runtime");
    return *m_runtime;
}

const GameRuntime& GameplayHost::Runtime() const
{
    DEBUG_ASSERT(m_runtime != nullptr, "GameplayHost has no runtime");
    return *m_runtime;
}

World& GameplayHost::World()
{
    DEBUG_ASSERT(m_runtime != nullptr && m_runtime->WorldPtr() != nullptr, "GameplayHost world is unavailable");
    return m_runtime->World();
}

const World& GameplayHost::World() const
{
    DEBUG_ASSERT(m_runtime != nullptr && m_runtime->WorldPtr() != nullptr, "GameplayHost world is unavailable");
    return m_runtime->World();
}

bool GameplayHost::IsServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_runtime && m_runtime->WorldPtr())
    {
        return m_runtime->World().Networking().IsServer();
    }
#endif
    return true;
}

bool GameplayHost::IsClient() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_runtime && m_runtime->WorldPtr())
    {
        return m_runtime->World().Networking().IsClient();
    }
#endif
    return false;
}

bool GameplayHost::IsListenServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_runtime && m_runtime->WorldPtr())
    {
        return m_runtime->World().Networking().IsListenServer();
    }
#endif
    return false;
}

IGame* GameplayHost::Game() const
{
    return m_game.get();
}

IGameMode* GameplayHost::GameMode() const
{
    return m_gameMode.get();
}

Result GameplayHost::SetGame(std::unique_ptr<IGame> GameInstance)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!m_runtime || m_runtime->WorldPtr() == nullptr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "GameplayHost runtime is unavailable"));
    }

    if (m_game)
    {
        m_game->Shutdown(*this);
        m_game.reset();
    }

    if (!GameInstance)
    {
        return Ok();
    }

    if (const Result InitResult = GameInstance->Initialize(*this); !InitResult)
    {
        return InitResult;
    }

    m_game = std::move(GameInstance);

    const auto CurrentLevels = World().Levels();
    const auto CurrentPlayers = LocalPlayers();
    for (const NodeHandle LevelHandle : CurrentLevels)
    {
        m_game->OnLevelLoaded(*this, LevelHandle);
    }
    for (const NodeHandle PlayerHandle : CurrentPlayers)
    {
        m_game->OnLocalPlayerAdded(*this, PlayerHandle);
    }

    if (IsServer() && !m_gameMode)
    {
        std::unique_ptr<IGameMode> InitialMode = m_game->CreateInitialGameMode(*this);
        if (InitialMode)
        {
            return SetServerGameMode(std::move(InitialMode));
        }
    }

    return Ok();
}

Result GameplayHost::SetServerGameMode(std::unique_ptr<IGameMode> GameModeInstance)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!IsServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "GameMode exists only on server authority"));
    }

    if (m_gameMode)
    {
        m_gameMode->Shutdown(*this);
        m_gameMode.reset();
    }

    if (!GameModeInstance)
    {
        return Ok();
    }

    if (const Result InitResult = GameModeInstance->Initialize(*this); !InitResult)
    {
        return InitResult;
    }

    m_gameMode = std::move(GameModeInstance);

    for (const NodeHandle LevelHandle : World().Levels())
    {
        m_gameMode->OnLevelLoaded(*this, LevelHandle);
    }
    for (const NodeHandle PlayerHandle : LocalPlayers())
    {
        m_gameMode->OnLocalPlayerAdded(*this, PlayerHandle);
    }

    return Ok();
}

Result GameplayHost::ClearServerGameMode()
{
    return SetServerGameMode({});
}

TExpected<GameFramework::NodeHandle> GameplayHost::CreateLocalPlayer(std::string Name,
                                                      const unsigned int PlayerIndex,
                                                      const bool ReplicatedPlayer,
                                                      const std::uint64_t OwnerConnectionId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    std::string EffectiveName = std::move(Name);
    if (EffectiveName.empty())
    {
        EffectiveName = "LocalPlayer";
    }

    auto CreateResult = World().CreateNode<LocalPlayer>(std::move(EffectiveName));
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    NodeHandle Handle = CreateResult.value();
    auto* Player = dynamic_cast<LocalPlayer*>(Handle.Borrowed());
    if (!Player)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Created local-player node type mismatch"));
    }

    Player->EditPlayerIndex() = PlayerIndex;
    Player->EditOwnerConnectionId() = OwnerConnectionId;
    Player->Replicated(ReplicatedPlayer);
    return Handle;
}

TExpected<GameFramework::NodeHandle> GameplayHost::JoinPlayer(const std::uint64_t OwnerConnectionId,
                                               std::string Name,
                                               const std::optional<unsigned int> PreferredPlayerIndex,
                                               const bool ReplicatedPlayer)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "JoinPlayer is server-authoritative"));
    }

    unsigned int PlayerIndex = 0;
    if (PreferredPlayerIndex)
    {
        PlayerIndex = *PreferredPlayerIndex;
    }
    else
    {
        const auto AvailableIndex = FirstAvailablePlayerIndexForOwner(OwnerConnectionId);
        if (!AvailableIndex)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Unable to resolve a free local-player index"));
        }
        PlayerIndex = *AvailableIndex;
    }

    const NodeHandle Existing = FindLocalPlayerByOwnerAndIndex(OwnerConnectionId, PlayerIndex);
    if (!Existing.IsNull())
    {
        if (auto* ExistingPlayer = dynamic_cast<LocalPlayer*>(Existing.Borrowed()))
        {
            EnsurePlayerHasPossession(*ExistingPlayer);
        }
        return Existing;
    }

    std::string EffectiveName = std::move(Name);
    if (EffectiveName.empty())
    {
        if (OwnerConnectionId == 0)
        {
            EffectiveName = "LocalPlayer";
        }
        else
        {
            EffectiveName = std::string("RemotePlayer_") + std::to_string(OwnerConnectionId) + "_" +
                            std::to_string(PlayerIndex);
        }
    }

    auto CreateResult = CreateLocalPlayer(std::move(EffectiveName), PlayerIndex, ReplicatedPlayer, OwnerConnectionId);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    if (auto* Player = dynamic_cast<LocalPlayer*>(CreateResult.value().Borrowed()))
    {
        EnsurePlayerHasPossession(*Player);
    }
    return CreateResult.value();
}

Result GameplayHost::LeavePlayer(const NodeHandle PlayerHandle)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (PlayerHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Local-player handle is null"));
    }
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "LeavePlayer is server-authoritative"));
    }

    auto* Node = PlayerHandle.Borrowed();
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Local-player node not found"));
    }
    if (!dynamic_cast<LocalPlayer*>(Node))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node is not a LocalPlayer"));
    }
    return World().DestroyNode(PlayerHandle);
}

Result GameplayHost::LeavePlayer(const Uuid& PlayerId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    for (const NodeHandle PlayerHandle : LocalPlayers())
    {
        if (PlayerHandle.Id == PlayerId)
        {
            return LeavePlayer(PlayerHandle);
        }
    }
    return std::unexpected(MakeError(EErrorCode::NotFound, "Local-player not found"));
}

Result GameplayHost::LeavePlayersForConnection(const std::uint64_t OwnerConnectionId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "LeavePlayersForConnection is server-authoritative"));
    }

    Result LastResult = Ok();
    bool FoundAny = false;
    for (const NodeHandle PlayerHandle : LocalPlayersForConnection(OwnerConnectionId))
    {
        FoundAny = true;
        if (const Result RemoveResult = LeavePlayer(PlayerHandle); !RemoveResult)
        {
            LastResult = std::unexpected(RemoveResult.error());
        }
    }

    if (!FoundAny)
    {
        return Ok();
    }
    return LastResult;
}

Result GameplayHost::HandleJoinPlayerRequest(const std::uint64_t OwnerConnectionId,
                                             std::string Name,
                                             const std::optional<unsigned int> PreferredPlayerIndex,
                                             const bool ReplicatedPlayer)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Join-player requests are server-authoritative"));
    }

    if (const Result PolicyResult =
            EvaluateJoinRequestPolicy(OwnerConnectionId, Name, PreferredPlayerIndex, ReplicatedPlayer);
        !PolicyResult)
    {
        return PolicyResult;
    }

    auto JoinResult = JoinPlayer(OwnerConnectionId, std::move(Name), PreferredPlayerIndex, ReplicatedPlayer);
    if (!JoinResult)
    {
        return std::unexpected(JoinResult.error());
    }
    return Ok();
}

Result GameplayHost::HandleLeavePlayerRequest(const std::uint64_t OwnerConnectionId,
                                              const std::optional<unsigned int> PlayerIndex)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Leave-player requests are server-authoritative"));
    }

    if (const Result PolicyResult = EvaluateLeaveRequestPolicy(OwnerConnectionId, PlayerIndex); !PolicyResult)
    {
        return PolicyResult;
    }

    if (!PlayerIndex)
    {
        return LeavePlayersForConnection(OwnerConnectionId);
    }

    for (const NodeHandle PlayerHandle : LocalPlayersForConnection(OwnerConnectionId))
    {
        const auto* Player = dynamic_cast<const LocalPlayer*>(PlayerHandle.Borrowed());
        if (!Player)
        {
            continue;
        }
        if (Player->GetPlayerIndex() == *PlayerIndex)
        {
            return LeavePlayer(PlayerHandle);
        }
    }

    return std::unexpected(MakeError(EErrorCode::NotFound, "Requested local-player index was not found"));
}

Result GameplayHost::HandleLoadLevelRequest(const std::uint64_t OwnerConnectionId, std::string Name)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Load-level requests are server-authoritative"));
    }

    if (const Result PolicyResult = EvaluateLoadLevelRequestPolicy(OwnerConnectionId, Name); !PolicyResult)
    {
        return PolicyResult;
    }

    auto LoadResult = LoadLevel(std::move(Name));
    if (!LoadResult)
    {
        return std::unexpected(LoadResult.error());
    }
    return Ok();
}

Result GameplayHost::HandleUnloadLevelRequest(const std::uint64_t OwnerConnectionId, const Uuid& LevelId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(
            MakeError(EErrorCode::InvalidArgument, "Unload-level requests are server-authoritative"));
    }

    if (const Result PolicyResult = EvaluateUnloadLevelRequestPolicy(OwnerConnectionId, LevelId); !PolicyResult)
    {
        return PolicyResult;
    }

    return UnloadLevel(LevelId);
}

Result GameplayHost::RequestJoinPlayer(std::string Name,
                                       const std::optional<unsigned int> PreferredPlayerIndex,
                                       const bool ReplicatedPlayer)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsServer())
    {
        return HandleJoinPlayerRequest(0, std::move(Name), PreferredPlayerIndex, ReplicatedPlayer);
    }

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    GameplayRpcGateway* Gateway = ResolveRpcGatewayNode();
    if (!Gateway)
    {
        if (const Result EnsureResult = EnsureRpcGatewayNode(); !EnsureResult)
        {
            return EnsureResult;
        }
        Gateway = ResolveRpcGatewayNode();
    }
    if (!Gateway)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Gameplay RPC gateway node was not found"));
    }

    const int IndexValue = PreferredPlayerIndex ? static_cast<int>(*PreferredPlayerIndex) : -1;
    if (!Gateway->CallRPC("ServerRequestJoinPlayer",
                          {Variant::FromValue(Name),
                           Variant::FromValue(IndexValue),
                           Variant::FromValue(ReplicatedPlayer)}))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to dispatch join-player RPC request"));
    }

    return Ok();
#else
    (void)Name;
    (void)PreferredPlayerIndex;
    (void)ReplicatedPlayer;
    return std::unexpected(MakeError(EErrorCode::NotReady, "Networking is disabled; no remote authority available"));
#endif
}

Result GameplayHost::RequestLeavePlayer(const std::optional<unsigned int> PlayerIndex)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsServer())
    {
        return HandleLeavePlayerRequest(0, PlayerIndex);
    }

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    GameplayRpcGateway* Gateway = ResolveRpcGatewayNode();
    if (!Gateway)
    {
        if (const Result EnsureResult = EnsureRpcGatewayNode(); !EnsureResult)
        {
            return EnsureResult;
        }
        Gateway = ResolveRpcGatewayNode();
    }
    if (!Gateway)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Gameplay RPC gateway node was not found"));
    }

    const int IndexValue = PlayerIndex ? static_cast<int>(*PlayerIndex) : -1;
    if (!Gateway->CallRPC("ServerRequestLeavePlayer", {Variant::FromValue(IndexValue)}))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to dispatch leave-player RPC request"));
    }

    return Ok();
#else
    (void)PlayerIndex;
    return std::unexpected(MakeError(EErrorCode::NotReady, "Networking is disabled; no remote authority available"));
#endif
}

Result GameplayHost::RequestLoadLevel(std::string Name)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsServer())
    {
        return HandleLoadLevelRequest(0, std::move(Name));
    }

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    GameplayRpcGateway* Gateway = ResolveRpcGatewayNode();
    if (!Gateway)
    {
        if (const Result EnsureResult = EnsureRpcGatewayNode(); !EnsureResult)
        {
            return EnsureResult;
        }
        Gateway = ResolveRpcGatewayNode();
    }
    if (!Gateway)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Gameplay RPC gateway node was not found"));
    }

    if (!Gateway->CallRPC("ServerRequestLoadLevel", {Variant::FromValue(Name)}))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to dispatch load-level RPC request"));
    }
    return Ok();
#else
    (void)Name;
    return std::unexpected(MakeError(EErrorCode::NotReady, "Networking is disabled; no remote authority available"));
#endif
}

Result GameplayHost::RequestUnloadLevel(const Uuid& LevelId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsServer())
    {
        return HandleUnloadLevelRequest(0, LevelId);
    }

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    GameplayRpcGateway* Gateway = ResolveRpcGatewayNode();
    if (!Gateway)
    {
        if (const Result EnsureResult = EnsureRpcGatewayNode(); !EnsureResult)
        {
            return EnsureResult;
        }
        Gateway = ResolveRpcGatewayNode();
    }
    if (!Gateway)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Gameplay RPC gateway node was not found"));
    }

    if (!Gateway->CallRPC("ServerRequestUnloadLevel", {Variant::FromValue(ToString(LevelId))}))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to dispatch unload-level RPC request"));
    }
    return Ok();
#else
    (void)LevelId;
    return std::unexpected(MakeError(EErrorCode::NotReady, "Networking is disabled; no remote authority available"));
#endif
}

std::vector<GameFramework::NodeHandle> GameplayHost::LocalPlayers() const
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    std::vector<NodeHandle> Handles{};
    if (!m_runtime || !m_runtime->WorldPtr())
    {
        return Handles;
    }

    World().NodePool().ForEach([&Handles](const NodeHandle& Handle, BaseNode& Node) {
        if (dynamic_cast<LocalPlayer*>(&Node))
        {
            Handles.push_back(Handle);
        }
    });
    return Handles;
}

std::vector<GameFramework::NodeHandle> GameplayHost::LocalPlayersForConnection(const std::uint64_t OwnerConnectionId) const
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    std::vector<NodeHandle> Matches{};
    for (const NodeHandle PlayerHandle : LocalPlayers())
    {
        const auto* Player = dynamic_cast<const LocalPlayer*>(PlayerHandle.Borrowed());
        if (!Player)
        {
            continue;
        }
        if (Player->GetOwnerConnectionId() == OwnerConnectionId)
        {
            Matches.push_back(PlayerHandle);
        }
    }
    return Matches;
}

TExpected<GameFramework::NodeHandle> GameplayHost::LoadLevel(std::string Name)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "LoadLevel is server-authoritative"));
    }

    std::string EffectiveName = std::move(Name);
    if (EffectiveName.empty())
    {
        EffectiveName = "Level";
    }
    return World().CreateLevel(std::move(EffectiveName));
}

Result GameplayHost::UnloadLevel(const NodeHandle LevelHandle)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (LevelHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Level handle is null"));
    }
    if (IsClient() && !IsListenServer())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "UnloadLevel is server-authoritative"));
    }

    auto* Node = LevelHandle.Borrowed();
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Level node not found"));
    }
    if (!dynamic_cast<Level*>(Node))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node is not a Level"));
    }
    return World().DestroyNode(LevelHandle);
}

Result GameplayHost::UnloadLevel(const Uuid& LevelId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    for (const NodeHandle LevelHandle : World().Levels())
    {
        if (LevelHandle.Id == LevelId)
        {
            return UnloadLevel(LevelHandle);
        }
    }
    return std::unexpected(MakeError(EErrorCode::NotFound, "Level not found"));
}

Result GameplayHost::RegisterService(std::unique_ptr<IGameService> Service)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    if (!Service)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Service instance must not be null"));
    }

    const std::type_index ServiceType = std::type_index(typeid(*Service));
    if (m_serviceIndexByType.contains(ServiceType))
    {
        return Ok();
    }

    ServiceEntry Entry{};
    Entry.Type = ServiceType;
    Entry.Instance = std::move(Service);
    Entry.Initialized = false;

    const std::size_t NewIndex = m_services.size();
    m_services.emplace_back(std::move(Entry));
    m_serviceIndexByType.emplace(ServiceType, NewIndex);

    if (!IsInitialized())
    {
        return Ok();
    }

    if (const Result InitResult = InitializeServices(); !InitResult)
    {
        m_serviceIndexByType.erase(ServiceType);
        m_services.pop_back();
        (void)BuildServiceOrder();
        return InitResult;
    }

    return Ok();
}

Result GameplayHost::UnregisterService(const std::type_index& ServiceType)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    const auto ServiceIt = m_serviceIndexByType.find(ServiceType);
    if (ServiceIt == m_serviceIndexByType.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Gameplay service was not found"));
    }

    const std::size_t ServiceCount = m_services.size();
    std::vector<bool> RemoveMask(ServiceCount, false);
    std::vector<std::size_t> PendingIndices{ServiceIt->second};
    while (!PendingIndices.empty())
    {
        const std::size_t Current = PendingIndices.back();
        PendingIndices.pop_back();
        if (Current >= ServiceCount || RemoveMask[Current])
        {
            continue;
        }

        RemoveMask[Current] = true;
        const std::type_index CurrentType = m_services[Current].Type;
        for (std::size_t Candidate = 0; Candidate < ServiceCount; ++Candidate)
        {
            if (RemoveMask[Candidate] || !m_services[Candidate].Instance)
            {
                continue;
            }

            const auto Dependencies = m_services[Candidate].Instance->Dependencies();
            if (std::find(Dependencies.begin(), Dependencies.end(), CurrentType) == Dependencies.end())
            {
                continue;
            }
            PendingIndices.push_back(Candidate);
        }
    }

    if (IsInitialized())
    {
        for (auto ReverseIt = m_serviceOrder.rbegin(); ReverseIt != m_serviceOrder.rend(); ++ReverseIt)
        {
            const std::size_t ServiceIndex = *ReverseIt;
            if (ServiceIndex >= ServiceCount || !RemoveMask[ServiceIndex])
            {
                continue;
            }

            ServiceEntry& Entry = m_services[ServiceIndex];
            if (Entry.Initialized && Entry.Instance)
            {
                Entry.Instance->Shutdown(*this);
                Entry.Initialized = false;
            }
        }
    }

    std::vector<ServiceEntry> Remaining{};
    Remaining.reserve(ServiceCount);
    for (std::size_t Index = 0; Index < ServiceCount; ++Index)
    {
        if (!RemoveMask[Index])
        {
            Remaining.emplace_back(std::move(m_services[Index]));
        }
    }

    m_services = std::move(Remaining);
    RebuildServiceIndex();
    m_serviceOrder.clear();

    if (const Result OrderResult = BuildServiceOrder(); !OrderResult)
    {
        return OrderResult;
    }
    if (IsInitialized())
    {
        if (const Result InitResult = InitializeServices(); !InitResult)
        {
            return InitResult;
        }
    }

    return Ok();
}

void GameplayHost::RebuildServiceIndex()
{
    m_serviceIndexByType.clear();
    for (std::size_t Index = 0; Index < m_services.size(); ++Index)
    {
        m_serviceIndexByType[m_services[Index].Type] = Index;
    }
}

Result GameplayHost::BuildServiceOrder()
{
    m_serviceOrder.clear();
    if (m_services.empty())
    {
        return Ok();
    }

    const std::size_t ServiceCount = m_services.size();
    std::vector<bool> Resolved(ServiceCount, false);
    m_serviceOrder.reserve(ServiceCount);

    for (std::size_t ResolveCount = 0; ResolveCount < ServiceCount; ++ResolveCount)
    {
        std::size_t BestIndex = ServiceCount;
        int BestPriority = 0;

        for (std::size_t Index = 0; Index < ServiceCount; ++Index)
        {
            if (Resolved[Index] || !m_services[Index].Instance)
            {
                continue;
            }

            const auto Dependencies = m_services[Index].Instance->Dependencies();
            bool DependenciesReady = true;
            for (const std::type_index& DependencyType : Dependencies)
            {
                const auto DependencyIt = m_serviceIndexByType.find(DependencyType);
                if (DependencyIt == m_serviceIndexByType.end())
                {
                    const std::string Message = std::string("Gameplay service '") +
                                                std::string(m_services[Index].Instance->Name()) +
                                                "' is missing dependency '" + std::string(DependencyType.name()) + "'";
                    return std::unexpected(MakeError(EErrorCode::NotFound, Message));
                }
                if (!Resolved[DependencyIt->second])
                {
                    DependenciesReady = false;
                    break;
                }
            }

            if (!DependenciesReady)
            {
                continue;
            }

            const int CandidatePriority = m_services[Index].Instance->Priority();
            if (BestIndex == ServiceCount || CandidatePriority < BestPriority ||
                (CandidatePriority == BestPriority && Index < BestIndex))
            {
                BestIndex = Index;
                BestPriority = CandidatePriority;
            }
        }

        if (BestIndex == ServiceCount)
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError, "Circular or unsatisfied gameplay service dependencies"));
        }

        Resolved[BestIndex] = true;
        m_serviceOrder.push_back(BestIndex);
    }

    return Ok();
}

Result GameplayHost::InitializeServices()
{
    if (const Result OrderResult = BuildServiceOrder(); !OrderResult)
    {
        return OrderResult;
    }

    std::vector<std::size_t> InitializedOrder{};
    InitializedOrder.reserve(m_serviceOrder.size());

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || Entry.Initialized)
        {
            continue;
        }

        if (const Result InitResult = Entry.Instance->Initialize(*this); !InitResult)
        {
            for (auto ReverseIt = InitializedOrder.rbegin(); ReverseIt != InitializedOrder.rend(); ++ReverseIt)
            {
                ServiceEntry& InitializedEntry = m_services[*ReverseIt];
                if (InitializedEntry.Instance && InitializedEntry.Initialized)
                {
                    InitializedEntry.Instance->Shutdown(*this);
                    InitializedEntry.Initialized = false;
                }
            }
            return InitResult;
        }

        Entry.Initialized = true;
        InitializedOrder.push_back(ServiceIndex);

        if (m_initialized && m_runtime && m_runtime->WorldPtr())
        {
            const auto CurrentLevels = World().Levels();
            const auto CurrentPlayers = LocalPlayers();
            for (const NodeHandle LevelHandle : CurrentLevels)
            {
                Entry.Instance->OnLevelLoaded(*this, LevelHandle);
            }
            for (const NodeHandle PlayerHandle : CurrentPlayers)
            {
                Entry.Instance->OnLocalPlayerAdded(*this, PlayerHandle);
            }
        }
    }

    return Ok();
}

void GameplayHost::ShutdownServices()
{
    if (m_services.empty())
    {
        return;
    }

    if (m_serviceOrder.empty())
    {
        (void)BuildServiceOrder();
    }

    for (auto ReverseIt = m_serviceOrder.rbegin(); ReverseIt != m_serviceOrder.rend(); ++ReverseIt)
    {
        const std::size_t ServiceIndex = *ReverseIt;
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }

        ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        Entry.Instance->Shutdown(*this);
        Entry.Initialized = false;
    }
}

void GameplayHost::TickServices(const float DeltaSeconds)
{
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }
        Entry.Instance->Tick(*this, DeltaSeconds);
    }
}

void GameplayHost::RefreshObservedWorldState(const bool SeedOnly)
{
    RefreshObservedConnectionState(SeedOnly);

    std::unordered_map<Uuid, NodeHandle, UuidHash> CurrentLevels{};
    for (const NodeHandle Handle : World().Levels())
    {
        if (!Handle.IsNull())
        {
            CurrentLevels.emplace(Handle.Id, Handle);
        }
    }

    std::unordered_map<Uuid, NodeHandle, UuidHash> CurrentPlayers{};
    for (const NodeHandle Handle : LocalPlayers())
    {
        if (!Handle.IsNull())
        {
            CurrentPlayers.emplace(Handle.Id, Handle);
        }
    }

    if (!SeedOnly)
    {
        for (const auto& [Id, Handle] : CurrentLevels)
        {
            if (!m_knownLevelIds.contains(Id))
            {
                NotifyLevelLoaded(Handle);
            }
        }
        for (const auto& Id : m_knownLevelIds)
        {
            if (!CurrentLevels.contains(Id))
            {
                NotifyLevelUnloaded(Id);
            }
        }

        for (const auto& [Id, Handle] : CurrentPlayers)
        {
            if (!m_knownLocalPlayerIds.contains(Id))
            {
                NotifyLocalPlayerAdded(Handle);
            }
        }
        for (const auto& Id : m_knownLocalPlayerIds)
        {
            if (!CurrentPlayers.contains(Id))
            {
                NotifyLocalPlayerRemoved(Id);
            }
        }
    }

    m_knownLevelIds.clear();
    for (const auto& [Id, _] : CurrentLevels)
    {
        m_knownLevelIds.insert(Id);
    }

    m_knownLocalPlayerIds.clear();
    for (const auto& [Id, _] : CurrentPlayers)
    {
        m_knownLocalPlayerIds.insert(Id);
    }
}

void GameplayHost::RefreshObservedConnectionState(const bool SeedOnly)
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (!m_runtime || !m_runtime->WorldPtr())
    {
        return;
    }

    std::unordered_set<std::uint64_t> CurrentConnections{};
    if (World().Networking().Session() != nullptr)
    {
        const auto Connections = World().Networking().Connections();
        CurrentConnections.reserve(Connections.size());
        for (const auto Connection : Connections)
        {
            CurrentConnections.insert(static_cast<std::uint64_t>(Connection));
        }
    }

    if (!SeedOnly && IsServer())
    {
        for (const std::uint64_t ConnectionId : CurrentConnections)
        {
            if (!m_knownConnectionIds.contains(ConnectionId))
            {
                NotifyConnectionAdded(ConnectionId);
            }
        }
        for (const std::uint64_t KnownConnectionId : m_knownConnectionIds)
        {
            if (!CurrentConnections.contains(KnownConnectionId))
            {
                NotifyConnectionRemoved(KnownConnectionId);
            }
        }
    }

    m_knownConnectionIds = std::move(CurrentConnections);
#else
    (void)SeedOnly;
#endif
}

void GameplayHost::NotifyLevelLoaded(const NodeHandle LevelHandle)
{
    if (m_game)
    {
        m_game->OnLevelLoaded(*this, LevelHandle);
    }
    if (m_gameMode && IsServer())
    {
        m_gameMode->OnLevelLoaded(*this, LevelHandle);
    }
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (Entry.Instance && Entry.Initialized)
        {
            Entry.Instance->OnLevelLoaded(*this, LevelHandle);
        }
    }
}

void GameplayHost::NotifyLevelUnloaded(const Uuid& LevelId)
{
    if (m_game)
    {
        m_game->OnLevelUnloaded(*this, LevelId);
    }
    if (m_gameMode && IsServer())
    {
        m_gameMode->OnLevelUnloaded(*this, LevelId);
    }
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (Entry.Instance && Entry.Initialized)
        {
            Entry.Instance->OnLevelUnloaded(*this, LevelId);
        }
    }
}

void GameplayHost::NotifyLocalPlayerAdded(const NodeHandle PlayerHandle)
{
    if (m_game)
    {
        m_game->OnLocalPlayerAdded(*this, PlayerHandle);
    }
    if (m_gameMode && IsServer())
    {
        m_gameMode->OnLocalPlayerAdded(*this, PlayerHandle);
    }
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (Entry.Instance && Entry.Initialized)
        {
            Entry.Instance->OnLocalPlayerAdded(*this, PlayerHandle);
        }
    }
}

void GameplayHost::NotifyLocalPlayerRemoved(const Uuid& PlayerId)
{
    if (m_game)
    {
        m_game->OnLocalPlayerRemoved(*this, PlayerId);
    }
    if (m_gameMode && IsServer())
    {
        m_gameMode->OnLocalPlayerRemoved(*this, PlayerId);
    }
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (Entry.Instance && Entry.Initialized)
        {
            Entry.Instance->OnLocalPlayerRemoved(*this, PlayerId);
        }
    }
}

void GameplayHost::NotifyConnectionAdded(const std::uint64_t OwnerConnectionId)
{
    if (m_game)
    {
        m_game->OnConnectionAdded(*this, OwnerConnectionId);
    }
    if (m_gameMode && IsServer())
    {
        m_gameMode->OnConnectionAdded(*this, OwnerConnectionId);
    }
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (Entry.Instance && Entry.Initialized)
        {
            Entry.Instance->OnConnectionAdded(*this, OwnerConnectionId);
        }
    }

    if (!IsServer() || !m_settings.AutoCreateRemotePlayerOnConnection)
    {
        return;
    }

    if (!LocalPlayersForConnection(OwnerConnectionId).empty())
    {
        return;
    }

    const auto JoinResult = JoinPlayer(OwnerConnectionId, {}, 0U, true);
    (void)JoinResult;
}

void GameplayHost::NotifyConnectionRemoved(const std::uint64_t OwnerConnectionId)
{
    if (IsServer() && m_settings.AutoDestroyRemotePlayersOnDisconnect)
    {
        (void)LeavePlayersForConnection(OwnerConnectionId);
    }

    if (m_game)
    {
        m_game->OnConnectionRemoved(*this, OwnerConnectionId);
    }
    if (m_gameMode && IsServer())
    {
        m_gameMode->OnConnectionRemoved(*this, OwnerConnectionId);
    }
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (Entry.Instance && Entry.Initialized)
        {
            Entry.Instance->OnConnectionRemoved(*this, OwnerConnectionId);
        }
    }
}

Result GameplayHost::EnsureRpcGatewayNode()
{
    NodeHandle LookupHandle{GameplayRpcGateway::GatewayNodeId()};
    if (auto* Existing = dynamic_cast<GameplayRpcGateway*>(LookupHandle.BorrowedSlowByUuid());
        Existing != nullptr && Existing->World() == &World())
    {
        return Ok();
    }

    auto CreateResult =
        World().CreateNodeWithId<GameplayRpcGateway>(GameplayRpcGateway::GatewayNodeId(), "Gameplay.RpcGateway");
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    auto* Gateway = dynamic_cast<GameplayRpcGateway*>(CreateResult.value().Borrowed());
    if (!Gateway)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Created gameplay RPC gateway type mismatch"));
    }
    Gateway->Replicated(false);
    return Ok();
}

GameplayRpcGateway* GameplayHost::ResolveRpcGatewayNode() const
{
    NodeHandle LookupHandle{GameplayRpcGateway::GatewayNodeId()};
    auto* Gateway = dynamic_cast<GameplayRpcGateway*>(LookupHandle.BorrowedSlowByUuid());
    if (!Gateway)
    {
        return nullptr;
    }
    if (Gateway->World() != &World())
    {
        return nullptr;
    }
    return Gateway;
}

Result GameplayHost::EvaluateJoinRequestPolicy(const std::uint64_t OwnerConnectionId,
                                               const std::string& RequestedName,
                                               const std::optional<unsigned int> PreferredPlayerIndex,
                                               const bool ReplicatedPlayer)
{
    if (m_game && !m_game->AllowPlayerJoinRequest(*this, OwnerConnectionId, RequestedName, PreferredPlayerIndex, ReplicatedPlayer))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Join-player request denied by active game"));
    }
    if (m_gameMode && IsServer() &&
        !m_gameMode->AllowPlayerJoinRequest(*this, OwnerConnectionId, RequestedName, PreferredPlayerIndex, ReplicatedPlayer))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Join-player request denied by active game mode"));
    }

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        const ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        if (!Entry.Instance->AllowPlayerJoinRequest(*this, OwnerConnectionId, RequestedName, PreferredPlayerIndex, ReplicatedPlayer))
        {
            const std::string Message = std::string("Join-player request denied by gameplay service '") +
                                        std::string(Entry.Instance->Name()) + "'";
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Message));
        }
    }

    return Ok();
}

Result GameplayHost::EvaluateLeaveRequestPolicy(const std::uint64_t OwnerConnectionId,
                                                const std::optional<unsigned int> PlayerIndex)
{
    if (m_game && !m_game->AllowPlayerLeaveRequest(*this, OwnerConnectionId, PlayerIndex))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Leave-player request denied by active game"));
    }
    if (m_gameMode && IsServer() &&
        !m_gameMode->AllowPlayerLeaveRequest(*this, OwnerConnectionId, PlayerIndex))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Leave-player request denied by active game mode"));
    }

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        const ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        if (!Entry.Instance->AllowPlayerLeaveRequest(*this, OwnerConnectionId, PlayerIndex))
        {
            const std::string Message = std::string("Leave-player request denied by gameplay service '") +
                                        std::string(Entry.Instance->Name()) + "'";
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Message));
        }
    }

    return Ok();
}

Result GameplayHost::EvaluateLoadLevelRequestPolicy(const std::uint64_t OwnerConnectionId,
                                                    const std::string& RequestedName)
{
    if (m_game && !m_game->AllowLevelLoadRequest(*this, OwnerConnectionId, RequestedName))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Load-level request denied by active game"));
    }
    if (m_gameMode && IsServer() &&
        !m_gameMode->AllowLevelLoadRequest(*this, OwnerConnectionId, RequestedName))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Load-level request denied by active game mode"));
    }

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        const ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        if (!Entry.Instance->AllowLevelLoadRequest(*this, OwnerConnectionId, RequestedName))
        {
            const std::string Message = std::string("Load-level request denied by gameplay service '") +
                                        std::string(Entry.Instance->Name()) + "'";
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Message));
        }
    }

    return Ok();
}

Result GameplayHost::EvaluateUnloadLevelRequestPolicy(const std::uint64_t OwnerConnectionId, const Uuid& LevelId)
{
    if (m_game && !m_game->AllowLevelUnloadRequest(*this, OwnerConnectionId, LevelId))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unload-level request denied by active game"));
    }
    if (m_gameMode && IsServer() &&
        !m_gameMode->AllowLevelUnloadRequest(*this, OwnerConnectionId, LevelId))
    {
        return std::unexpected(
            MakeError(EErrorCode::InvalidArgument, "Unload-level request denied by active game mode"));
    }

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        if (ServiceIndex >= m_services.size())
        {
            continue;
        }
        const ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        if (!Entry.Instance->AllowLevelUnloadRequest(*this, OwnerConnectionId, LevelId))
        {
            const std::string Message = std::string("Unload-level request denied by gameplay service '") +
                                        std::string(Entry.Instance->Name()) + "'";
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Message));
        }
    }

    return Ok();
}

NodeHandle GameplayHost::FindAutoPossessTarget(const std::uint64_t OwnerConnectionId) const
{
    (void)OwnerConnectionId;

    std::unordered_set<Uuid, UuidHash> ClaimedTargets{};
    for (const NodeHandle PlayerHandle : LocalPlayers())
    {
        const auto* Player = dynamic_cast<const LocalPlayer*>(PlayerHandle.Borrowed());
        if (!Player)
        {
            continue;
        }

        const NodeHandle Possessed = Player->GetPossessedNode();
        if (!Possessed.IsNull())
        {
            ClaimedTargets.insert(Possessed.Id);
        }
    }

    NodeHandle BestTarget{};
    int BestScore = -1;
    World().NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (ClaimedTargets.contains(Handle.Id))
        {
            return;
        }

        const int Score = ScoreAutoPossessTarget(Node);
        if (Score <= 0)
        {
            return;
        }
        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Handle;
        }
    });

    return BestTarget;
}

void GameplayHost::EnsurePlayerHasPossession(LocalPlayer& Player)
{
    if (!Player.GetPossessedNode().IsNull())
    {
        return;
    }

    NodeHandle SelectedTarget{};
    if (m_gameMode && IsServer())
    {
        SelectedTarget = m_gameMode->SelectInitialPossessionTarget(*this, Player);
        if (!IsValidPossessionTargetForPlayer(Player, SelectedTarget))
        {
            SelectedTarget = {};
        }
    }

    if (SelectedTarget.IsNull() && m_game)
    {
        SelectedTarget = m_game->SelectInitialPossessionTarget(*this, Player);
        if (!IsValidPossessionTargetForPlayer(Player, SelectedTarget))
        {
            SelectedTarget = {};
        }
    }

    if (SelectedTarget.IsNull())
    {
        for (const std::size_t ServiceIndex : m_serviceOrder)
        {
            if (ServiceIndex >= m_services.size())
            {
                continue;
            }
            ServiceEntry& Entry = m_services[ServiceIndex];
            if (!Entry.Instance || !Entry.Initialized)
            {
                continue;
            }

            SelectedTarget = Entry.Instance->SelectInitialPossessionTarget(*this, Player);
            if (SelectedTarget.IsNull())
            {
                continue;
            }

            if (!IsValidPossessionTargetForPlayer(Player, SelectedTarget))
            {
                SelectedTarget = {};
                continue;
            }

            break;
        }
    }

    if (SelectedTarget.IsNull())
    {
        SelectedTarget = FindAutoPossessTarget(Player.GetOwnerConnectionId());
    }

    if (SelectedTarget.IsNull() || !IsValidPossessionTargetForPlayer(Player, SelectedTarget))
    {
        return;
    }

    Player.EditPossessedNode() = SelectedTarget;
}

Result GameplayHost::AutoCreateConfiguredLocalPlayer()
{
    std::string Name = m_settings.AutoCreateLocalPlayerName;
    if (Name.empty())
    {
        Name = "LocalPlayer";
    }

    auto CreateResult = CreateLocalPlayer(
        std::move(Name), m_settings.AutoCreateLocalPlayerIndex, m_settings.AutoCreateReplicatedLocalPlayer, 0);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    if (auto* Player = dynamic_cast<LocalPlayer*>(CreateResult.value().Borrowed()))
    {
        EnsurePlayerHasPossession(*Player);
    }
    return Ok();
}

std::optional<unsigned int> GameplayHost::FirstAvailablePlayerIndexForOwner(const std::uint64_t OwnerConnectionId) const
{
    std::unordered_set<unsigned int> UsedIndices{};
    for (const NodeHandle PlayerHandle : LocalPlayersForConnection(OwnerConnectionId))
    {
        const auto* Player = dynamic_cast<const LocalPlayer*>(PlayerHandle.Borrowed());
        if (Player)
        {
            UsedIndices.insert(Player->GetPlayerIndex());
        }
    }

    for (unsigned int CandidateIndex = 0; CandidateIndex < std::numeric_limits<unsigned int>::max(); ++CandidateIndex)
    {
        if (!UsedIndices.contains(CandidateIndex))
        {
            return CandidateIndex;
        }
    }

    return std::nullopt;
}

NodeHandle GameplayHost::FindLocalPlayerByOwnerAndIndex(const std::uint64_t OwnerConnectionId,
                                                        const unsigned int PlayerIndex) const
{
    for (const NodeHandle PlayerHandle : LocalPlayersForConnection(OwnerConnectionId))
    {
        const auto* Player = dynamic_cast<const LocalPlayer*>(PlayerHandle.Borrowed());
        if (!Player)
        {
            continue;
        }
        if (Player->GetPlayerIndex() == PlayerIndex)
        {
            return PlayerHandle;
        }
    }
    return {};
}

} // namespace SnAPI::GameFramework
