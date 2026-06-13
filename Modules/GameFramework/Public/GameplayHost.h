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
 * @ingroup SnAPI_GameFramework
 * @brief Gameplay bootstrap policy consumed by `GameplayHost`.
 *
 * `GameRuntimeGameplaySettings` describes how the gameplay host should construct and manage
 * its high-level session objects and convenience behaviors. It intentionally uses raw factory
 * function pointers instead of `std::function` so the default path remains deterministic,
 * allocation-free, and trivial to store in runtime settings.
 *
 * Semantics:
 * - null factories mean "do not auto-create that object"
 * - auto-create flags only affect host bootstrap behavior; they do not prevent later manual calls
 * - connection/player automation applies only where the host has authority to act
 *
 * @see GameplayHost
 * @see GameRuntimeSettings
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
 * @ingroup SnAPI_GameFramework
 * @brief High-level gameplay/session orchestrator owned by `GameRuntime`.
 *
 * `GameplayHost` is the bridge between the low-level world graph and higher-level gameplay
 * concepts such as the active game instance, server game mode, modular gameplay services,
 * player join/leave flow, level load/unload flow, and possession/spawn defaults. It is the
 * object that decides when gameplay abstractions are created, ticked, notified, and torn down.
 *
 * Core responsibilities:
 * - own and lifecycle-manage `IGame`, `IGameMode`, and `IGameService` instances
 * - expose authority-aware APIs for player and level mutation
 * - observe world state and relay lifecycle callbacks for levels, players, and connections
 * - resolve player starts, pawn spawning, and initial possession targets
 * - route client-authored requests to authority through `GameplayRpcGateway` when needed
 *
 * Authority model:
 * - direct mutation APIs such as `JoinPlayer()`, `LeavePlayer()`, `LoadLevel()`, and `UnloadLevel()`
 *   are server-authoritative
 * - request APIs such as `RequestJoinPlayer()` route to the server when called from a pure client
 * - standalone and listen-server runtimes execute authority paths locally
 *
 * Ownership and lifetime:
 * - Owned by `GameRuntime`.
 * - Non-owning references to `GameRuntime` and `World` remain valid only while the runtime is initialized.
 * - Registered gameplay services, game, and game mode are owned by the host.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameRuntime
 * @see IGame
 * @see IGameMode
 * @see IGameService
 */
class SNAPI_GAMEFRAMEWORK_API GameplayHost final
{
public:
    /**
     * @brief Initialize gameplay orchestration for an already initialized runtime.
     * @param RuntimeRef Borrowed runtime that owns the world.
     * @param SettingsValue Gameplay bootstrap policy.
     * @return Success or an initialization error.
     * @pre `RuntimeRef` must already be initialized and bound to a world.
     * @post On success, the host owns any created game, game mode, and service instances until shutdown.
     */
    Result Initialize(GameRuntime& RuntimeRef, const GameRuntimeGameplaySettings& SettingsValue);

    /**
     * @brief Shutdown gameplay orchestration and release owned gameplay objects.
     * @remarks Services are shut down before the owned game/mode instances are discarded.
     */
    void Shutdown();

    /**
     * @brief Check whether the host is currently initialized.
     * @return `true` when the host has successfully completed initialization.
     */
    [[nodiscard]] bool IsInitialized() const;

    /**
     * @brief Run one gameplay-host frame update.
     * @param DeltaSeconds Frame delta time in seconds.
     * @remarks
     * Ticks registered services first, then the owned game and game mode, and finally
     * reconciles observed world state such as levels, players, and connections.
     */
    void Tick(float DeltaSeconds);

    /**
     * @brief Access the owning runtime.
     * @return Borrowed runtime reference.
     * @pre The host must be initialized.
     */
    [[nodiscard]] GameRuntime& Runtime();
    /**
     * @brief Access the owning runtime.
     * @return Borrowed runtime reference.
     * @pre The host must be initialized.
     */
    [[nodiscard]] const GameRuntime& Runtime() const;

    /**
     * @brief Access the world owned by the runtime.
     * @return Borrowed world reference.
     */
    [[nodiscard]] GameFramework::World& World();
    /**
     * @brief Access the world owned by the runtime.
     * @return Borrowed world reference.
     */
    [[nodiscard]] const GameFramework::World& World() const;

    /** @brief Query whether the host currently has server authority. @return `true` for standalone, dedicated, or listen-server authority. */
    [[nodiscard]] bool IsServer() const;
    /** @brief Query whether the host is executing in any client role. @return `true` when the underlying world networking role is client-capable. */
    [[nodiscard]] bool IsClient() const;
    /** @brief Query whether the host is executing as a listen server. @return `true` when server and client roles are both present. */
    [[nodiscard]] bool IsListenServer() const;

    /**
     * @brief Access the active session-wide game instance.
     * @return Non-owning pointer or `nullptr` when no game is active.
     */
    [[nodiscard]] IGame* Game() const;

