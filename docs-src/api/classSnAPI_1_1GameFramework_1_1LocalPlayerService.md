# SnAPI::GameFramework::LocalPlayerService

Default gameplay service that maps locally owned players to local input devices.

`LocalPlayerService` is the stock service registered by `GameplayHost` when `GameRuntimeGameplaySettings::RegisterDefaultLocalPlayerService` is enabled. It inspects the current input snapshot and keeps `LocalPlayer` device assignment state synchronized with the available local gamepads.

Policy:
- locally owned player index `N` maps to gamepad slot `N` when one exists
- remote-owned players are explicitly stripped of local device assignments
- player index `0` naturally falls back to the unassigned keyboard/mouse path when no gamepad exists

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::LocalPlayerService::Name() const override`

Stable diagnostic name for the concrete service implementation.
</div>
<div class="snapi-api-card" markdown="1">
### `int SnAPI::GameFramework::LocalPlayerService::Priority() const override`

Optional ordering priority among dependency-ready services.

**Returns:** Relative priority value.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::LocalPlayerService::Initialize(GameplayHost &Host) override`

Initialize service state.

**Parameters**

- `Host`: Borrowed gameplay host.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayerService::Tick(GameplayHost &Host, float DeltaSeconds) override`

Per-frame service update.

**Parameters**

- `Host`: Borrowed gameplay host.
- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayerService::OnLocalPlayerAdded(GameplayHost &Host, const NodeHandle &PlayerHandle) override`

Notification that a local-player node was added.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerHandle`: Added player handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayerService::OnLocalPlayerRemoved(GameplayHost &Host, const Uuid &PlayerId) override`

Notification that a local-player node was removed.

**Parameters**

- `Host`: Borrowed gameplay host.
- `PlayerId`: Stable id of the removed player.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayerService::Shutdown(GameplayHost &Host) override`

Shutdown and release service state.

**Parameters**

- `Host`: Borrowed gameplay host.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayerService::RefreshAssignments(GameplayHost &Host)`

**Parameters**

- `Host`:
</div>
