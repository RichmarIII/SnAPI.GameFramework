# SnAPI::GameFramework::GameplayHost

High-level gameplay/session orchestrator owned by `GameRuntime`.

`GameplayHost` is the bridge between the low-level world graph and higher-level gameplay concepts such as the active game instance, server game mode, modular gameplay services, player join/leave flow, level load/unload flow, and possession/spawn defaults. It is the object that decides when gameplay abstractions are created, ticked, notified, and torn down.

Core responsibilities:
- own and lifecycle-manage `IGame`, `IGameMode`, and `IGameService` instances
- expose authority-aware APIs for player and level mutation
- observe world state and relay lifecycle callbacks for levels, players, and connections
- resolve player starts, pawn spawning, and initial possession targets
- route client-authored requests to authority through `GameplayRpcGateway` when needed

Authority model:
- direct mutation APIs such as `JoinPlayer()`, `LeavePlayer()`, `LoadLevel()`, and `UnloadLevel()` are server-authoritative
- request APIs such as `RequestJoinPlayer()` route to the server when called from a pure client
- standalone and listen-server runtimes execute authority paths locally

Ownership and lifetime:
- Owned by `GameRuntime`.
- Non-owning references to `GameRuntime` and `World` remain valid only while the runtime is initialized.
- Registered gameplay services, game, and game mode are owned by the host.

Threading model:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::GameplayHost::ServiceEntry

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameRuntime* SnAPI::GameFramework::GameplayHost::m_runtime`
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntimeGameplaySettings SnAPI::GameFramework::GameplayHost::m_settings`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<IGame> SnAPI::GameFramework::GameplayHost::m_game`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<IGameMode> SnAPI::GameFramework::GameplayHost::m_gameMode`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ServiceEntry> SnAPI::GameFramework::GameplayHost::m_services`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::type_index, std::size_t> SnAPI::GameFramework::GameplayHost::m_serviceIndexByType`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::size_t> SnAPI::GameFramework::GameplayHost::m_serviceOrder`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_set<Uuid, UuidHash> SnAPI::GameFramework::GameplayHost::m_knownLevelIds`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_set<Uuid, UuidHash> SnAPI::GameFramework::GameplayHost::m_knownLocalPlayerIds`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_set<std::uint64_t> SnAPI::GameFramework::GameplayHost::m_knownConnectionIds`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameplayHost::m_initialized`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::Initialize(GameRuntime &RuntimeRef, const GameRuntimeGameplaySettings &SettingsValue)`

Initialize gameplay orchestration for an already initialized runtime.

**Parameters**

- `RuntimeRef`: Borrowed runtime that owns the world.
- `SettingsValue`: Gameplay bootstrap policy.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::Shutdown()`

Shutdown gameplay orchestration and release owned gameplay objects.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameplayHost::IsInitialized() const`

Check whether the host is currently initialized.

**Returns:** `true` when the host has successfully completed initialization.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::Tick(float DeltaSeconds)`

Run one gameplay-host frame update.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntime & SnAPI::GameFramework::GameplayHost::Runtime()`

Access the owning runtime.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameRuntime & SnAPI::GameFramework::GameplayHost::Runtime() const`

Access the owning runtime.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `World & SnAPI::GameFramework::GameplayHost::World()`

Access the world owned by the runtime.

**Returns:** Borrowed world reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const World & SnAPI::GameFramework::GameplayHost::World() const`

Access the world owned by the runtime.

**Returns:** Borrowed world reference.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameplayHost::IsServer() const`

Query whether the host currently has server authority.

**Returns:** `true` for standalone, dedicated, or listen-server authority.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameplayHost::IsClient() const`

Query whether the host is executing in any client role.

**Returns:** `true` when the underlying world networking role is client-capable.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameplayHost::IsListenServer() const`

Query whether the host is executing as a listen server.

**Returns:** `true` when server and client roles are both present.
</div>
<div class="snapi-api-card" markdown="1">
### `IGame * SnAPI::GameFramework::GameplayHost::Game() const`

Access the active session-wide game instance.

**Returns:** Non-owning pointer or `nullptr` when no game is active.
</div>
<div class="snapi-api-card" markdown="1">
### `IGameMode * SnAPI::GameFramework::GameplayHost::GameMode() const`

Active server game mode accessor.

**Returns:** Non-owning pointer or `nullptr` when no server game mode is active.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::SetGame(std::unique_ptr< IGame > GameInstance)`

Replace the active session-wide game instance.

**Parameters**

- `GameInstance`: Owned game instance to adopt, or `nullptr` to clear it.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::SetServerGameMode(std::unique_ptr< IGameMode > GameModeInstance)`

Replace the active server game mode instance.

**Parameters**

- `GameModeInstance`: Owned mode instance to adopt, or `nullptr` to clear it.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::ClearServerGameMode()`

Shutdown and clear the active server game mode.

