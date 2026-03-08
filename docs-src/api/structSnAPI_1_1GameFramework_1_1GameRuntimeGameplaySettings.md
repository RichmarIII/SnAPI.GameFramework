# SnAPI::GameFramework::GameRuntimeGameplaySettings

Gameplay bootstrap policy consumed by `GameplayHost`.

`GameRuntimeGameplaySettings` describes how the gameplay host should construct and manage its high-level session objects and convenience behaviors. It intentionally uses raw factory function pointers instead of `std::function` so the default path remains deterministic, allocation-free, and trivial to store in runtime settings.

Semantics:
- null factories mean "do not auto-create that object"
- auto-create flags only affect host bootstrap behavior; they do not prevent later manual calls
- connection/player automation applies only where the host has authority to act

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::GameRuntimeGameplaySettings::GameFactory = std::unique_ptr<IGame>(*)()`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::GameRuntimeGameplaySettings::GameModeFactory = std::unique_ptr<IGameMode>(*)()`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `GameFactory SnAPI::GameFramework::GameRuntimeGameplaySettings::CreateGame`

Optional session-game factory.
</div>
<div class="snapi-api-card" markdown="1">
### `GameModeFactory SnAPI::GameFramework::GameRuntimeGameplaySettings::CreateServerGameMode`

Optional server-mode factory override.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeGameplaySettings::AutoCreateLocalPlayer`

Create one local-player node during initialize.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeGameplaySettings::AutoCreateReplicatedLocalPlayer`

Replication gate for auto-created local player.
</div>
<div class="snapi-api-card" markdown="1">
### `unsigned int SnAPI::GameFramework::GameRuntimeGameplaySettings::AutoCreateLocalPlayerIndex`

Player index used for auto-created local player.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::GameRuntimeGameplaySettings::AutoCreateLocalPlayerName`

Name used for auto-created local player node.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeGameplaySettings::RegisterDefaultLocalPlayerService`

Register default local-player input-mapping service.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeGameplaySettings::AutoCreateRemotePlayerOnConnection`

Server: auto-create one replicated local-player per remote connection.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeGameplaySettings::AutoDestroyRemotePlayersOnDisconnect`

Server: remove local-players owned by disconnected remote connections.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntimeGameplaySettings::SetGameFactory()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntimeGameplaySettings::SetServerGameModeFactory()`
</div>
