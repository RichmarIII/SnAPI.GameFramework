#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Export.h"
#include "IGame.h"
#include "IGameMode.h"
#include "IGameService.h"

namespace SnAPI::GameFramework
{

class GameRuntime;
class GameplayRpcGateway;
class LocalPlayer;
class World;

/**
 * @brief Gameplay bootstrap settings consumed by `GameplayHost`.
 * @remarks
 * Uses factory-function pointers instead of `std::function` to keep runtime
 * behavior deterministic and allocation-free for default factory calls.
 */
struct GameRuntimeGameplaySettings
{
    using GameFactory = std::unique_ptr<IGame>(*)();
    using GameModeFactory = std::unique_ptr<IGameMode>(*)();

    GameFactory CreateGame = nullptr; /**< @brief Optional session-game factory. */
    GameModeFactory CreateServerGameMode = nullptr; /**< @brief Optional server-mode factory override. */
    bool AutoCreateLocalPlayer = true; /**< @brief Create one local-player node during initialize. */
    bool AutoCreateReplicatedLocalPlayer = true; /**< @brief Replication gate for auto-created local player. */
    unsigned int AutoCreateLocalPlayerIndex = 0; /**< @brief Player index used for auto-created local player. */
    std::string AutoCreateLocalPlayerName = "LocalPlayer"; /**< @brief Name used for auto-created local player node. */
    bool RegisterDefaultLocalPlayerService = true; /**< @brief Register default local-player input-mapping service. */
    bool AutoCreateRemotePlayerOnConnection = true; /**< @brief Server: auto-create one replicated local-player per remote connection. */
    bool AutoDestroyRemotePlayersOnDisconnect = true; /**< @brief Server: remove local-players owned by disconnected remote connections. */

    template<typename TGame>
    void SetGameFactory()
    {
        static_assert(std::is_base_of_v<IGame, TGame>, "TGame must derive from IGame");
        CreateGame = []() -> std::unique_ptr<IGame> {
            return std::make_unique<TGame>();
        };
    }

    template<typename TGameMode>
    void SetServerGameModeFactory()
    {
        static_assert(std::is_base_of_v<IGameMode, TGameMode>, "TGameMode must derive from IGameMode");
        CreateServerGameMode = []() -> std::unique_ptr<IGameMode> {
            return std::make_unique<TGameMode>();
        };
    }
};

/**
 * @brief Multiplayer-first gameplay orchestration host owned by `GameRuntime`.
 * @remarks
 * Responsibilities:
 * - service lifecycle (dependency ordered)
 * - session game lifecycle (`IGame`)
 * - server-only game mode lifecycle (`IGameMode`)
 * - world observation callbacks for levels/local-players
 */
class SNAPI_GAMEFRAMEWORK_API GameplayHost final
{
public:
    /**
     * @brief Initialize gameplay host for an initialized runtime.
     */
    Result Initialize(GameRuntime& RuntimeRef, const GameRuntimeGameplaySettings& SettingsValue);

    /**
     * @brief Shutdown gameplay host.
     */
    void Shutdown();

    /**
     * @brief True when gameplay host is initialized.
     */
    [[nodiscard]] bool IsInitialized() const;

    /**
     * @brief Per-frame gameplay update.
     */
    void Tick(float DeltaSeconds);

    /**
     * @brief Runtime accessor.
     */
    [[nodiscard]] GameRuntime& Runtime();
    /**
     * @brief Runtime accessor (const).
     */
    [[nodiscard]] const GameRuntime& Runtime() const;

    /**
     * @brief World accessor.
     */
    [[nodiscard]] GameFramework::World& World();
    /**
     * @brief World accessor (const).
     */
    [[nodiscard]] const GameFramework::World& World() const;

    /**
     * @brief Networking role helper.
     */
    [[nodiscard]] bool IsServer() const;
    /**
     * @brief Networking role helper.
     */
    [[nodiscard]] bool IsClient() const;
    /**
     * @brief Networking role helper.
     */
    [[nodiscard]] bool IsListenServer() const;

    /**
     * @brief Active game accessor.
     */
    [[nodiscard]] IGame* Game() const;

    /**
     * @brief Active server game mode accessor.
     * @remarks Returns null on clients by design.
     */
    [[nodiscard]] IGameMode* GameMode() const;

    /**
     * @brief Replace session game instance.
     */
    Result SetGame(std::unique_ptr<IGame> GameInstance);

    /**
     * @brief Replace server game mode instance.
     * @remarks Fails when called from client authority.
     */
    Result SetServerGameMode(std::unique_ptr<IGameMode> GameModeInstance);

    /**
     * @brief Shutdown and clear server game mode.
     */
    Result ClearServerGameMode();

