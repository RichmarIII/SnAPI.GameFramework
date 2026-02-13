# Networking Replication and RPC

This page explains how SnAPI.GameFramework networking works with SnAPI.Networking.

## 1. Big Picture

- `World` owns `NetworkSystem` (session + transport + replication bridge + RPC bridge wiring).
- `NetworkSystem` is owner-only for network runtime resources (no external session attach path).
- `NetReplicationBridge` maps graph objects to replication entities and applies spawn/update/despawn payloads.
- `NetRpcBridge` maps reflected methods to network RPC calls.
- `INode::CallRPC(...)` and `IComponent::CallRPC(...)` are gameplay-facing helpers that route by reflected method flags + network role.
- `World::Tick` / `World::EndFrame` pump networking; `GameRuntime::Update` orchestrates world lifecycle phases.
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

## 4. Server Setup (Recommended `GameRuntime` Path)

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "ServerWorld";
Settings.Tick.EnableFixedTick = false;

GameRuntimeNetworkingSettings Net{};
Net.Role = ESessionRole::Server;
Net.Net = Config;
Net.BindAddress = "0.0.0.0";
Net.BindPort = 7777;
Net.AutoConnect = false;
Settings.Networking = Net;

auto InitResult = Runtime.Init(Settings);
if (!InitResult)
{
    // handle error
}

auto& ServerWorld = Runtime.World();
auto NodeResult = ServerWorld.CreateNode("Cube_0");
auto* Node = NodeResult->Borrowed();
Node->Replicated(true);

auto TransformResult = Node->Add<ReplicatedTransformComponent>();
auto* Transform = &*TransformResult;
Transform->Replicated(true);
```

During runtime, update server-side fields and call `Runtime.Update(DeltaSeconds)`.
Clients receive spawn/update automatically.

There is no separate `AttachSession(...)` step in GameFramework anymore.
The world networking subsystem owns and configures session/transport from `Settings.Networking`.

## 5. Client Setup

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "ClientWorld";
Settings.Tick.EnableFixedTick = false;

GameRuntimeNetworkingSettings Net{};
Net.Role = ESessionRole::Client;
Net.Net = Config;
Net.BindAddress = "0.0.0.0";
Net.BindPort = 0; // ephemeral local port
Net.ConnectAddress = "127.0.0.1";
Net.ConnectPort = 7777;
Net.AutoConnect = true;
Settings.Networking = Net;

auto InitResult = Runtime.Init(Settings);
if (!InitResult)
{
    // handle error
}

auto& ClientWorld = Runtime.World();
```

As packets arrive, client world objects are created/updated by the bridge.

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

## 8. Connection Diagnostics (Recommended)

Use `INetSessionListener` and periodic `DumpConnections(...)` logging.
This helps catch pacing/reliability backpressure and disconnect reasons quickly.

The multiplayer example includes this pattern (`SessionListener`, `PrintConnectionDump`).

Important counters in dumps:

- `pending_rel`: reliable messages queued/in-flight.
- `pending_unrel`: queued/in-flight unreliable messages.
- `pkt_sent`, `pkt_acked`, `pkt_lost`: transport packet stats, not direct gameplay event counts.

### Interpreting packet loss correctly

Seeing non-zero `pkt_lost` (especially right after connect) is normal on UDP-style transport.
It does **not** automatically mean gameplay replication is broken.

Use this rule of thumb:

- `pending_rel` keeps climbing and never drains: reliable path is unhealthy.
- `pending_rel` drains to `0` and gameplay looks correct: occasional packet loss is being recovered as designed.

## 9. Reliable vs Unreliable Channels

Reliable and unreliable traffic are separate channel paths with separate queueing/ordering behavior.

- Reliable channels retry until acked.
- Unreliable sequenced traffic favors new state and may drop old state under loss/backpressure.

This is why high-rate transform updates are usually unreliable-sequenced while gameplay-critical actions (spawn/ownership/authoritative state changes) stay reliable.

## 10. Reliable Window Backpressure

SnAPI.Networking uses ACK-mask-based reliable tracking.
To keep a reliable message ackable, sender-side in-flight reliable depth is window-limited.

Behavior when pressure is high:

- new reliable payloads are queued/deferred,
- they are sent once earlier reliable payloads are acked,
- they are **not** silently dropped.

Result: under sustained overload you get higher latency before delivery, not silent data loss.

## 11. Reflection RPC (Optional but Powerful)

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

Use world-owned RPC bridge + service:

```cpp
auto* Session = WorldInstance.Networking().Session();
auto* RpcBridge = WorldInstance.Networking().RpcBridge();
auto Rpc = WorldInstance.Networking().Rpc();

if (!Session || !RpcBridge || !Rpc)
{
    return;
}
```

Call by reflected method name:

```cpp
std::array<Variant, 1> Args{Variant::FromValue(true)};
RpcBridge->Call(ConnectionHandle,
                *WeaponComponentPtr,
                "ServerSetFiring",
                Args);
```

`NetRpcBridge` resolves method metadata, serializes arguments via `ValueCodecRegistry`, and routes through SnAPI.Networking RPC.

## 12. Gameplay RPC Dispatch Pattern (`CallRPC`)

Most gameplay code should avoid touching `NetRpcBridge` directly.
Use `INode::CallRPC(...)` / `IComponent::CallRPC(...)` from your gameplay methods:

```cpp
void AudioSourceComponent::Play()
{
    if (CallRPC("PlayServer"))
    {
        return;
    }
    PlayClient();
}

void AudioSourceComponent::PlayServer()
{
    m_playRequested = true;
    if (CallRPC("PlayClient"))
    {
        return;
    }
    PlayClient();
}
```

`CallRPC(...)` behavior:

- resolves reflected methods by name + argument type list (including base types)
- routes using method flags:
    - `RpcNetServer`: server invokes locally, client forwards to server
    - `RpcNetClient`: client invokes locally, server forwards to client
    - `RpcNetMulticast`: server multicasts, clients invoke locally
- returns `true` when the call was handled (local invoke or queued network call)
- returns `false` when method/route cannot be resolved so caller can apply local fallback behavior

For components, `TypeKey` is assigned automatically during `Add<T>()` / `AddWithId<T>()`, so component call sites do not need to pass type ids manually.

## 13. Common Replication and RPC Problems

- Objects never replicate:
    - forgot `Replicated(true)` on node/component
    - forgot `EFieldFlagBits::Replication` on fields

- `CallRPC(...)` returns false unexpectedly:
    - method name/signature does not match reflected metadata
    - method exists but is missing RPC flags
    - client path has no active primary connection/session

- Initial spawn works, then updates stop:
    - inspect connection dumps for pending reliable growth/disconnect reasons
    - check pacing/MTU settings in `NetConfig`

- Type not found on remote:
    - ensure reflected type macro is linked and `RegisterBuiltinTypes()` ran

Next: [Audio Components](audio.md)
