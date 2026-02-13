# Worlds and Graphs

This page explains the runtime object model. If this is clear, everything else (physics, serialization, replication, audio) becomes much easier.

## 1. Runtime Hierarchy

SnAPI.GameFramework is centered around node graphs:

- `World` is the runtime root.
- `Level` is a node type that also behaves like a graph.
- `NodeGraph` is a node that owns other nodes.
- `BaseNode` is a regular node.
- `IComponent` attaches behavior/state to a node.

Because `Level` and `NodeGraph` are nodes, graphs can be nested.

## 2. Handles vs Borrowed Pointers

You will use two access patterns constantly:

- `NodeHandle` / `ComponentHandle`: stable identity (UUID-based)
- `Borrowed()` pointer: quick lookup for immediate use

Important rule:

- Do not cache borrowed pointers long-term. Resolve from the handle when needed.

## 3. Create a World, Level, Graph, and Nodes

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

void BuildScene()
{
    RegisterBuiltinTypes();

    World WorldInstance("GameWorld");

    auto LevelHandleResult = WorldInstance.CreateLevel("MainLevel");
    if (!LevelHandleResult)
    {
        return;
    }

    auto LevelResult = WorldInstance.LevelRef(LevelHandleResult.value());
    if (!LevelResult)
    {
        return;
    }

    Level& MainLevel = *LevelResult;

    auto GraphHandleResult = MainLevel.CreateGraph("Gameplay");
    if (!GraphHandleResult)
    {
        return;
    }

    auto GraphResult = MainLevel.Graph(GraphHandleResult.value());
    if (!GraphResult)
    {
        return;
    }

    NodeGraph& Gameplay = *GraphResult;

    auto PlayerResult = Gameplay.CreateNode("Player");
    auto CameraResult = Gameplay.CreateNode("Camera");
    if (!PlayerResult || !CameraResult)
    {
        return;
    }

    // Parent-child relation: Camera becomes child of Player.
    (void)Gameplay.AttachChild(PlayerResult.value(), CameraResult.value());
}
```

## 4. Frame Lifecycle

Tick order is tree-driven from graph roots.
`World` also runs subsystem work during frame lifecycle:

- networking session pumping in `Tick` + `EndFrame`
- optional physics stepping in `Tick` and/or `FixedTick` (based on physics settings)
- audio system update in `Tick`

Typical frame loop:

```cpp
void RunFrame(World& WorldInstance, float DeltaSeconds)
{
    WorldInstance.Tick(DeltaSeconds);
    WorldInstance.FixedTick(DeltaSeconds); // optional if you use fixed-step logic
    WorldInstance.LateTick(DeltaSeconds);
    WorldInstance.EndFrame(); // processes deferred destruction
}
```

Why `EndFrame()` matters:

- `DestroyNode()` and component `Remove<T>()` are deferred.
- Handles remain valid until `EndFrame()`.
- This prevents mid-frame invalidation bugs.

If you prefer less boilerplate in apps/examples, use `GameRuntime` and call `Runtime.Update(DeltaSeconds)`.
`GameRuntime` orchestrates world lifecycle phases while `World` owns subsystem ticking/pumping.

## 5. Standalone Graphs (Prefab-Style)

Standalone graphs are valid data containers:

```cpp
NodeGraph PrefabGraph("EnemyPrefab");
auto EnemyRoot = PrefabGraph.CreateNode("EnemyRoot");
```

They can be serialized and reused, but they are not automatically pumped by a world loop unless you call tick methods yourself.

## 6. World Ownership Pointer (`INode::World()`)

Every node has `World()` access:

- In a world-owned tree, `World()` points to that `World`.
- In standalone/prefab graphs, `World()` may be `nullptr`.

This is how systems are accessed from gameplay code:

```cpp
auto* OwnerNode = SomeComponent.Owner().Borrowed();
if (OwnerNode && OwnerNode->World())
{
    // OwnerNode->World()->Audio(), OwnerNode->World()->Physics(), etc.
}
```

## 7. Common Mistakes

- Creating logic that assumes `World()` is always non-null.
- Forgetting `EndFrame()` and then wondering why removed objects still appear valid.
- Caching borrowed pointers across frame boundaries.

Next: [Nodes and Components](nodes_components.md)