    /**
     * @brief Create a world-root local-player node.
     */
    TExpected<GameFramework::NodeHandle> CreateLocalPlayer(std::string Name,
                                            unsigned int PlayerIndex,
                                            bool ReplicatedPlayer = true,
                                            std::uint64_t OwnerConnectionId = 0);

    /**
     * @brief Authoritatively join one player for an owner connection.
     * @remarks Server-authoritative in multiplayer.
     */
    TExpected<GameFramework::NodeHandle> JoinPlayer(std::uint64_t OwnerConnectionId,
                                                    std::string Name = {},
                                                    std::optional<unsigned int> PreferredPlayerIndex = std::nullopt,
                                                    bool ReplicatedPlayer = true);

    /**
     * @brief Authoritatively remove one local-player node.
     */
    Result LeavePlayer(const NodeHandle& PlayerHandle);

    /**
     * @brief Authoritatively remove one local-player by id.
     */
    Result LeavePlayer(const Uuid& PlayerId);

    /**
     * @brief Authoritatively remove all local-players owned by one connection.
     */
    Result LeavePlayersForConnection(std::uint64_t OwnerConnectionId);

    /**
     * @brief Server-authoritative connection request entrypoint for joining a local-player.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleJoinPlayerRequest(std::uint64_t OwnerConnectionId,
                                   std::string Name = {},
                                   std::optional<unsigned int> PreferredPlayerIndex = std::nullopt,
                                   bool ReplicatedPlayer = true);

    /**
     * @brief Server-authoritative connection request entrypoint for leaving a local-player.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleLeavePlayerRequest(std::uint64_t OwnerConnectionId,
                                    std::optional<unsigned int> PlayerIndex = std::nullopt);

    /**
     * @brief Server-authoritative connection request entrypoint for level load.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleLoadLevelRequest(std::uint64_t OwnerConnectionId, std::string Name);

    /**
     * @brief Server-authoritative connection request entrypoint for level unload.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleUnloadLevelRequest(std::uint64_t OwnerConnectionId, const Uuid& LevelId);

    /**
     * @brief Request player join on the active authority.
     * @remarks
     * Server/listen-server executes immediately.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestJoinPlayer(std::string Name = {},
                             std::optional<unsigned int> PreferredPlayerIndex = std::nullopt,
                             bool ReplicatedPlayer = true);

    /**
     * @brief Request player leave on the active authority.
     * @remarks
     * Server/listen-server executes immediately for owner `0`.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestLeavePlayer(std::optional<unsigned int> PlayerIndex = std::nullopt);

    /**
     * @brief Request level load on the active authority.
     * @remarks
     * Server/listen-server executes immediately.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestLoadLevel(std::string Name);

    /**
     * @brief Request level unload on the active authority.
     * @remarks
     * Server/listen-server executes immediately.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestUnloadLevel(const Uuid& LevelId);

    /**
     * @brief Query currently active local-player nodes.
     */
    [[nodiscard]] std::vector<GameFramework::NodeHandle> LocalPlayers() const;

    /**
     * @brief Query local-player nodes owned by one connection.
     */
    [[nodiscard]] std::vector<GameFramework::NodeHandle> LocalPlayersForConnection(
        std::uint64_t OwnerConnectionId) const;

    /**
     * @brief Create and attach one level node.
     * @remarks Server-authoritative in multiplayer.
     */
    TExpected<GameFramework::NodeHandle> LoadLevel(std::string Name);

    /**
     * @brief Schedule one level for end-of-frame unload.
     * @remarks Server-authoritative in multiplayer.
     */
    Result UnloadLevel(const NodeHandle& LevelHandle);

    /**
     * @brief Schedule one level for end-of-frame unload by UUID.
     * @remarks Server-authoritative in multiplayer.
     */
    Result UnloadLevel(const Uuid& LevelId);

    /**
     * @brief Register a gameplay service instance.
     */
    Result RegisterService(std::unique_ptr<IGameService> Service);

    /**
     * @brief Unregister a gameplay service type.
     * @remarks Also unregisters transitive dependents.
     */
    Result UnregisterService(const std::type_index& ServiceType);

    /**
     * @brief Register a concrete gameplay service type.
     * @remarks Registration is idempotent by service type.
     */
    template<typename TService, typename... TArgs>
    TService& RegisterService(TArgs&&... Args);

    /**
     * @brief Query a gameplay service by type.
     */
    template<typename TService>
    [[nodiscard]] TService* GetService();

    /**
     * @brief Query a gameplay service by type (const).
     */
    template<typename TService>
    [[nodiscard]] const TService* GetService() const;

private:
    struct ServiceEntry
    {
        std::type_index Type = std::type_index(typeid(void));
        std::unique_ptr<IGameService> Instance{};
        bool Initialized = false;
    };

