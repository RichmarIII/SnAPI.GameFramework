# SnAPI::GameFramework::GameplayRpcGateway

Deterministically-addressable RPC gateway node for gameplay authority requests.

`GameplayRpcGateway` is the narrow RPC surface used by clients to ask the authoritative side to perform gameplay-host operations such as joining/leaving players or loading/unloading levels. The node is created with a deterministic UUID so all runtimes can target it without discovery or replication of a random identity.

Core semantics:
- It does not own gameplay logic itself; it forwards validated requests into `GameplayHost`.
- It is intentionally non-replicated and resolved by deterministic UUID.
- Server endpoint methods are no-ops when called without server authority.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::GameplayRpcGateway::kTypeName`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::GameplayRpcGateway::GameplayRpcGateway()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::GameplayRpcGateway::GameplayRpcGateway(std::string Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayRpcGateway::ServerRequestJoinPlayer(std::string RequestedName, int PreferredPlayerIndex, bool ReplicatedPlayer)`

Server-authoritative player join request endpoint.

**Parameters**

- `RequestedName`: Optional preferred player node name.
- `PreferredPlayerIndex`: Player index, or `-1` for auto-assignment.
- `ReplicatedPlayer`: Replication state for the created local-player.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayRpcGateway::ServerRequestLeavePlayer(int PlayerIndex)`

Server-authoritative player leave request endpoint.

**Parameters**

- `PlayerIndex`: Player index to remove, or `-1` for all caller-owned players.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayRpcGateway::ServerRequestLoadLevel(std::string RequestedName)`

Server-authoritative level load request endpoint.

**Parameters**

- `RequestedName`: Optional level node name.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameplayRpcGateway::ServerRequestUnloadLevel(std::string LevelIdText)`

Server-authoritative level unload request endpoint.

**Parameters**

- `LevelIdText`: UUID string of the target level node.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `const Uuid & SnAPI::GameFramework::GameplayRpcGateway::GatewayNodeId()`

Deterministic UUID used by all runtimes for the gateway node.

**Returns:** Stable UUID shared by all gameplay runtimes.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `GameplayHost * SnAPI::GameFramework::GameplayRpcGateway::ResolveGameplayHost() const`
</div>