    /**
     * @brief Active server game mode accessor.
     * @return Non-owning pointer or `nullptr` when no server game mode is active.
     * @remarks Returns null on clients by design.
     */
    [[nodiscard]] IGameMode* GameMode() const;

    /**
     * @brief Replace the active session-wide game instance.
     * @param GameInstance Owned game instance to adopt, or `nullptr` to clear it.
     * @return Success or an initialization error.
     * @remarks
     * The previous game, if any, is shut down before the replacement is initialized.
     * On server authority, setting a game may also create an initial game mode if none exists.
     */
    Result SetGame(std::unique_ptr<IGame> GameInstance);

    /**
     * @brief Replace the active server game mode instance.
     * @param GameModeInstance Owned mode instance to adopt, or `nullptr` to clear it.
     * @return Success or an initialization error.
     * @remarks Fails when called from client authority.
     */
    Result SetServerGameMode(std::unique_ptr<IGameMode> GameModeInstance);

    /**
     * @brief Shutdown and clear the active server game mode.
     * @return Success or an authority error.
     */
    Result ClearServerGameMode();

    /**
     * @brief Create a `LocalPlayer` node without applying join-policy or authority checks.
     * @param Name Preferred node name. Empty names fall back to `"LocalPlayer"`.
     * @param PlayerIndex Player slot index for the owning connection.
     * @param ReplicatedPlayer Replication state for the created node.
     * @param OwnerConnectionId Owning connection id. `0` represents local authority.
     * @return Handle to the created player node or an error.
     * @remarks
     * This is a low-level creation helper used by higher-level join flows. It does not
     * automatically validate request policy or guarantee possession setup on its own.
     */
    TExpected<GameFramework::NodeHandle> CreateLocalPlayer(std::string Name,
                                            unsigned int PlayerIndex,
                                            bool ReplicatedPlayer = true,
                                            std::uint64_t OwnerConnectionId = 0);

    /**
     * @brief Authoritatively ensure one player exists for an owning connection/index combination.
     * @param OwnerConnectionId Owning connection id. `0` represents local authority.
     * @param Name Preferred player node name. Empty names use host-generated defaults.
     * @param PreferredPlayerIndex Requested player index, or `std::nullopt` for first available.
     * @param ReplicatedPlayer Replication state for the player node.
     * @return Handle to the existing or newly created player node, or an error.
     * @remarks
     * Server-authoritative in multiplayer. Existing players with the same owner/index pair are reused.
     */
    TExpected<GameFramework::NodeHandle> JoinPlayer(std::uint64_t OwnerConnectionId,
                                                    std::string Name = {},
                                                    std::optional<unsigned int> PreferredPlayerIndex = std::nullopt,
                                                    bool ReplicatedPlayer = true);

    /**
     * @brief Authoritatively remove one local-player node.
     * @param PlayerHandle Target player handle.
     * @return Success or an error.
     */
    Result LeavePlayer(const NodeHandle& PlayerHandle);

    /**
     * @brief Authoritatively remove one local-player by id.
     * @param PlayerId Stable id of the player to remove.
     * @return Success or an error.
     */
    Result LeavePlayer(const Uuid& PlayerId);

    /**
     * @brief Authoritatively remove all local-players owned by one connection.
     * @param OwnerConnectionId Owning connection id.
     * @return Success or an error.
     */
    Result LeavePlayersForConnection(std::uint64_t OwnerConnectionId);

    /**
     * @brief Server-authoritative connection request entrypoint for joining a local-player.
     * @param OwnerConnectionId Requesting connection id.
     * @param Name Requested player name, possibly empty.
     * @param PreferredPlayerIndex Requested player index, if any.
     * @param ReplicatedPlayer Requested replication state for the created player node.
     * @return Success or an error.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleJoinPlayerRequest(std::uint64_t OwnerConnectionId,
                                   std::string Name = {},
                                   std::optional<unsigned int> PreferredPlayerIndex = std::nullopt,
                                   bool ReplicatedPlayer = true);

    /**
     * @brief Server-authoritative connection request entrypoint for leaving a local-player.
     * @param OwnerConnectionId Requesting connection id.
     * @param PlayerIndex Requested player index, or `std::nullopt` for all caller-owned players.
     * @return Success or an error.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleLeavePlayerRequest(std::uint64_t OwnerConnectionId,
                                    std::optional<unsigned int> PlayerIndex = std::nullopt);

    /**
     * @brief Server-authoritative connection request entrypoint for level load.
     * @param OwnerConnectionId Requesting connection id.
     * @param Name Requested level name, possibly empty.
     * @return Success or an error.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleLoadLevelRequest(std::uint64_t OwnerConnectionId, std::string Name);

    /**
     * @brief Server-authoritative connection request entrypoint for level unload.
     * @param OwnerConnectionId Requesting connection id.
     * @param LevelId Stable id of the level to unload.
     * @return Success or an error.
     * @remarks Applies `IGame`, `IGameMode`, and `IGameService` request-policy hooks.
     */
    Result HandleUnloadLevelRequest(std::uint64_t OwnerConnectionId, const Uuid& LevelId);

