---
title: SnAPI.GameFramework
---

<div class="snapi-hero">
  <div>
    <p class="snapi-kicker">Gameplay framework for C++23</p>
    <h1>SnAPI.GameFramework</h1>
    <p>
      A data-driven game framework centered around Node graphs, reflection metadata,
      serialization pipelines, and network-aware gameplay systems.
      It integrates directly with SnAPI.AssetPipeline, SnAPI.Networking, SnAPI.Audio, and SnAPI.Physics.
    </p>
    <div class="snapi-actions">
      <a class="md-button md-button--primary" href="tutorials/">Get Started</a>
      <a class="md-button" href="api/">API Reference</a>
      <a class="md-button" href="architecture/">Architecture</a>
    </div>
    <div class="snapi-badges">
      <span class="snapi-badge">Node Graphs</span>
      <span class="snapi-badge">Reflection</span>
      <span class="snapi-badge">Serialization</span>
      <span class="snapi-badge">Replication + RPC</span>
      <span class="snapi-badge">Physics Simulation</span>
      <span class="snapi-badge">Audio Components</span>
    </div>
    <div class="snapi-hero__features">
      <strong>What you get</strong>
      <ul>
        <li>World/Level/NodeGraph runtime architecture with clean hierarchy and tick orchestration.</li>
        <li>Standalone graph support for prefab-style authored content that can be serialized and reused.</li>
        <li>UUID-first identity model with stable handles and safe end-of-frame destruction semantics.</li>
        <li>Data-driven node/component composition with minimal boilerplate and strong runtime ergonomics.</li>
        <li>Lazy, register-on-first-use reflection that avoids giant startup registration lists.</li>
        <li>Type-safe reflection metadata for fields, methods, constructors, and inheritance chains.</li>
        <li>Field and method flags for replication and network RPC intent directly in reflected metadata.</li>
        <li>Ergonomic gameplay RPC dispatch with `INode::CallRPC(...)` / `IComponent::CallRPC(...)` role-aware helpers.</li>
        <li>Reflection-powered graph, level, and world serialization with schema-versioned payloads.</li>
        <li>Custom value codec extension points (`TValueCodec<T>`) for packed/high-performance data formats.</li>
        <li>World-owned `PhysicsSystem` adapter over SnAPI.Physics with backend routing and coupling support.</li>
        <li>Built-in `ColliderComponent`, `RigidBodyComponent`, and `CharacterMovementController` for fast gameplay iteration.</li>
        <li>Configurable physics stepping policy (fixed tick, variable tick, or fully manual step) per world runtime settings.</li>
        <li>Direct access to query/event APIs (`Raycast`, `Sweep`, `Overlap`, collision/trigger events) through world physics scene.</li>
        <li>First-class SnAPI.AssetPipeline integration for cooking, packing, mounting, and runtime loading.</li>
        <li>SnAPI.Networking bridges for auto-spawned replicated nodes/components and reflection-driven RPC.</li>
        <li>World-owned subsystem pattern (`Owner()->World()->...`) for scalable engine module integration.</li>
        <li>Integrated SnAPI.Audio listener/source components with transform-driven 3D spatial updates.</li>
        <li>Catch2 test coverage across handles, reflection, serialization, relevance, and replication behavior.</li>
      </ul>
    </div>
  </div>
</div>

## Quick Start

```bash
cmake -S . -B build/debug -DSNAPI_GF_BUILD_TESTS=ON -DSNAPI_GF_BUILD_DOCS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

## Learn The System

<div class="snapi-grid">
  <a class="snapi-card" href="tutorials/worlds_graphs/">
    <h3>Worlds and Graphs</h3>
    <p>How World, Level, NodeGraph, and BaseNode fit together.</p>
  </a>
  <a class="snapi-card" href="tutorials/nodes_components/">
    <h3>Nodes and Components</h3>
    <p>Create gameplay objects, add components, and drive lifecycle events.</p>
  </a>
  <a class="snapi-card" href="tutorials/reflection_serialization/">
    <h3>Reflection and Serialization</h3>
    <p>Use TTypeBuilder metadata and reflection-driven serializers.</p>
  </a>
  <a class="snapi-card" href="tutorials/assetpipeline/">
    <h3>AssetPipeline Integration</h3>
    <p>Save/load NodeGraph, Level, and World payloads as assets.</p>
  </a>
  <a class="snapi-card" href="tutorials/networking/">
    <h3>Networking</h3>
    <p>Replicate nodes/components and invoke reflection RPC methods.</p>
  </a>
  <a class="snapi-card" href="tutorials/physics/">
    <h3>Physics Components</h3>
    <p>Configure world physics, rigid bodies, colliders, and character movement.</p>
  </a>
  <a class="snapi-card" href="tutorials/physics_queries_events/">
    <h3>Physics Queries and Events</h3>
    <p>Run raycasts/overlaps, drain events, and tune backend routing/couplings.</p>
  </a>
  <a class="snapi-card" href="tutorials/audio/">
    <h3>Audio Components</h3>
    <p>Attach listeners and sources powered by SnAPI.Audio.</p>
  </a>
</div>

## Checkpoint Context (2026-02-15)

These docs describe the pre-refactor baseline across SnAPI modules.

- Behavior and examples here align with the checkpoint commit taken before the renderer-side `IRenderObject` refactor.
- If you are validating post-refactor behavior, compare against newer docs/commits after this date.
