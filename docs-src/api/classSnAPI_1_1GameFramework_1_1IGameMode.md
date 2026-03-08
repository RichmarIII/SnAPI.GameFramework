# SnAPI::GameFramework::IGameMode

Server-authoritative gameplay rule layer.

`IGameMode` represents the authoritative rules for one gameplay session. Unlike `IGame`, the mode exists only where server authority exists: standalone runtime, listen server, or dedicated server. Client-only runtimes should assume that no local mode instance exists.

Typical responsibilities:
- authoritative player-join and leave policy
- spawn-point and pawn-type decisions that must be server-owned
- level-load/unload authorization
- server-side per-frame rule evaluation

Ownership and lifetime:
- Owned by `GameplayHost`.
- Replaced or cleared through `GameplayHost::SetServerGameMode()`.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IGameMode::~IGameMode()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::string_view SnAPI::GameFramework::IGameMode::Name() const =0`

Stable diagnostic name for the concrete game-mode implementation.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IGameMode::Initialize(GameplayHost &Host)=0`

Initialize mode state.

**Parameters**

- `Host`: Borrowed gameplay host.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::Tick(GameplayHost &Host, float DeltaSeconds)`

Per-frame authoritative mode update.

**Parameters**

- `Host`: Borrowed gameplay host.
- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::IGameMode::SelectInitialPossessionTarget(GameplayHost &Host, LocalPlayer &Player)`

Optional initial possession-target resolver for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.

**Returns:** Handle to a possession target, or a null handle to defer to later resolvers.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::IGameMode::SelectPlayerStart(GameplayHost &Host, LocalPlayer &Player)`

Optional player-start resolver for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.

**Returns:** Handle to a `PlayerStart`, or a null handle to defer to later resolvers.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::optional< TypeId > SnAPI::GameFramework::IGameMode::SelectSpawnedPawnType(GameplayHost &Host, LocalPlayer &Player, const NodeHandle &PlayerStart)`

Optional spawned-pawn type override for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.
- `PlayerStart`: 

**Returns:** Pawn type id or `std::nullopt` to keep host/default behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::optional< bool > SnAPI::GameFramework::IGameMode::SelectSpawnedPawnReplicated(GameplayHost &Host, LocalPlayer &Player, const NodeHandle &PlayerStart)`

Optional replication-policy override for the pawn spawned for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.
- `PlayerStart`: 

**Returns:** Replication preference or `std::nullopt` to keep host/default behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameMode::AllowPlayerJoinRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const std::string &RequestedName, std::optional< unsigned int > PreferredPlayerIndex, bool ReplicatedPlayer)`

Policy hook for connection-authored join requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `RequestedName`: Requested player name, possibly empty.
- `PreferredPlayerIndex`: Requested player index, if any.
- `ReplicatedPlayer`: Requested replication state for the created player node.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameMode::AllowPlayerLeaveRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, std::optional< unsigned int > PlayerIndex)`

Policy hook for connection-authored leave requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `PlayerIndex`: Requested player index, or `std::nullopt` for all caller-owned players.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameMode::AllowLevelLoadRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const std::string &RequestedName)`

Policy hook for connection-authored level-load requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `RequestedName`: Requested level name, possibly empty.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameMode::AllowLevelUnloadRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const Uuid &LevelId)`

Policy hook for connection-authored level-unload requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `LevelId`: Stable id of the level targeted for unload.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::OnLevelLoaded(GameplayHost &Host, const NodeHandle &LevelHandle)`

Notification that a level became present in the world.

**Parameters**

- `Host`: Borrowed gameplay host.
- `LevelHandle`: Loaded level handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::OnLevelUnloaded(GameplayHost &Host, const Uuid &LevelId)`

Notification that a level was removed from the world.

**Parameters**

- `Host`: Borrowed gameplay host.
- `LevelId`: Stable id of the unloaded level.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::OnLocalPlayerAdded(GameplayHost &Host, const NodeHandle &PlayerHandle)`

Notification that a local-player node was added.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerHandle`: Added player handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::OnLocalPlayerRemoved(GameplayHost &Host, const Uuid &PlayerId)`

Notification that a local-player node was removed.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerId`: Stable id of the removed player.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::OnConnectionAdded(GameplayHost &Host, std::uint64_t OwnerConnectionId)`

Notification that a connection became visible to the gameplay host.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Connection id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::OnConnectionRemoved(GameplayHost &Host, std::uint64_t OwnerConnectionId)`

Notification that a connection is no longer visible to the gameplay host.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Connection id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameMode::Shutdown(GameplayHost &Host)=0`

Shutdown mode state and release authoritative resources.

**Parameters**

- `Host`: Borrowed gameplay host.
</div>
