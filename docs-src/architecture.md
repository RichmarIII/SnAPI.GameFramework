# Architecture

SnAPI.GameFramework is built around three main axes:

1. **Runtime hierarchy**: `World -> Level -> NodeGraph -> BaseNode`
2. **Type metadata**: `TypeRegistry`, `TTypeBuilder`, and cached `TypeId`
3. **Data transport**: reflection serialization, asset payloads, and network replication

## Runtime Model

- `World` is the runtime root and owns subsystems (jobs, audio, networking bridges).
- `World` is also responsible for subsystem frame work (networking pump + audio update).
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

## Audio Model

- World-level `AudioSystem` owns runtime backend state.
- `World::Tick(...)` performs audio system frame update.
- `AudioSourceComponent` and `AudioListenerComponent` bridge node state to audio playback/listening.