**Returns:** Success or an authority error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< GameFramework::NodeHandle > SnAPI::GameFramework::GameplayHost::CreateLocalPlayer(std::string Name, unsigned int PlayerIndex, bool ReplicatedPlayer=true, std::uint64_t OwnerConnectionId=0)`

Create a `LocalPlayer` node without applying join-policy or authority checks.

**Parameters**

- `Name`: Preferred node name. Empty names fall back to `"LocalPlayer"`.
- `PlayerIndex`: Player slot index for the owning connection.
- `ReplicatedPlayer`: Replication state for the created node.
- `OwnerConnectionId`: Owning connection id. `0` represents local authority.

**Returns:** Handle to the created player node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< GameFramework::NodeHandle > SnAPI::GameFramework::GameplayHost::JoinPlayer(std::uint64_t OwnerConnectionId, std::string Name={}, std::optional< unsigned int > PreferredPlayerIndex=std::nullopt, bool ReplicatedPlayer=true)`

Authoritatively ensure one player exists for an owning connection/index combination.

**Parameters**

- `OwnerConnectionId`: Owning connection id. `0` represents local authority.
- `Name`: Preferred player node name. Empty names use host-generated defaults.
- `PreferredPlayerIndex`: Requested player index, or `std::nullopt` for first available.
- `ReplicatedPlayer`: Replication state for the player node.

**Returns:** Handle to the existing or newly created player node, or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::LeavePlayer(const NodeHandle &PlayerHandle)`

Authoritatively remove one local-player node.

**Parameters**

- `PlayerHandle`: Target player handle.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::LeavePlayer(const Uuid &PlayerId)`

Authoritatively remove one local-player by id.

**Parameters**

- `PlayerId`: Stable id of the player to remove.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::LeavePlayersForConnection(std::uint64_t OwnerConnectionId)`

Authoritatively remove all local-players owned by one connection.

**Parameters**

- `OwnerConnectionId`: Owning connection id.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::HandleJoinPlayerRequest(std::uint64_t OwnerConnectionId, std::string Name={}, std::optional< unsigned int > PreferredPlayerIndex=std::nullopt, bool ReplicatedPlayer=true)`

Server-authoritative connection request entrypoint for joining a local-player.

**Parameters**

- `OwnerConnectionId`: Requesting connection id.
- `Name`: Requested player name, possibly empty.
- `PreferredPlayerIndex`: Requested player index, if any.
- `ReplicatedPlayer`: Requested replication state for the created player node.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::HandleLeavePlayerRequest(std::uint64_t OwnerConnectionId, std::optional< unsigned int > PlayerIndex=std::nullopt)`

Server-authoritative connection request entrypoint for leaving a local-player.

**Parameters**

- `OwnerConnectionId`: Requesting connection id.
- `PlayerIndex`: Requested player index, or `std::nullopt` for all caller-owned players.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::HandleLoadLevelRequest(std::uint64_t OwnerConnectionId, std::string Name)`

Server-authoritative connection request entrypoint for level load.

**Parameters**

- `OwnerConnectionId`: Requesting connection id.
- `Name`: Requested level name, possibly empty.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::HandleUnloadLevelRequest(std::uint64_t OwnerConnectionId, const Uuid &LevelId)`

Server-authoritative connection request entrypoint for level unload.

**Parameters**

- `OwnerConnectionId`: Requesting connection id.
- `LevelId`: Stable id of the level to unload.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::RequestJoinPlayer(std::string Name={}, std::optional< unsigned int > PreferredPlayerIndex=std::nullopt, bool ReplicatedPlayer=true)`

Request player join on the active authority.

**Parameters**

- `Name`: Requested player name, possibly empty.
- `PreferredPlayerIndex`: Requested player index, if any.
- `ReplicatedPlayer`: Requested replication state for the created player node.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::RequestLeavePlayer(std::optional< unsigned int > PlayerIndex=std::nullopt)`

Request player leave on the active authority.

**Parameters**

- `PlayerIndex`: Requested player index, or `std::nullopt` for all caller-owned players.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::RequestLoadLevel(std::string Name)`

Request level load on the active authority.

**Parameters**

- `Name`: Requested level name, possibly empty.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::RequestUnloadLevel(const Uuid &LevelId)`

Request level unload on the active authority.

**Parameters**

- `LevelId`: Stable id of the level to unload.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< GameFramework::NodeHandle > SnAPI::GameFramework::GameplayHost::LocalPlayers() const`

Query currently active local-player nodes.

**Returns:** Snapshot of player handles known to the world at the time of the call.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< GameFramework::NodeHandle > SnAPI::GameFramework::GameplayHost::LocalPlayersForConnection(std::uint64_t OwnerConnectionId) const`

Query local-player nodes owned by one connection.

**Parameters**

- `OwnerConnectionId`: Owning connection id.

**Returns:** Snapshot of matching player handles.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< GameFramework::NodeHandle > SnAPI::GameFramework::GameplayHost::LoadLevel(std::string Name)`

Create and attach one level node.

