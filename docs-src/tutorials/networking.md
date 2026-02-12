# Networking Replication and RPC

This page explains how SnAPI.GameFramework networking works with SnAPI.Networking.

## 1. Big Picture

- `NetReplicationBridge` maps graph objects to replication entities.
- `NetRpcBridge` maps reflected methods to RPC calls.
- Reflection metadata (`FieldFlags`, `MethodFlags`) controls what is eligible.

## 2. Replication Requires Two Things

A field replicates only when **both** are true:

1. the field is marked with `EFieldFlagBits::Replication`
2. the owning runtime object has replication enabled:
   - `Node->Replicated(true)`
   - `Component->Replicated(true)`

## 3. Define a Replicated Component

```cpp
struct ReplicatedTransformComponent final : public IComponent
{
    static constexpr const char* kTypeName = "MyGame::ReplicatedTransformComponent";

    Vec3 Position{};
    Vec3 Rotation{};
    Vec3 Scale{1.0f, 1.0f, 1.0f};
};

SNAPI_REFLECT_TYPE(ReplicatedTransformComponent, (TTypeBuilder<ReplicatedTransformComponent>(ReplicatedTransformComponent::kTypeName)
    .Field("Position", &ReplicatedTransformComponent::Position, EFieldFlagBits::Replication)
    .Field("Rotation", &ReplicatedTransformComponent::Rotation, EFieldFlagBits::Replication)
    .Field("Scale", &ReplicatedTransformComponent::Scale, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));
```

## 4. Server Setup

```cpp
NetSession ServerSession(Config);
ServerSession.Role(ESessionRole::Server);

auto Replication = ReplicationService::Create(ServerSession);

NodeGraph ServerGraph("ServerGraph");
NetReplicationBridge ServerBridge(ServerGraph);

Replication->EntityProvider(&ServerBridge);
Replication->InterestProvider(&ServerBridge);
Replication->PriorityProvider(&ServerBridge);

auto NodeResult = ServerGraph.CreateNode("Cube_0");
auto* Node = NodeResult->Borrowed();
Node->Replicated(true);

auto TransformResult = Node->Add<ReplicatedTransformComponent>();
auto* Transform = &*TransformResult;
Transform->Replicated(true);
```

During runtime, update server-side fields and pump session.
Clients receive spawn/update automatically.

## 5. Client Setup

```cpp
NetSession ClientSession(Config);
ClientSession.Role(ESessionRole::Client);

auto Replication = ReplicationService::Create(ClientSession);

NodeGraph ClientGraph("ClientGraph");
NetReplicationBridge ClientBridge(ClientGraph);

Replication->Receiver(&ClientBridge);
```

As packets arrive, client graph objects are created/updated by the bridge.

## 6. Why Nodes Appear on Client Without Spawn RPC

Node/component creation is part of replication spawn payload handling (`OnSpawn`), not a separate explicit gameplay RPC call.

That is why you can see client objects appear even when you only configured replication.

Identity is preserved with object UUIDs and replication entity IDs carried in payload headers.

## 7. Process Model (Two Executables)

`examples/MultiplayerExample` is designed for separate processes:

```bash
./MultiplayerExample --server
./MultiplayerExample --client --host <server-ip>
```

This is the intended model for real deployment.

## 8. Add Connection Diagnostics (Recommended)

Use `INetSessionListener` and periodic `DumpConnections(...)` logging.
This helps catch pacing/reliability backpressure and disconnect reasons quickly.

The multiplayer example includes this pattern (`SessionListener`, `PrintConnectionDump`).

## 9. Reflection RPC (Optional but Powerful)

Mark methods for RPC in reflection metadata:

```cpp
class WeaponComponent final : public IComponent
{
public:
    static constexpr const char* kTypeName = "MyGame::WeaponComponent";

    void ServerSetFiring(bool Value) { m_firing = Value; }

private:
    bool m_firing = false;
};

SNAPI_REFLECT_COMPONENT(WeaponComponent, (TTypeBuilder<WeaponComponent>(WeaponComponent::kTypeName)
    .Method("ServerSetFiring",
            &WeaponComponent::ServerSetFiring,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Constructor<>()
    .Register()));
```

Bind bridge to `RpcService`:

```cpp
auto Rpc = RpcService::Create(Session);
NetRpcBridge RpcBridge(&Graph);
RpcBridge.Bind(*Rpc, 1);
```

Call by reflected method name:

```cpp
std::array<Variant, 1> Args{Variant::FromValue(true)};
RpcBridge.Call(ConnectionHandle,
               *WeaponComponentPtr,
               "ServerSetFiring",
               Args);
```

`NetRpcBridge` resolves method metadata, serializes arguments via `ValueCodecRegistry`, and routes through SnAPI.Networking RPC.

## 10. Common Replication Problems

- Objects never replicate:
  - forgot `Replicated(true)` on node/component
  - forgot `EFieldFlagBits::Replication` on fields
- Initial spawn works, then updates stop:
  - inspect connection dumps for pending reliable growth/disconnect reasons
  - check pacing/MTU settings in `NetConfig`
- Type not found on remote:
  - ensure reflected type macro is linked and `RegisterBuiltinTypes()` ran

Next: [Audio Components](audio.md)
