# Tutorials

This guide is written for a complete beginner who wants to understand how SnAPI.GameFramework actually works in code, not just copy snippets.

You will build a mental model in this order:

1. Runtime structure (`World -> Level -> NodeGraph -> Node -> Component`)
2. Renderer ownership and render-object flow
3. Physics ownership and simulation flow
4. Reflection registration (how types become visible to the engine)
5. Serialization and asset packaging
6. Networking replication + RPC
7. Audio components
8. Testing and validation

If you work through the pages in sequence, you will be able to read the examples in `examples/` and understand why each system is there.

## Before You Start

You should be comfortable with:

- C++ classes, inheritance, and templates
- CMake build basics
- Running binaries from the terminal

Build once before reading the tutorials so you can run examples as you go:

```bash
cmake -S . -B build/debug \
  -DSNAPI_GF_BUILD_TESTS=ON \
  -DSNAPI_GF_BUILD_EXAMPLES=ON
cmake --build build/debug
```

Run tests:

```bash
ctest --test-dir build/debug --output-on-failure
```

Run examples:

```bash
./build/debug/examples/FeatureShowcase/FeatureShowcase
./build/debug/examples/WorldPerfBenchmark/WorldPerfBenchmark
./build/debug/examples/MultiplayerExample/MultiplayerExample --server
./build/debug/examples/MultiplayerExample/MultiplayerExample --client
```

## Recommended Learning Path

1. [Worlds and Graphs](tutorials/worlds_graphs.md)
2. [Nodes and Components](tutorials/nodes_components.md)
3. [Renderer Integration and Mesh Components](tutorials/renderer.md)
4. [Physics System and Components](tutorials/physics.md)
5. [Physics Queries and Events](tutorials/physics_queries_events.md)
6. [Reflection and Serialization](tutorials/reflection_serialization.md)
7. [AssetPipeline Integration](tutorials/assetpipeline.md)
8. [Networking Replication and RPC](tutorials/networking.md)
9. [Audio Components](tutorials/audio.md)
10. [Testing and Validation](tutorials/testing.md)

## Core Ideas You Should Keep In Mind

- Handles are identity; pointers are borrowed views.
- Reflection metadata is lazy-registered on first use.
- Serialization and networking both read reflection metadata.
- Gameplay RPC call sites can stay small with `INode::CallRPC(...)` / `IComponent::CallRPC(...)`.
- Replication has two gates:
    - The field must have `EFieldFlagBits::Replication`.
    - The owning node/component must have `Replicated(true)`.
- Nested field replication depends on codec availability:
    - with `TValueCodec<T>`, the full field value is codec-serialized.
    - without a codec, only nested reflected fields marked for replication are serialized.
- `World` owns subsystem runtime (networking/audio/physics/renderer), and `GameRuntime` orchestrates world update phases.
- Renderer follows the same world-owned subsystem pattern (`World::Renderer()`), and render submit/present is driven by `World::EndFrame()`.
- Mesh assets are data-only; per-instance render behavior lives on render objects (`MeshRenderObject` via `IRenderObject`).
- Physics stepping can be configured:
    - fixed tick (`World::FixedTick`) for deterministic gameplay behavior.
    - variable tick (`World::Tick`) for simple real-time simulation paths.
- `RigidBodyComponent` sync direction depends on body type:
    - dynamic bodies usually pull transforms from physics.
    - static/kinematic bodies usually push transforms to physics.
- Transport `pkt_lost` counters can be non-zero while gameplay remains correct; reliable backlog health is tracked with `pending_rel`.
- Destruction is deferred to `EndFrame()` to keep handles stable during a frame.
- `CameraComponent`, `StaticMeshComponent`, and `SkeletalMeshComponent` are only available when renderer integration is compiled in.

With that baseline, start with `Worlds and Graphs`.

## Renderer Refactor Context (2026-02-15)

Renderer docs/tutorials here track the post-refactor model where `Mesh` is asset data and runtime instance state is represented by `IRenderObject`/`MeshRenderObject`.
