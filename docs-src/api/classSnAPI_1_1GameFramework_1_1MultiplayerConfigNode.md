# SnAPI::GameFramework::MultiplayerConfigNode

Data-only configuration node for local-multiplayer startup policy.

`MultiplayerConfigNode` is a lightweight settings container that can be placed in content or worlds to describe how many local players should exist and whether splitscreen-style behavior should be enabled. It does not perform any orchestration itself; higher-level systems are expected to interpret its fields.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::MultiplayerConfigNode::kTypeName`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `int SnAPI::GameFramework::MultiplayerConfigNode::m_localPlayerCount`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MultiplayerConfigNode::m_splitscreen`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MultiplayerConfigNode::m_autoJoinAdditionalLocalPlayers`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MultiplayerConfigNode::m_requireGamepadForAdditionalPlayers`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::MultiplayerConfigNode::MultiplayerConfigNode()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::MultiplayerConfigNode::MultiplayerConfigNode(std::string Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `int & SnAPI::GameFramework::MultiplayerConfigNode::EditLocalPlayerCount()`

Access the desired local-player count.

**Returns:** Mutable player-count field.
</div>
<div class="snapi-api-card" markdown="1">
### `const int & SnAPI::GameFramework::MultiplayerConfigNode::GetLocalPlayerCount() const`

Access the desired local-player count.

**Returns:** Const player-count field.
</div>
<div class="snapi-api-card" markdown="1">
### `bool & SnAPI::GameFramework::MultiplayerConfigNode::EditSplitscreen()`

Access the splitscreen enable flag.

**Returns:** Mutable splitscreen field.
</div>
<div class="snapi-api-card" markdown="1">
### `const bool & SnAPI::GameFramework::MultiplayerConfigNode::GetSplitscreen() const`

Access the splitscreen enable flag.

**Returns:** Const splitscreen field.
</div>
<div class="snapi-api-card" markdown="1">
### `bool & SnAPI::GameFramework::MultiplayerConfigNode::EditAutoJoinAdditionalLocalPlayers()`

Access the auto-join policy for additional local players.

**Returns:** Mutable auto-join field.
</div>
<div class="snapi-api-card" markdown="1">
### `const bool & SnAPI::GameFramework::MultiplayerConfigNode::GetAutoJoinAdditionalLocalPlayers() const`

Access the auto-join policy for additional local players.

**Returns:** Const auto-join field.
</div>
<div class="snapi-api-card" markdown="1">
### `bool & SnAPI::GameFramework::MultiplayerConfigNode::EditRequireGamepadForAdditionalPlayers()`

Access the policy requiring a gamepad for additional local players.

**Returns:** Mutable requirement field.
</div>
<div class="snapi-api-card" markdown="1">
### `const bool & SnAPI::GameFramework::MultiplayerConfigNode::GetRequireGamepadForAdditionalPlayers() const`

Access the policy requiring a gamepad for additional local players.

**Returns:** Const requirement field.
</div>