    void RebuildServiceIndex();
    Result BuildServiceOrder();
    Result InitializeServices();
    void ShutdownServices();
    void TickServices(float DeltaSeconds);

    void RefreshObservedWorldState(bool SeedOnly);
    void RefreshObservedConnectionState(bool SeedOnly);
    void NotifyLevelLoaded(const NodeHandle& LevelHandle);
    void NotifyLevelUnloaded(const Uuid& LevelId);
    void NotifyLocalPlayerAdded(const NodeHandle& PlayerHandle);
    void NotifyLocalPlayerRemoved(const Uuid& PlayerId);
    void NotifyConnectionAdded(std::uint64_t OwnerConnectionId);
    void NotifyConnectionRemoved(std::uint64_t OwnerConnectionId);
    Result AutoCreateConfiguredLocalPlayer();
    Result EnsureRpcGatewayNode();
    GameplayRpcGateway* ResolveRpcGatewayNode() const;
    Result EvaluateJoinRequestPolicy(std::uint64_t OwnerConnectionId,
                                     const std::string& RequestedName,
                                     std::optional<unsigned int> PreferredPlayerIndex,
                                     bool ReplicatedPlayer);
    Result EvaluateLeaveRequestPolicy(std::uint64_t OwnerConnectionId,
                                      std::optional<unsigned int> PlayerIndex);
    Result EvaluateLoadLevelRequestPolicy(std::uint64_t OwnerConnectionId, const std::string& RequestedName);
    Result EvaluateUnloadLevelRequestPolicy(std::uint64_t OwnerConnectionId, const Uuid& LevelId);
    void SyncLocalPlayerPossessionCallbacks();
    NodeHandle ResolvePlayerStart(LocalPlayer& Player);
    NodeHandle SpawnPlayerPawn(LocalPlayer& Player, const NodeHandle& PlayerStart);
    NodeHandle FindAutoPossessTarget(std::uint64_t OwnerConnectionId) const;
    void EnsurePlayerHasPossession(LocalPlayer& Player);
    std::optional<unsigned int> FirstAvailablePlayerIndexForOwner(std::uint64_t OwnerConnectionId) const;
    NodeHandle FindLocalPlayerByOwnerAndIndex(std::uint64_t OwnerConnectionId, unsigned int PlayerIndex) const;

    GameRuntime* m_runtime = nullptr;
    GameRuntimeGameplaySettings m_settings{};

    std::unique_ptr<IGame> m_game{};
    std::unique_ptr<IGameMode> m_gameMode{};

    std::vector<ServiceEntry> m_services{};
    std::unordered_map<std::type_index, std::size_t> m_serviceIndexByType{};
    std::vector<std::size_t> m_serviceOrder{};

    std::unordered_set<Uuid, UuidHash> m_knownLevelIds{};
    std::unordered_set<Uuid, UuidHash> m_knownLocalPlayerIds{};
    std::unordered_set<std::uint64_t> m_knownConnectionIds{};

    bool m_initialized = false;
};

template<typename TService, typename... TArgs>
TService& GameplayHost::RegisterService(TArgs&&... Args)
{
    static_assert(std::is_base_of_v<IGameService, TService>, "TService must derive from IGameService");

    const std::type_index ServiceType = std::type_index(typeid(TService));
    if (const auto Existing = m_serviceIndexByType.find(ServiceType); Existing != m_serviceIndexByType.end())
    {
        return static_cast<TService&>(*m_services[Existing->second].Instance);
    }

    ServiceEntry Entry{};
    Entry.Type = ServiceType;
    Entry.Instance = std::make_unique<TService>(std::forward<TArgs>(Args)...);
    Entry.Initialized = false;

    const std::size_t NewIndex = m_services.size();
    m_services.emplace_back(std::move(Entry));
    m_serviceIndexByType.emplace(ServiceType, NewIndex);
    return static_cast<TService&>(*m_services.back().Instance);
}

template<typename TService>
TService* GameplayHost::GetService()
{
    static_assert(std::is_base_of_v<IGameService, TService>, "TService must derive from IGameService");
    const auto It = m_serviceIndexByType.find(std::type_index(typeid(TService)));
    if (It == m_serviceIndexByType.end())
    {
        return nullptr;
    }
    return static_cast<TService*>(m_services[It->second].Instance.get());
}

template<typename TService>
const TService* GameplayHost::GetService() const
{
    static_assert(std::is_base_of_v<IGameService, TService>, "TService must derive from IGameService");
    const auto It = m_serviceIndexByType.find(std::type_index(typeid(TService)));
    if (It == m_serviceIndexByType.end())
    {
        return nullptr;
    }
    return static_cast<const TService*>(m_services.at(It->second).Instance.get());
}

} // namespace SnAPI::GameFramework
