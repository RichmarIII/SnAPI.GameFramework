# SnAPI.GameFramework

## Overview

`SnAPI.GameFramework` is the gameplay/runtime layer that sits on top of the lower-level SnAPI subsystems.
It gives you one coherent model for:

- bootstrapping an application with `GameRuntime`
- owning simulation state inside a `World`
- organizing scene/gameplay objects as `BaseNode` instances
- attaching reusable behavior/data as `BaseComponent` instances
- integrating optional input, UI, networking, physics, audio, rendering, and scripting
- running the same core world model in runtime, editor, and Play-In-Editor flows

The framework is built around explicit ownership and stable identities:

- `GameRuntime` owns a `World`
- `World` owns nodes, components, dense runtime storage, and subsystem instances
- `NodeHandle` / `ComponentHandle` are the stable public identity boundary
- raw pointers are borrowed views, not ownership

## Conduit

`Conduit` is the reflection-driven visual scripting runtime for `SnAPI.GameFramework`.

It is built around:

- authored graph assets that compile into runtime plans
- authored class assets that bind graphs to reflected host node types
- compiled frame slots
- cached reflected field/method bindings
- control-flow primitives
- builtin logic/math intrinsics
- handle-family based instance resolution

Read [Conduit.md](/mnt/Dev/CodeProjects/SnAPI.GameFramework/Docs/GameFramework/Conduit.md) for the detailed runtime architecture and API model.
Read [ConduitEditor.md](/mnt/Dev/CodeProjects/SnAPI.GameFramework/Docs/GameFramework/ConduitEditor.md) for the authored graph editor architecture and document model.

## Authored Assets

`SnAPI.GameFramework` is moving toward a clear source-asset model:

- editor-authored assets derive from `IAsset`
- authored assets are saved as JSON source files
- authored assets are imported and cooked into final runtime payloads
- runtime `Load<>` and `Get<>` stay focused on cooked game-ready types

Read [AuthoredAssets.md](/mnt/Dev/CodeProjects/SnAPI.GameFramework/Docs/GameFramework/AuthoredAssets.md) for the authored source-asset architecture and pipeline model.
Read [AuthoredAssetRefactorPlan.md](/mnt/Dev/CodeProjects/SnAPI.GameFramework/Docs/GameFramework/AuthoredAssetRefactorPlan.md) for the execution plan and work order.

## Mental Model

### What is a `World`?

A `World` is the authoritative root of one gameplay/editor session.
It owns:

- node storage
- dense component storage
- subsystem adapters
- frame execution policy
- task dispatch and script runtime integration

If something belongs to the active session, it usually belongs to the world.

### What is a `Node`?

A node is the identity-bearing graph object you put into the world.
Nodes are good for:

- hierarchy
- scene ownership
- stable identity
- high-level gameplay objects such as levels, pawns, starts, settings nodes, and game-specific entities

Nodes derive from `BaseNode`.
They can have child nodes and attached components.

### What is a `Component`?

A component is an attachable unit of data or behavior that belongs to one node.
Components are good for:

- transform/state payloads
- rendering bindings
- physics participation
- input intent/state
- reusable gameplay logic

Components derive from `BaseComponent`.
They are owned by world-managed runtime storage, not by the caller.

### What is a `Handle`?

A handle is the stable reference you use across frames and systems.
Handles are:

- non-owning
- UUID-backed with an optional runtime-key cache
- safe to serialize/replicate/store
- able to resolve to borrowed pointers while the target is alive

Handles are not the same thing as raw pointers.
Use handles for durable references. Use raw pointers only as temporary borrowed views.

### What is `GameRuntime`?

`GameRuntime` is the application host.
It owns startup/shutdown and one-frame update orchestration.
Typical responsibilities:

- create the world
- initialize optional subsystems
- start gameplay orchestration
- tick fixed/variable/late/end-frame phases
- process runtime-level input/UI bridging

### What is `GameplayHost`?

`GameplayHost` is the high-level gameplay/session orchestrator that sits above raw node/component execution.
Use it for game/session concepts such as:

- game instance style setup
- game mode flow
- player/session joins
- pawn spawning/possession

The host does not replace the world.
It coordinates game-level policy on top of the world.

## Lifecycle Summary

Typical runtime lifecycle:

1. Build `GameRuntimeSettings`.
2. Call `GameRuntime::Init()`.
3. Access the created `World` if you need to spawn content or configure systems.
4. Repeatedly call `GameRuntime::Update(deltaSeconds)`.
5. Call `GameRuntime::Shutdown()` on exit.

