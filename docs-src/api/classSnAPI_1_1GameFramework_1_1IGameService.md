# SnAPI::GameFramework::IGameService

Contract for modular gameplay services owned by `GameplayHost`.

`IGameService` is the extension point for gameplay features that do not belong inside one monolithic `IGame` or `IGameMode` implementation. Services are registered into the host, ordered by declared dependencies and priority, initialized once, ticked every frame, and shut down in reverse order.

Use services for:
- modular policy layers
- player/session support systems
- gameplay-side adapters that need lifecycle hooks but are not node/component types

Ownership and lifetime:
- Services are heap-allocated and transferred to `GameplayHost`.
- The host owns the service instances and controls their lifecycle.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IGameService::~IGameService()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::string_view SnAPI::GameFramework::IGameService::Name() const =0`

Stable diagnostic name for the concrete service implementation.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::vector< std::type_index > SnAPI::GameFramework::IGameService::Dependencies() const`

Optional dependency list expressed as concrete service types.

**Returns:** Types that must already be initialized before this service may initialize.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual int SnAPI::GameFramework::IGameService::Priority() const`

Optional ordering priority among dependency-ready services.

**Returns:** Relative priority value.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IGameService::Initialize(GameplayHost &Host)=0`

Initialize service state.

**Parameters**

- `Host`: Borrowed gameplay host.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::Tick(GameplayHost &Host, float DeltaSeconds)`

Per-frame service update.

**Parameters**

- `Host`: Borrowed gameplay host.
- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::IGameService::SelectInitialPossessionTarget(GameplayHost &Host, LocalPlayer &Player)`

Optional initial possession-target resolver for a newly joined player.

**Parameters**

- `Host`: Borrowed gameplay host.
- `Player`: Borrowed player node being initialized.

**Returns:** Handle to a possession target, or a null handle to defer to later resolvers.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameService::AllowPlayerJoinRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const std::string &RequestedName, std::optional< unsigned int > PreferredPlayerIndex, bool ReplicatedPlayer)`

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
### `virtual bool SnAPI::GameFramework::IGameService::AllowPlayerLeaveRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, std::optional< unsigned int > PlayerIndex)`

Policy hook for connection-authored leave requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `PlayerIndex`: Requested player index, or `std::nullopt` for all caller-owned players.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameService::AllowLevelLoadRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const std::string &RequestedName)`

Policy hook for connection-authored level-load requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `RequestedName`: Requested level name, possibly empty.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IGameService::AllowLevelUnloadRequest(GameplayHost &Host, std::uint64_t OwnerConnectionId, const Uuid &LevelId)`

Policy hook for connection-authored level-unload requests.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Requesting connection id.
- `LevelId`: Stable id of the level targeted for unload.

**Returns:** `true` to allow the request, `false` to deny it.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::OnLevelLoaded(GameplayHost &Host, const NodeHandle &LevelHandle)`

Notification that a level became present in the world.

**Parameters**

- `Host`: Borrowed gameplay host.
- `LevelHandle`: Loaded level handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::OnLevelUnloaded(GameplayHost &Host, const Uuid &LevelId)`

Notification that a level was removed from the world.

**Parameters**

- `Host`: Borrowed gameplay host.
- `LevelId`: Stable id of the unloaded level.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::OnLocalPlayerAdded(GameplayHost &Host, const NodeHandle &PlayerHandle)`

Notification that a local-player node was added.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerHandle`: Added player handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::OnLocalPlayerRemoved(GameplayHost &Host, const Uuid &PlayerId)`

Notification that a local-player node was removed.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerId`: Stable id of the removed player.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::OnConnectionAdded(GameplayHost &Host, std::uint64_t OwnerConnectionId)`

Notification that a connection became visible to the gameplay host.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Connection id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::OnConnectionRemoved(GameplayHost &Host, std::uint64_t OwnerConnectionId)`

Notification that a connection is no longer visible to the gameplay host.

**Parameters**

- `Host`: Borrowed gameplay host.
- `OwnerConnectionId`: Connection id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IGameService::Shutdown(GameplayHost &Host)=0`

Shutdown and release service state.

**Parameters**

- `Host`: Borrowed gameplay host.
</div>
