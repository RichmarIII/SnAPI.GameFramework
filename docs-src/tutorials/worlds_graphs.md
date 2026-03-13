# Worlds, Levels, and Hierarchies

This is the most important tutorial in the docs because it fixes the biggest source of confusion from older material.

## The Current Hierarchy

The public beginner-facing hierarchy is:

- `GameRuntime` owns a `World`
- `World` owns nodes
- `Level` is a node used as a grouping root or partition
- `BaseNode` is the normal gameplay node type
- `BaseComponent` attaches behavior or data to a node

There is no separate `NodeGraph` layer you need to create first.

## `World` Is The Owner

The world owns:

- node lifetime
- component lifetime
- runtime hierarchy
- subsystem objects
- dense runtime scheduling/storage

That is why almost all creation APIs eventually forward into `World`, even when you call them through a `Level`.

## `Level` Is A Convenience Node

`Level` is not a second storage system. It is a `BaseNode`-derived type that forwards graph-style helper calls back into the world.

That gives you level-style authoring without splitting ownership.

Use it when you want:

- a root level in the world
- nested content partitions
- a clear parent for a group of related nodes

## Build A Tiny Hierarchy

```cpp
#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;

void BuildScene()
{
    RegisterBuiltinTypes();

    World WorldInstance("GameWorld");

    auto MainLevelHandle = WorldInstance.CreateLevel("MainLevel");
    if (!MainLevelHandle)
    {
        return;
    }

    auto* MainLevel = NodeCast<Level>(MainLevelHandle->Borrowed());
    if (!MainLevel)
    {
        return;
    }

    auto GameplayPartitionHandle = MainLevel->CreateNode<Level>("Gameplay");
    auto PropsPartitionHandle = MainLevel->CreateNode<Level>("Props");
    if (!GameplayPartitionHandle || !PropsPartitionHandle)
    {
        return;
    }

    auto* Gameplay = NodeCast<Level>(GameplayPartitionHandle->Borrowed());
    auto* Props = NodeCast<Level>(PropsPartitionHandle->Borrowed());
    if (!Gameplay || !Props)
    {
        return;
    }

    auto PlayerHandle = Gameplay->CreateNode<BaseNode>("Player");
    auto CameraHandle = Gameplay->CreateNode<BaseNode>("CameraBoom");
    auto LampHandle = Props->CreateNode<BaseNode>("Lamp");
    if (!PlayerHandle || !CameraHandle || !LampHandle)
    {
        return;
    }

    (void)WorldInstance.AttachChild(*PlayerHandle, *CameraHandle);
}
```

What happened here:

- the world created the root level
- the root level created two child `Level` partitions
- those partitions created regular child nodes
- the world remained the actual owner of every created object

## Handles vs Borrowed Pointers

You will use both constantly.

### Use handles for identity

- storing references in save data
- replication
- cross-frame references
- parent/child relationships

### Use borrowed pointers for immediate work

- changing a field right now
- calling `Add<T>()`
- reading state during this scope

Bad pattern:

- caching a borrowed pointer and assuming it survives destroy, reload, or shutdown

Good pattern:

- storing a `NodeHandle`
- resolving with `Borrowed()` when needed
- passing `NodeHandle&` in hot/runtime APIs when the call may need to rehydrate the runtime key

## Frame Order Matters

If you are manually driving a world, the full loop is:

```cpp
WorldInstance.FixedTick(FixedDeltaSeconds); // optional, possibly several times
WorldInstance.Tick(DeltaSeconds);
WorldInstance.LateTick(DeltaSeconds);
WorldInstance.EndFrame();
```

Most users should not hand-roll this. Use `GameRuntime` instead:

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "GameWorld";
Runtime.Init(Settings);

while (Runtime.Update(1.0f / 60.0f))
{
}
```

`GameRuntime` handles:

- fixed-step accumulation
- late tick
- end-frame
- optional platform/UI event forwarding
- frame pacing

## Destruction Is Usually Deferred

`DestroyNode()` schedules destruction. Actual removal typically happens in `World::EndFrame()`.

Why that design exists:

- traversal can continue safely through the frame
- handles stay meaningful until the flush point
- subsystems can release resources in a predictable phase

If you are testing manual world code and forget `EndFrame()`, destroyed nodes can appear to "stick around" longer than you expect.

## Dense Runtime Storage

`WorldEcsRuntime` is the world-owned dense storage/scheduling layer behind the normal node/component API.

Important consequences:

- concrete node/component types tick in dense per-type batches
- storage is page-backed, so creating one object does not relocate unrelated live objects
- handles still resolve through the same `Borrowed()` API, but hot paths should pass mutable handles when rehydration matters
- types can override `kStoragePageSize` if they need a larger or smaller page policy

## Common Mistakes

### Looking for `CreateGraph()`

That is old documentation. Use `CreateLevel()` or `CreateNode<Level>()` for partitions.

### Treating `Level` as a separate storage root

It is a convenience node over world ownership, not a second allocator or object registry.

### Forgetting `RegisterBuiltinTypes()` in ad hoc tools/tests

If reflection-driven features are involved, register builtins unless your host does it for you.

## What To Read Next

- [Nodes and Components](nodes_components.md)
- [First Play Session](first_play_session.md)
- [Architecture](../architecture.md)
