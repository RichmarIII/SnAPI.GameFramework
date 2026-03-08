# Nodes and Components

Once the world hierarchy is clear, the next question is how to model gameplay objects.

The answer in this framework is still straightforward:

- derive a node when the thing needs identity, hierarchy, or a distinct semantic role
- attach components when you want reusable behavior or state

## What Belongs On A Node

A node is the right place for:

- a gameplay identity like `PawnBase`, `LocalPlayer`, `PlayerStart`, or `WorldRenderSettings`
- hierarchy ownership
- cross-system references that conceptually belong to the whole object
- high-level callbacks like possession or spawn semantics

## What Belongs On A Component

A component is the right place for:

- transform
- physics state
- audio playback
- camera state
- input adaptation
- script binding
- small reusable behaviors

## A Small Custom Node And Component

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

class TreasureNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "MyGame::TreasureNode";

    int Coins = 10;
    bool Claimed = false;
};

class SpinComponent final : public BaseComponent, public ComponentCRTP<SpinComponent>
{
public:
    static constexpr const char* kTypeName = "MyGame::SpinComponent";

    float DegreesPerSecond = 90.0f;

    void Tick(float DeltaSeconds)
    {
        BaseNode* OwnerNode = OwnerNode();
        if (!OwnerNode)
        {
            return;
        }

        auto TransformResult = OwnerNode->Component<TransformComponent>();
        if (!TransformResult)
        {
            return;
        }

        // Replace this with your actual quaternion update logic.
        (void)DeltaSeconds;
    }
};
```

The important part is structural:

- the node carries identity and game meaning
- the component carries a reusable behavior

## Reflect Them If You Need Engine Features

If a type needs serialization, editor property inspection, replication metadata, or reflected RPC, register it.

```cpp
SNAPI_REFLECT_TYPE(TreasureNode, (TTypeBuilder<TreasureNode>(TreasureNode::kTypeName)
    .Base<BaseNode>()
    .Field("Coins", &TreasureNode::Coins)
    .Field("Claimed", &TreasureNode::Claimed)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SpinComponent, (TTypeBuilder<SpinComponent>(SpinComponent::kTypeName)
    .Field("DegreesPerSecond", &SpinComponent::DegreesPerSecond)
    .Constructor<>()
    .Register()));
```

## Attaching Components

```cpp
auto TreasureHandle = WorldInstance.CreateNode<TreasureNode>("Treasure");
if (!TreasureHandle)
{
    return;
}

auto* Treasure = TreasureHandle->Borrowed();
if (!Treasure)
{
    return;
}

if (auto Transform = Treasure->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 1.0f, 0.0f);
}

if (auto Spin = Treasure->Add<SpinComponent>())
{
    Spin->DegreesPerSecond = 180.0f;
}
```

Useful helpers on `BaseNode`:

- `Add<T>()`
- `Component<T>()`
- `Has<T>()`
- `Remove<T>()`

These all route through world-owned storage.

## Lifecycle Hooks

Nodes and components can implement:

- `OnCreate`
- `OnDestroy`
- `PreTick`
- `Tick`
- `FixedTick`
- `LateTick`
- `PostTick`
- `EndFrame`
- editor-only hooks when built with editor support

Use them carefully.

### `OnCreate` is not a constructor substitute

`OnCreate` exists because the object needs to be world-owned and registered first.

That matters for:

- looking up the world
- adding sibling components
- interacting with networking or renderer state
- editor bootstrap deferral

### `OnCreate` can be deferred

This is especially important in the editor path.

If your node depends on renderer viewports or editor UI state, do not assume `OnCreate` runs at the instant the C++ object was allocated. The framework can intentionally defer it until the world is actually ready.

## Activity, Replication, and Destroy State

Every node and component has separate concepts for:

- active or inactive execution
- replicated or local-only behavior
- pending destroy

Those are not the same thing.

- `Active(false)` stops tick-style execution.
- `Replicated(true)` only opens the object-level replication gate.
- `PendingDestroy()` means the object has been scheduled for cleanup.

## Runtime Components

Classic components are not the only option.

If you want very dense runtime-only state, you can attach runtime ECS components through the runtime mirror.

Typical reasons to use runtime components:

- thousands of lightweight simulation records
- explicit tick-priority ordering
- data that does not need the full reflected component model

Do not reach for runtime components by default. Start with classic nodes/components first.

## Design Heuristics

Use these rules when deciding between node vs component:

1. If it needs its own handle and hierarchy identity, make it a node.
2. If it is reusable behavior, make it a component.
3. If it is world-scoped policy rather than object behavior, consider a world subsystem, gameplay service, or a special node like `WorldRenderSettings`.
4. If it needs fixed-step, high-density runtime processing, consider a runtime component.

## What To Read Next

- [Input System](input.md)
- [Physics System and Components](physics.md)
- [Reflection and Serialization](reflection_serialization.md)
