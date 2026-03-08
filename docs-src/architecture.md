# Architecture

This page describes the framework as it exists now.

If you remember the older generated docs, reset your mental model first:

- There is no public `NodeGraph`-centric runtime path in the current beginner-facing architecture.
- `World` owns the hierarchy.
- `Level` is a node that provides level-style creation and attachment helpers on top of world ownership.
- `GameRuntime` is the normal application shell.

## Mental Model

The shortest accurate description is:

```text
GameRuntime
  -> World
    -> root Level nodes and root BaseNode nodes
      -> child BaseNode / Level nodes
        -> BaseComponent attachments
    -> optional subsystems (input, UI, networking, physics, audio, renderer)
    -> script runtime
    -> runtime ECS mirror (WorldEcsRuntime)
```

What each layer means:

- `GameRuntime` is the host for startup, update, shutdown, fixed-step accumulation, optional platform input forwarding, and optional gameplay host lifetime.
- `World` is the authoritative owner of gameplay objects and subsystems.
- `Level` is a convenience grouping node. Creating a child `Level` is the current way to partition a scene into authored regions or logical chunks.
- `BaseNode` is the stable gameplay object with identity, hierarchy, and component access.
- `BaseComponent` adds behavior or data to a node without changing the node type.

## Ownership And Lifetime

The rules that matter most are simple:

- The world owns nodes.
- The world also owns classic components and runtime ECS components.
- Handles are stable public identity.
- Borrowed pointers are temporary views. Do not store them across destructive operations or subsystem shutdown.
- Destruction is usually deferred to `World::EndFrame()`, which is how handle stability is preserved during a frame.

The practical consequence is that this style is correct:

```cpp
NodeHandle PlayerHandle = *WorldInstance.CreateNode<BaseNode>("Player");
BaseNode* Player = PlayerHandle.Borrowed();
```

This style is not safe as a long-term ownership model:

```cpp
BaseNode* CachedPointer = PlayerHandle.Borrowed();
// ... many frames later after destroy/recreate/shutdown assumptions ...
```

## Initialization Order

`GameRuntime::Init()` creates and initializes the session in this order:

1. optional builtin type registration
2. world creation
3. input subsystem
4. UI subsystem
5. physics subsystem
6. networking subsystem
7. renderer subsystem
8. gameplay host

That order matters. The gameplay host starts last during bootstrap so it can rely on the other world-owned subsystems already existing.

Source of truth: `src/GameRuntime.cpp`.

## Per-Frame Execution Order

`GameRuntime::Update(DeltaSeconds)` currently runs in this order:

1. gameplay host tick, if configured and enabled
2. fixed-tick accumulator loop (`World::FixedTick` zero or more times)
3. variable tick (`World::Tick`)
4. late tick (`World::LateTick`) when enabled
5. end-of-frame (`World::EndFrame`) when enabled
6. optional platform/UI input forwarding and close-request handling
7. optional frame pacing

Inside `World::Tick`, the order is:

1. queued task execution
2. script hot-reload tick
3. input pump
4. UI tick
5. networking session pump
6. runtime ECS gameplay tick or editor tick
7. physics variable step, when configured
8. audio update

Inside `World::EndFrame`, the important work is:

1. queued task execution
2. networking queued-task flush
3. deferred node destruction and runtime-node teardown
4. UI packet generation for viewport-bound contexts
5. renderer end-of-frame submission

## Editor Bootstrap And Deferred `OnCreate`

One of the major behavioral changes since older docs were generated is editor bootstrap safety.

The editor no longer assumes node `OnCreate()` work can run immediately while the UI viewport and render path are still being built. Instead, bootstrap can:

- defer node `OnCreate()` callbacks
- suppress component `OnCreate()` delivery
- let editor services construct layout and create `UIRenderViewport`
- let the viewport lazily create its actual render viewport and pass graph during layout/paint
- flush the deferred callbacks after the viewport path is ready

This is the reason render-facing nodes such as `WorldRenderSettings` and similar initialization code are now safer in editor startup than they were under the old docs.

## Runtime ECS Mirror

There are two layers users should know about:

- the user-facing node/component API (`BaseNode`, `BaseComponent`)
- the dense runtime ECS mirror (`WorldEcsRuntime`)

You normally work with the first layer.

The second layer exists for:

- dense storage
- tick-priority ordering
- runtime-only ECS components
- faster hot-path traversal and hierarchy mirroring

Useful facts:

- `BaseNode` exposes `AddRuntimeComponent<T>()`, `RuntimeComponent<T>()`, and related helpers.
- runtime component storages tick in ascending `kTickPriority`
- runtime node hierarchy mirrors the world hierarchy
- world execution profiles can disable gameplay ECS phases entirely, which is how editor worlds avoid running runtime gameplay logic

## Reflection, Serialization, Replication

These systems all meet in the type registry.

- Reflection metadata is registered lazily or through explicit registration.
- Serialization uses that metadata to walk fields and create components by reflected type.
- Replication uses field flags plus object replication gates.
- Reflected RPC uses method flags plus exact signature matching.

The current serializer surface is:

- `NodeSerializer`
- `LevelSerializer`
- `WorldSerializer`
- `SerializeNodePayload` / `DeserializeNodePayload`
- `SerializeLevelPayload` / `DeserializeLevelPayload`
- `SerializeWorldPayload` / `DeserializeWorldPayload`

That replaces the old `NodeGraphSerializer` framing from the earlier docs.

## Networking Model

There are two different levels of networking API:

### Session-level flow

Use:

- `NetworkSystem`
- `GameplayHost`
- `LocalPlayer`
- `IGame`, `IGameMode`, `IGameService`

This covers things like:

- hosting or joining a session
- joining or leaving players
- loading or unloading levels
- possession defaults
- server-authoritative request policy

### Object-level flow

Use:

- object replication flags on nodes/components/fields
- `CallRPC(...)` on nodes and components
- `NetReplicationBridge`
- `NetRpcBridge`

This covers things like:

- spawn/update/despawn of reflected state
- node/component reflected RPC methods
- object-scoped gameplay messages

## Renderer And UI Viewports

The renderer and UI are intentionally coupled only at explicit points.

- `RendererSystem` owns renderer bootstrap, viewport objects, pass-graph registration, and end-frame submission.
- `UISystem` owns one root `UIContext` plus optional child contexts.
- `GameRuntime::BindViewportWithUI()` and `UISystem::BindViewportContext()` create the mapping between a renderer viewport and a UI context.
- `UIRenderViewport` is the special UI element that lazily creates and maintains a renderer viewport from layout.

This means viewport existence is layout-driven, not guaranteed at constructor time.

## Practical Design Rules

If you are adding new gameplay code, these rules keep you out of most trouble:

1. Put session logic in `GameplayHost`, `IGame`, `IGameMode`, or services. Do not fake session orchestration inside random nodes.
2. Put world-owned state on nodes/components. Do not invent parallel ownership trees.
3. Use child `Level` nodes for content partitions.
4. Treat borrowed pointers as frame-local.
5. Use `EndFrame()` in manual world loops, or just use `GameRuntime` so you do not forget it.
6. For editor-facing render setup, assume viewport creation is lazy and rely on the framework's deferred bootstrap behavior rather than forcing eager initialization from node constructors.

Continue to [Start Here](tutorials.md) if you want the guided path.
