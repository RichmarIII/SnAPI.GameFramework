# Bouncy Basement

This tutorial is a playground for rigid bodies, colliders, and event flow.

The premise is simple: build a basement full of bouncing crates and a player pawn that can shove them around.

## What You Will Learn

- how `ColliderComponent` and `RigidBodyComponent` cooperate
- why dynamic bodies pull transforms from physics
- how to tune restitution and friction
- how to listen for physics events

## 1. Make The Room

Create floor and wall nodes with static rigid bodies.

For each wall:

- add `TransformComponent`
- add `ColliderComponent` with box shape
- add `RigidBodyComponent` with `BodyType = Static`
- optionally add `StaticMeshComponent` with `primitive://box`

The repeated pattern matters more than the art.

## 2. Spawn Bouncy Crates

```cpp
auto CrateHandle = Runtime.World().CreateNode<BaseNode>("Crate");
auto* Crate = CrateHandle ? CrateHandle->Borrowed() : nullptr;
if (!Crate)
{
    return;
}

if (auto Transform = Crate->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 5.0f, 0.0f);
}

if (auto Collider = Crate->Add<ColliderComponent>())
{
    auto& C = Collider->EditSettings();
    C.Shape = SnAPI::Physics::EShapeType::Box;
    C.HalfExtent = Vec3(0.5f, 0.5f, 0.5f);
    C.Restitution = 0.8f;
    C.Friction = 0.2f;
}

if (auto Body = Crate->Add<RigidBodyComponent>())
{
    auto& B = Body->EditSettings();
    B.BodyType = SnAPI::Physics::EBodyType::Dynamic;
    B.Mass = 1.0f;
    B.EnableCcd = true;
}
```

High restitution and lower friction are what make the basement fun instead of dull.

## 3. Give The Player A Push Mechanic

Use a pawn with:

- `InputIntentComponent`
- `InputComponent`
- `CharacterMovementController`
- `RigidBodyComponent`
- `ColliderComponent`

Then add a gameplay action that calls `ApplyForce(...)` on nearby crates.

That demonstrates two different control styles in the same level:

- continuous movement through the controller
- one-shot impulses through rigid body commands

## 4. Add Event Logging

Register a general physics event listener and print contact or trigger events.

The exact event payload depends on the backend event type, but the framework-side pattern is consistent:

```cpp
auto Token = Runtime.World().Physics().AddEventListener(
    [](const SnAPI::Physics::PhysicsEvent& Event)
    {
        (void)Event;
    });
```

This is a good place to learn the difference between:

- simulation state
- drained event queues
- listener callbacks

## 5. Add A Trigger Zone

Set a collider's `IsTrigger = true` and use the resulting events to:

- open a door
- start a sound
- spawn extra crates

That keeps the tutorial playful while still teaching the actual collider settings surface.

## 6. Extensions

1. Add sleep/wake debug colors to crates.
2. Teleport crates back to shelves with `Teleport(...)`.
3. Rebase the world with floating origin enabled and confirm the basement still behaves.

Continue with [Physics Queries and Events](physics_queries_events.md).