Typical object lifecycle:

1. Create or deserialize a node.
2. Attach components.
3. World dispatches lifecycle callbacks such as `OnCreate`, `Tick`, and `OnDestroy`.
4. Destruction is typically flushed during `World::EndFrame()` to preserve frame-stable identity.

Runtime storage details that matter:

- nodes and components live in page-backed dense storages
- live objects keep a stable address until destroy
- handles keep the same public shape; the runtime index is decoded internally as page plus slot
- constructors/destructors should stay side-effect free, with world/backend work in `OnCreate()` / `OnDestroy()`

## Threading Rules

Default rule: assume graph mutation is main-thread only.

Use this module with the following expectations:

- `GameRuntime::Init()`, `Update()`, and `Shutdown()` are main-thread only.
- `World` graph mutation is main-thread only unless a specific API explicitly says otherwise.
- handles are cheap value types, but resolving them to borrowed pointers is not a free-threaded ownership model.
- if background work needs to feed results into the world, marshal it back through the world/task-dispatch path

## Hello GameFramework

```cpp
#include "GameFramework.hpp"

int main()
{
    using namespace SnAPI::GameFramework;

    GameRuntime Runtime{};
    GameRuntimeSettings Settings{};
    Settings.WorldName = "HelloGameFramework";
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;

    if (auto Init = Runtime.Init(Settings); !Init)
    {
        return 1;
    }

    World& WorldRef = Runtime.World();

    auto LevelHandle = WorldRef.CreateLevel("MainLevel");
    if (!LevelHandle)
    {
        Runtime.Shutdown();
        return 1;
    }

    auto PawnHandle = WorldRef.CreateNode<PawnBase>("PlayerPawn");
    if (PawnHandle)
    {
        if (BaseNode* Pawn = PawnHandle->Borrowed())
        {
            (void)Pawn->Add<TransformComponent>();
            (void)Pawn->Add<CameraComponent>();
        }
    }

    bool Running = true;
    while (Running)
    {
        Running = Runtime.Update(1.0f / 60.0f);
    }

    Runtime.Shutdown();
    return 0;
}
```

What this example shows:

- `GameRuntime` owns the session
- `World` owns created objects
- nodes are created through the world
- components are attached through the node
- the update loop is centralized in `GameRuntime::Update()`

## Glossary

### Borrowed pointer

A non-owning raw pointer returned by a handle, world lookup, or similar API.
It is only valid while the target object remains alive and registered.
Do not store it as a long-term reference.

### Stable identity

The UUID-backed identity used by handles, serialization, replication, and registry lookup.
Stable identity outlives any particular raw pointer value.

### Dense runtime storage

The per-type page-backed ECS storage used for nodes and components. It gives hot traversal and stable addresses until destroy.

### Execution profile

A `WorldExecutionProfile` value that decides which phases and subsystems are active for the current world mode.

### PIE

Play-In-Editor. A world mode that uses runtime-like simulation behavior while still being hosted by editor tooling.

## Key Headers

- `include/GameFramework.hpp`
  Umbrella include and Doxygen module entry point.
- `include/GameRuntime.h`
  Runtime host and bootstrap/update policy.
- `include/IWorld.h`
  Abstract world contract used by runtime, editor, and gameplay systems.
- `include/World.h`
  Concrete world implementation and execution profile.
- `include/BaseNode.h`
  Base node type for graph objects.
- `include/BaseComponent.h`
  Base component type for attachable behavior/data.
- `include/Handle.h`
  Stable non-owning typed handles.
- `include/WorldEcsRuntime.h`
  Dense per-type storage, phase scheduling, and page-size policy hooks.
- `include/Handles.h`
  Common aliases such as `NodeHandle` and `ComponentHandle`.
- `include/GameplayHost.h`
  High-level gameplay/session orchestration.
- `include/Conduit.h`
  Umbrella include for the Conduit visual scripting runtime.
- `include/Serialization.h`
  Reflection-driven serialization and reconstruction paths.

## Recommended Reading Order

1. `include/GameFramework.hpp`
2. `include/GameRuntime.h`
3. `include/IWorld.h`
4. `include/World.h`
5. `include/Handle.h`
6. `include/BaseNode.h`
7. `include/BaseComponent.h`
8. higher-level gameplay and subsystem headers as needed
