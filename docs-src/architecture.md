# Architecture

SnAPI.GameFramework is built around four main axes:

1. **Runtime hierarchy**: `World -> Level -> NodeGraph -> BaseNode`
2. **Type metadata**: `TypeRegistry`, `TTypeBuilder`, and cached `TypeId`
3. **Data transport**: reflection serialization, asset payloads, and network replication
4. **World-owned simulation systems**: networking, audio, and physics adapters

## Runtime Model

- `World` is the runtime root and owns subsystems (jobs, audio, networking bridges, physics system).
- `World` is also responsible for subsystem frame work (networking pump + audio update + optional physics step).
- `Level` and `NodeGraph` are nodes, so graphs can be nested.
- `BaseNode` owns hierarchy relationships and component type bookkeeping.
- `IComponent` adds behavior and state to nodes without changing node types.

## Reflection Model

- `TypeId` is deterministic and generated from stable type names.
- `SNAPI_REFLECT_TYPE(Type, ...)` installs lazy registration hooks.
- Metadata includes fields, methods, constructors, and inheritance chain.
- `TypeRegistry::Find` auto-ensures missing reflected types on-demand.

## Serialization Model

- `ValueCodecRegistry` handles primitive/custom value codecs.
- `ComponentSerializationRegistry` creates/serializes components by `TypeId`.
- `NodeGraphSerializer`, `LevelSerializer`, and `WorldSerializer` convert runtime graphs to payloads.
- AssetPipeline serializers wrap these payloads into `snpak` assets.

## Networking Model

- Field replication is driven by reflection flags.
- RPC routing is driven by reflection method metadata.
- `INode::CallRPC(...)` and `IComponent::CallRPC(...)` provide gameplay-facing routing helpers over reflected RPC endpoints.
- `NetworkSystem` owns session/transport lifecycle (owner-only), then wires replication and RPC services.
- `NetReplicationBridge` and `NetRpcBridge` coordinate with SnAPI.Networking services.

## Physics Model

- `World` owns `PhysicsSystem`, which wraps `SnAPI::Physics::PhysicsRuntime` + one world scene.
- Physics bootstrap is configured through `GameRuntimeSettings::Physics` (`PhysicsBootstrapSettings`).
- Tick policy is configurable:
    - fixed stepping in `World::FixedTick(...)`
    - variable stepping in `World::Tick(...)`
    - or manual stepping by disabling both and calling `World::Physics().Step(...)`.
- Component adapters:
    - `ColliderComponent` stores shape/material/filter settings.
    - `RigidBodyComponent` creates/synchronizes backend bodies for owner nodes.
    - `CharacterMovementController` provides movement + grounded probe behavior using physics queries.
- Direct scene access is available (`World::Physics().Scene()`) for domain APIs:
    - rigid body domain (`Rigid()`)
    - query domain (`Query()`)
    - event draining (`DrainEvents(...)`).

## Audio Model

- World-level `AudioSystem` owns runtime backend state.
- `World::Tick(...)` performs audio system frame update.
- `AudioSourceComponent` and `AudioListenerComponent` bridge node state to audio playback/listening.
