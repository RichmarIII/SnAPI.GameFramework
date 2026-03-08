# Networking, Replication, and Reflected RPC

Networking in the framework has two layers.

- session-level flow through `NetworkSystem` and `GameplayHost`
- object-level flow through replication flags and reflected RPC

Do not collapse those into one mental bucket.

## 1. Bootstrap A Session

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "ServerWorld";

GameRuntimeNetworkingSettings Net{};
Net.Role = ESessionRole::Server;
Net.BindAddress = "0.0.0.0";
Net.BindPort = 7777;
Net.AutoConnect = false;
Settings.Networking = Net;

if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

A client build would usually set:

- `Role = ESessionRole::Client`
- `ConnectAddress`
- `ConnectPort`
- `AutoConnect = true`

## 2. What `NetworkSystem` Owns

The world networking subsystem owns:

- the session/transport bootstrap path
- the replication bridge
- the reflected RPC bridge
- session-role state visible to nodes and components

That is why `BaseNode::IsServer()` and `BaseComponent::IsServer()` can answer role questions.

## 3. Replication Has Two Gates

A field only replicates when both gates are open.

### Gate 1: the object must be replicated

- `Node->Replicated(true)`
- `Component->Replicated(true)`

### Gate 2: the field must carry replication metadata

```cpp
.Field("Health", &MyNode::Health, EFieldFlagBits::Replication)
```

If either gate is missing, the field does not replicate.

## 4. Reflected RPC Is Exact

Reflected RPC dispatch is driven by:

- target object UUID
- reflected concrete type
- deterministic method id
- exact parameter signature

That means the bridge is powerful, but not fuzzy. Argument types must match the reflected method signature exactly.

## 5. Declare A Small RPC Surface

```cpp
class DoorNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "MyGame::DoorNode";

    bool Open = false;

    void ServerToggle()
    {
        Open = !Open;
        CallRPC("ClientSync", {Variant::FromValue(Open)});
    }

    void ClientSync(bool NewOpen)
    {
        Open = NewOpen;
    }
};

SNAPI_REFLECT_TYPE(DoorNode, (TTypeBuilder<DoorNode>(DoorNode::kTypeName)
    .Base<BaseNode>()
    .Field("Open", &DoorNode::Open, EFieldFlagBits::Replication)
    .Method("ServerToggle", &DoorNode::ServerToggle,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ClientSync", &DoorNode::ClientSync,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetClient)
    .Constructor<>()
    .Register()));
```

And later:

```cpp
Door->CallRPC("ServerToggle");
```

`CallRPC(...)` still exists and is valid for node/component reflected RPC. The point is not that it disappeared. The point is that session-level flow should not be modeled entirely through ad hoc object RPCs.

## 6. Use `GameplayHost` For Session-Level Actions

For player and level flow, prefer the high-level API.

Examples:

- `RequestJoinPlayer(...)`
- `RequestLeavePlayer(...)`
- `RequestLoadLevel(...)`
- `RequestUnloadLevel(...)`

Those calls:

- execute immediately on authority
- route through the gameplay RPC gateway on pure clients
- apply game/game-mode/service policy hooks on the authoritative side

That is the modern replacement for writing every session action as a custom node RPC.

## 7. What `NetReplicationBridge` Actually Does

The replication bridge gathers world entities and can:

- spawn nodes and components on demand
- update replicated fields
- buffer unresolved parent/component relationships until dependencies exist
- despawn mapped entities

This is why out-of-order parent/component delivery is tolerable.

## 8. Ambient RPC Context

When a reflected inbound RPC is being handled, `NetRpcInvocationContext::CurrentConnection()` exposes the initiating connection as thread-local context.

Use it when you need connection-aware behavior during an inbound reflected method without threading a connection id through every method signature.

## 9. Common Mistakes

### Using only object-level RPC for session flow

Use `GameplayHost` for session-level actions.

### Marking fields replicated but forgetting object replication

Both gates matter.

### Assuming inbound RPC runs on the game thread

The default inbound dispatch path is on the networking service's net thread. Treat that as real unless your surrounding integration marshals it elsewhere.

## What To Read Next

- [Net Arena](net_arena.md)
- [Couch Co-op Laser Tag](couch_coop_laser_tag.md)
