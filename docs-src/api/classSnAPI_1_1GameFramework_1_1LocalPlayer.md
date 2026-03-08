# SnAPI::GameFramework::LocalPlayer

Replicable player-ownership node used for local, remote, and splitscreen player state.

`LocalPlayer` is the world-owned representation of one player slot known to the gameplay host. Despite the name, the type is used for both truly local players and remote players; `OwnerConnectionId` distinguishes which connection owns the player. The node tracks the player index, possession target, optional input-device assignment, and whether this player should currently accept input.

Core semantics:
- the node is replicated by default
- possession is server-authoritative
- direct setters mutate local state immediately, while `RequestPossess()` / `RequestUnpossess()` use the RPC path when available

Ownership and lifetime:
- Owned by `World` like any other node.
- Usually created and destroyed by `GameplayHost`.
- `PossessedNode` is a non-owning handle to another world-owned node.

Threading model:
- Main-thread only.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `auto SnAPI::GameFramework::LocalPlayer::kTypeName`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `unsigned int SnAPI::GameFramework::LocalPlayer::m_playerIndex`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::LocalPlayer::m_possessedNode`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::LocalPlayer::m_lastNotifiedPossessedNode`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::LocalPlayer::m_acceptInput`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::LocalPlayer::m_ownerConnectionId`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::LocalPlayer::LocalPlayer()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::LocalPlayer::LocalPlayer(std::string Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `unsigned int & SnAPI::GameFramework::LocalPlayer::EditPlayerIndex()`

Access the player's slot index within its owning connection.

**Returns:** Mutable player-index field.
</div>
<div class="snapi-api-card" markdown="1">
### `const unsigned int & SnAPI::GameFramework::LocalPlayer::GetPlayerIndex() const`

Access the player's slot index within its owning connection.

**Returns:** Const player-index field.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle & SnAPI::GameFramework::LocalPlayer::EditPossessedNode()`

Access the currently possessed node handle.

**Returns:** Mutable possession handle field.
</div>
<div class="snapi-api-card" markdown="1">
### `const NodeHandle & SnAPI::GameFramework::LocalPlayer::GetPossessedNode() const`

Access the currently possessed node handle.

**Returns:** Const possession handle field.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::SetPossessedNode(const NodeHandle &Target)`

Set the currently possessed node immediately.

**Parameters**

- `Target`: Desired possession target, or null handle to clear possession.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::SyncPossessionCallbacks()`

Reconcile possession callbacks with the currently replicated possession state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool & SnAPI::GameFramework::LocalPlayer::EditAcceptInput()`

Access the flag that allows this player to consume input.

**Returns:** Mutable input-acceptance field.
</div>
<div class="snapi-api-card" markdown="1">
### `const bool & SnAPI::GameFramework::LocalPlayer::GetAcceptInput() const`

Access the flag that allows this player to consume input.

**Returns:** Const input-acceptance field.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t & SnAPI::GameFramework::LocalPlayer::EditOwnerConnectionId()`

Access the owning network connection id.

`0` represents local authority.

**Returns:** Mutable connection-id field.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::uint64_t & SnAPI::GameFramework::LocalPlayer::GetOwnerConnectionId() const`

Access the owning network connection id.

`0` represents local authority.

**Returns:** Const connection-id field.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::RequestPossess(const NodeHandle &Target)`

Request possession of target node.

**Parameters**

- `Target`: Desired possession target, or null handle to clear possession.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::RequestUnpossess()`

Request possession clear.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::ServerRequestPossess(const NodeHandle &Target)`

Server-authoritative possession RPC endpoint.

**Parameters**

- `Target`: Desired possession target, or null handle to clear possession.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::ServerRequestUnpossess()`

Server-authoritative unpossession RPC endpoint.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::LocalPlayer::DispatchPossessionTransition(const NodeHandle &PreviousTarget, const NodeHandle &NewTarget)`

**Parameters**

- `PreviousTarget`: 
- `NewTarget`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::LocalPlayer::CanPossessTarget(const NodeHandle &Target) const`

**Parameters**

- `Target`:
</div>
