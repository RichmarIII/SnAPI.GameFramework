# Tutorials

This guide is written for a complete beginner who wants to understand how SnAPI.GameFramework actually works in code, not just copy snippets.

You will build a mental model in this order:

1. Runtime structure (`World -> Level -> NodeGraph -> Node -> Component`)
2. Physics ownership and simulation flow
3. Reflection registration (how types become visible to the engine)
4. Serialization and asset packaging
5. Networking replication + RPC
6. Audio components
7. Testing and validation

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
3. [Physics System and Components](tutorials/physics.md)
4. [Physics Queries and Events](tutorials/physics_queries_events.md)
5. [Reflection and Serialization](tutorials/reflection_serialization.md)
6. [AssetPipeline Integration](tutorials/assetpipeline.md)
7. [Networking Replication and RPC](tutorials/networking.md)
8. [Audio Components](tutorials/audio.md)
9. [Testing and Validation](tutorials/testing.md)

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
- `World` owns subsystem runtime (networking/audio/physics), and `GameRuntime` orchestrates world update phases.
- Physics stepping can be configured:
    - fixed tick (`World::FixedTick`) for deterministic gameplay behavior.
    - variable tick (`World::Tick`) for simple real-time simulation paths.
- `RigidBodyComponent` sync direction depends on body type:
    - dynamic bodies usually pull transforms from physics.
    - static/kinematic bodies usually push transforms to physics.
- Transport `pkt_lost` counters can be non-zero while gameplay remains correct; reliable backlog health is tracked with `pending_rel`.
- Destruction is deferred to `EndFrame()` to keep handles stable during a frame.

With that baseline, start with `Worlds and Graphs`.

## Checkpoint Context (2026-02-15)

These docs describe the pre-refactor baseline across SnAPI modules.

- Behavior and examples here align with the checkpoint commit taken before the renderer-side `IRenderObject` refactor.
- If you are validating post-refactor behavior, compare against newer docs/commits after this date.