    /**
     * @brief Request player join on the active authority.
     * @param Name Requested player name, possibly empty.
     * @param PreferredPlayerIndex Requested player index, if any.
     * @param ReplicatedPlayer Requested replication state for the created player node.
     * @return Success or an error.
     * @remarks
     * Server/listen-server executes immediately.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestJoinPlayer(std::string Name = {},
                             std::optional<unsigned int> PreferredPlayerIndex = std::nullopt,
                             bool ReplicatedPlayer = true);

    /**
     * @brief Request player leave on the active authority.
     * @param PlayerIndex Requested player index, or `std::nullopt` for all caller-owned players.
     * @return Success or an error.
     * @remarks
     * Server/listen-server executes immediately for owner `0`.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestLeavePlayer(std::optional<unsigned int> PlayerIndex = std::nullopt);

    /**
     * @brief Request level load on the active authority.
     * @param Name Requested level name, possibly empty.
     * @return Success or an error.
     * @remarks
     * Server/listen-server executes immediately.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestLoadLevel(std::string Name);

    /**
     * @brief Request level unload on the active authority.
     * @param LevelId Stable id of the level to unload.
     * @return Success or an error.
     * @remarks
     * Server/listen-server executes immediately.
     * Clients route through the gameplay RPC gateway node.
     */
    Result RequestUnloadLevel(const Uuid& LevelId);

    /**
     * @brief Query currently active local-player nodes.
     * @return Snapshot of player handles known to the world at the time of the call.
     */
    [[nodiscard]] std::vector<GameFramework::NodeHandle> LocalPlayers() const;

    /**
     * @brief Query local-player nodes owned by one connection.
     * @param OwnerConnectionId Owning connection id.
     * @return Snapshot of matching player handles.
     */
    [[nodiscard]] std::vector<GameFramework::NodeHandle> LocalPlayersForConnection(
        std::uint64_t OwnerConnectionId) const;

    /**
     * @brief Create and attach one level node.
     * @param Name Preferred level node name. Empty names fall back to `"Level"`.
     * @return Handle to the created level or an error.
     * @remarks Server-authoritative in multiplayer.
     */
    TExpected<GameFramework::NodeHandle> LoadLevel(std::string Name);

    /**
     * @brief Schedule one level for end-of-frame unload.
     * @param LevelHandle Handle of the level to destroy.
     * @return Success or an error.
     * @remarks Server-authoritative in multiplayer.
     */
    Result UnloadLevel(const NodeHandle& LevelHandle);

    /**
     * @brief Schedule one level for end-of-frame unload by UUID.
     * @param LevelId Stable id of the level to destroy.
     * @return Success or an error.
     * @remarks Server-authoritative in multiplayer.
     */
    Result UnloadLevel(const Uuid& LevelId);

    /**
     * @brief Register a gameplay service instance.
     * @param Service Owned service instance to adopt.
     * @return Success or an error.
     * @remarks
     * Registration is idempotent by concrete service type. If the host is already initialized,
     * the new service is initialized immediately after dependency-order rebuild.
     */
    Result RegisterService(std::unique_ptr<IGameService> Service);

    /**
     * @brief Unregister a gameplay service type.
     * @param ServiceType Concrete service type to remove.
     * @return Success or an error.
     * @remarks Also unregisters transitive dependents.
     */
    Result UnregisterService(const std::type_index& ServiceType);

    /**
     * @brief Register a concrete gameplay service type.
     * @tparam TService Concrete service type deriving from `IGameService`.
     * @tparam TArgs Constructor argument types forwarded into the service constructor.
     * @param Args Constructor arguments for the new service.
     * @return Borrowed reference to the registered service instance.
     * @remarks Registration is idempotent by service type.
     */
    template<typename TService, typename... TArgs>
    TService& RegisterService(TArgs&&... Args);

    /**
     * @brief Query a gameplay service by type.
     * @tparam TService Concrete service type deriving from `IGameService`.
     * @return Non-owning pointer to the service instance or `nullptr`.
     */
    template<typename TService>
    [[nodiscard]] TService* GetService();

    /**
     * @brief Query a gameplay service by type (const).
     * @tparam TService Concrete service type deriving from `IGameService`.
     * @return Non-owning pointer to the service instance or `nullptr`.
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
    GameplayRpcGateway* ResolveRpcGatewayNode();
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
    NodeHandle SpawnPlayerPawn(LocalPlayer& Player, NodeHandle PlayerStart);
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
    NodeHandle m_rpcGatewayNode{};

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