**Parameters**

- `Name`: Preferred level node name. Empty names fall back to `"Level"`.

**Returns:** Handle to the created level or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::UnloadLevel(const NodeHandle &LevelHandle)`

Schedule one level for end-of-frame unload.

**Parameters**

- `LevelHandle`: Handle of the level to destroy.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::UnloadLevel(const Uuid &LevelId)`

Schedule one level for end-of-frame unload by UUID.

**Parameters**

- `LevelId`: Stable id of the level to destroy.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::RegisterService(std::unique_ptr< IGameService > Service)`

Register a gameplay service instance.

**Parameters**

- `Service`: Owned service instance to adopt.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::UnregisterService(const std::type_index &ServiceType)`

Unregister a gameplay service type.

**Parameters**

- `ServiceType`: Concrete service type to remove.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TService & SnAPI::GameFramework::GameplayHost::RegisterService(TArgs &&... Args)`

Register a concrete gameplay service type.

**Parameters**

- `Args`: Constructor arguments for the new service.

**Returns:** Borrowed reference to the registered service instance.
</div>
<div class="snapi-api-card" markdown="1">
### `TService * SnAPI::GameFramework::GameplayHost::GetService()`

Query a gameplay service by type.

**Returns:** Non-owning pointer to the service instance or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `const TService * SnAPI::GameFramework::GameplayHost::GetService() const`

Query a gameplay service by type (const).

**Returns:** Non-owning pointer to the service instance or `nullptr`.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::RebuildServiceIndex()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::BuildServiceOrder()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::InitializeServices()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::ShutdownServices()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::TickServices(float DeltaSeconds)`

**Parameters**

- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::RefreshObservedWorldState(bool SeedOnly)`

**Parameters**

- `SeedOnly`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::RefreshObservedConnectionState(bool SeedOnly)`

**Parameters**

- `SeedOnly`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::NotifyLevelLoaded(const NodeHandle &LevelHandle)`

**Parameters**

- `LevelHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::NotifyLevelUnloaded(const Uuid &LevelId)`

**Parameters**

- `LevelId`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::NotifyLocalPlayerAdded(const NodeHandle &PlayerHandle)`

**Parameters**

- `PlayerHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::NotifyLocalPlayerRemoved(const Uuid &PlayerId)`

**Parameters**

- `PlayerId`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::NotifyConnectionAdded(std::uint64_t OwnerConnectionId)`

**Parameters**

- `OwnerConnectionId`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::NotifyConnectionRemoved(std::uint64_t OwnerConnectionId)`

**Parameters**

- `OwnerConnectionId`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::AutoCreateConfiguredLocalPlayer()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::EnsureRpcGatewayNode()`
</div>
<div class="snapi-api-card" markdown="1">
### `GameplayRpcGateway * SnAPI::GameFramework::GameplayHost::ResolveRpcGatewayNode() const`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::EvaluateJoinRequestPolicy(std::uint64_t OwnerConnectionId, const std::string &RequestedName, std::optional< unsigned int > PreferredPlayerIndex, bool ReplicatedPlayer)`

**Parameters**

- `OwnerConnectionId`: 
- `RequestedName`: 
- `PreferredPlayerIndex`: 
- `ReplicatedPlayer`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::EvaluateLeaveRequestPolicy(std::uint64_t OwnerConnectionId, std::optional< unsigned int > PlayerIndex)`

**Parameters**

- `OwnerConnectionId`: 
- `PlayerIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::EvaluateLoadLevelRequestPolicy(std::uint64_t OwnerConnectionId, const std::string &RequestedName)`

**Parameters**

- `OwnerConnectionId`: 
- `RequestedName`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameplayHost::EvaluateUnloadLevelRequestPolicy(std::uint64_t OwnerConnectionId, const Uuid &LevelId)`

**Parameters**

- `OwnerConnectionId`: 
- `LevelId`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::SyncLocalPlayerPossessionCallbacks()`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::GameplayHost::ResolvePlayerStart(LocalPlayer &Player)`

**Parameters**

- `Player`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::GameplayHost::SpawnPlayerPawn(LocalPlayer &Player, const NodeHandle &PlayerStart)`

**Parameters**

- `Player`: 
- `PlayerStart`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::GameplayHost::FindAutoPossessTarget(std::uint64_t OwnerConnectionId) const`

**Parameters**

- `OwnerConnectionId`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayHost::EnsurePlayerHasPossession(LocalPlayer &Player)`

**Parameters**

- `Player`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional< unsigned int > SnAPI::GameFramework::GameplayHost::FirstAvailablePlayerIndexForOwner(std::uint64_t OwnerConnectionId) const`

**Parameters**

- `OwnerConnectionId`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::GameplayHost::FindLocalPlayerByOwnerAndIndex(std::uint64_t OwnerConnectionId, unsigned int PlayerIndex) const`

**Parameters**

- `OwnerConnectionId`: 
- `PlayerIndex`:
</div>
