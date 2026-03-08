# SnAPI::GameFramework::IGame

Session-wide gameplay root that exists independently of server-only rule enforcement.

`IGame` is the gameplay/session object that most closely corresponds to a "game instance" style abstraction. It can exist on both server and clients and is responsible for session-wide behavior that is not inherently server-only, such as broad lifecycle flow, player-start selection hints, possession defaults, and policy checks that should run everywhere the session is represented.

Responsibility split:
- `IGame` is the session-wide layer and may exist on both server and client.
- `IGameMode` is the authoritative server-only rule layer.
- `GameplayHost` owns the actual `IGame` instance and calls it at well-defined points.

Ownership and lifetime:
- Implementations are heap-allocated and transferred to `GameplayHost`.
- The host owns the instance for the duration of the active gameplay session.
- `GameplayHost&` parameters are borrowed and remain owned by the runtime.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IGame::~IGame()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::string_view SnAPI::GameFramework::IGame::Name() const =0`

Stable diagnostic name for the concrete game implementation.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IGame::Initialize(GameplayHost &Host)=0`

Initialize game state for a newly started gameplay session.

**Parameters**

- `Host`: Borrowed gameplay host that owns this instance.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::Tick(GameplayHost &Host, float DeltaSeconds)`

Per-frame session update.

**Parameters**

- `Host`: Borrowed gameplay host.
- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::unique_ptr< IGameMode > SnAPI::GameFramework::IGame::CreateInitialGameMode(GameplayHost &Host)`

Optional hook that creates the initial authoritative game mode.

**Parameters**

- `Host`: Borrowed gameplay host.

**Returns:** Owned game mode instance or `nullptr` to skip automatic mode creation.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::IGame::SelectInitialPossessionTarget(GameplayHost &Host, LocalPlayer &Player)`

Optional initial possession-target resolver for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.

**Returns:** Handle to the desired possession target, or a null handle to defer to later resolvers.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::IGame::SelectPlayerStart(GameplayHost &Host, LocalPlayer &Player)`

Optional player-start resolver for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.

**Returns:** Handle to a `PlayerStart`, or a null handle to defer to later resolvers.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::optional< TypeId > SnAPI::GameFramework::IGame::SelectSpawnedPawnType(GameplayHost &Host, LocalPlayer &Player, const NodeHandle &PlayerStart)`

Optional spawned-pawn type override for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.
- `PlayerStart`: 

**Returns:** Concrete pawn type id to spawn, or `std::nullopt` to keep host/default behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::optional< bool > SnAPI::GameFramework::IGame::SelectSpawnedPawnReplicated(GameplayHost &Host, LocalPlayer &Player, const NodeHandle &PlayerStart)`

Optional replication-policy override for the pawn spawned for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.
- `PlayerStart`: 

**Returns:** Replication preference or `std::nullopt` to keep host/default behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGame::AllowPlayerJoinRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const std::string &RequestedName, std::optional< unsigned int > PreferredPlayerIndex, bool ReplicatedPlayer)`

Policy hook for connection-authored join requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id. `0` represents local/standalone authority.
- `RequestedName`: Requested player name, possibly empty.
- `PreferredPlayerIndex`: Requested player index, if any.
- `ReplicatedPlayer`: Requested replication state for the created player node.

**Returns:** `true` to allow the request, `false` to deny it before host mutation occurs.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGame::AllowPlayerLeaveRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, std::optional< unsigned int > PlayerIndex)`

Policy hook for connection-authored leave requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `PlayerIndex`: Requested player index, or `std::nullopt` for all caller-owned players.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGame::AllowLevelLoadRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const std::string &RequestedName)`

Policy hook for connection-authored level-load requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `RequestedName`: Requested level name, possibly empty.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGame::AllowLevelUnloadRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const Uuid &LevelId)`

Policy hook for connection-authored level-unload requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `LevelId`: Stable id of the level targeted for unload.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::OnLevelLoaded(GameplayHost &Host, const NodeHandle &LevelHandle)`

Notification that a level became present in the world.

**Parameters**

- `Host`: Borrowed gameplay host.
- `LevelHandle`: Loaded level handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::OnLevelUnloaded(GameplayHost &Host, const Uuid &LevelId)`

Notification that a level was removed from the world.

**Parameters**

- `Host`: Borrowed gameplay host.
- `LevelId`: Stable id of the unloaded level.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::OnLocalPlayerAdded(GameplayHost &Host, const NodeHandle &PlayerHandle)`

Notification that a local-player node was added.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerHandle`: Added player handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::OnLocalPlayerRemoved(GameplayHost &Host, const Uuid &PlayerId)`

Notification that a local-player node was removed.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerId`: Stable id of the removed player.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::OnConnectionAdded(GameplayHost &Host, std::uint64_t OwnerConnectionId)`

Notification that a connection became visible to the gameplay host.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Connection id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::OnConnectionRemoved(GameplayHost &Host, std::uint64_t OwnerConnectionId)`

Notification that a connection is no longer visible to the gameplay host.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Connection id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGame::Shutdown(GameplayHost &Host)=0`

Shutdown game state and release host-dependent resources.

**Parameters**

- `Host`: Borrowed gameplay host.
</div>
