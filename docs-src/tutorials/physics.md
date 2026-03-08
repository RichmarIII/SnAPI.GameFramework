# Physics System and Components

Physics is also world-owned.

The important pieces are:

- `PhysicsSystem` owns the scene
- `ColliderComponent` stores passive shape/filter/material data
- `RigidBodyComponent` owns the backend body handle
- `CharacterMovementController` drives movement on top of a sibling rigid body

## 1. Bootstrap Physics

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "PhysicsWorld";
Settings.Tick.EnableFixedTick = true;
Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;

GameRuntimePhysicsSettings Physics{};
Physics.TickInFixedTick = true;
Physics.TickInVariableTick = false;
Physics.EnableFloatingOrigin = true;
Physics.AutoRebaseFloatingOrigin = true;
Physics.FloatingOriginRebaseDistance = 512.0;
Settings.Physics = Physics;

if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

If fixed tick is enabled, physics usually belongs there. That is the default gameplay-safe path.

## 2. Add A Collider And Rigid Body

```cpp
auto BoxHandle = Runtime.World().CreateNode<BaseNode>("Crate");
if (!BoxHandle)
{
    return;
}

auto* Box = BoxHandle->Borrowed();
if (!Box)
{
    return;
}

if (auto Transform = Box->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 4.0f, 0.0f);
}

if (auto Collider = Box->Add<ColliderComponent>())
{
    auto& ColliderSettings = Collider->EditSettings();
    ColliderSettings.Shape = SnAPI::Physics::EShapeType::Box;
    ColliderSettings.HalfExtent = Vec3(0.5f, 0.5f, 0.5f);
    ColliderSettings.Friction = 0.5f;
    ColliderSettings.Restitution = 0.1f;
}

if (auto Body = Box->Add<RigidBodyComponent>())
{
    auto& BodySettings = Body->EditSettings();
    BodySettings.BodyType = SnAPI::Physics::EBodyType::Dynamic;
    BodySettings.Mass = 1.0f;
    BodySettings.EnableCcd = true;
}
```

### Division of responsibility

- `ColliderComponent` does not create a backend body by itself
- `RigidBodyComponent` reads the collider settings and creates the body
- if there is no collider, `RigidBodyComponent` falls back to a default box shape

## 3. Know The Sync Direction

This is one of the most important rules in the physics layer.

### Dynamic bodies

Dynamic bodies usually pull transforms from physics back into the owning node.

### Static and kinematic bodies

Static and kinematic bodies usually push the owning node transform into physics.

That is why `RigidBodyComponent` is more than a dumb handle wrapper. It owns synchronization policy.

## 4. Move A Character With `CharacterMovementController`

`CharacterMovementController` is a fixed-step locomotion helper that sits on top of a sibling rigid body.

Example setup:

```cpp
auto PawnHandle = Runtime.World().CreateNode<PawnBase>("Runner");
if (!PawnHandle)
{
    return;
}

auto* Pawn = PawnHandle->Borrowed();
if (!Pawn)
{
    return;
}

(void)Pawn->Add<TransformComponent>();
(void)Pawn->Add<ColliderComponent>();
(void)Pawn->Add<RigidBodyComponent>();
(void)Pawn->Add<InputIntentComponent>();

if (auto Movement = Pawn->Add<CharacterMovementController>())
{
    auto& MoveSettings = Movement->EditSettings();
    MoveSettings.MoveSpeed = 6.0f;
    MoveSettings.JumpImpulse = 5.0f;
    MoveSettings.KeepUpright = true;
}
```

The controller:

- reads movement input
- resolves grounded state through a downward probe
- applies horizontal velocity directly
- buffers jump input briefly to tolerate frame/tick mismatch

## 5. Fixed Tick Is The Right Place For Movement

`CharacterMovementController` only performs meaningful work in `FixedTick()`.

That is by design.

If you want deterministic-feeling movement, keep:

- fixed tick enabled
- physics stepping in fixed tick
- movement control in fixed tick

## 6. Teleports, Forces, and Velocity

`RigidBodyComponent` exposes imperative helpers:

- `ApplyForce(...)`
- `SetVelocity(...)`
- `Teleport(...)`

Use them for game actions that do not belong inside continuous controller logic.

## 7. Sleep And Activity

`RigidBodyComponent` can respond to body sleep and wake events and optionally deactivate the component when the body sleeps.

That is useful for:

- large scenes full of resting bodies
- keeping tick work down when nothing is moving

## 8. Floating Origin

The physics system owns the world-space to physics-local conversion policy when floating origin is enabled.

That means:

- node/gameplay code should stay in world coordinates
- the physics system handles conversion and rebasing
- components like `RigidBodyComponent` rely on that system so coordinates stay consistent

## What To Read Next

- [Physics Queries and Events](physics_queries_events.md)
- [Bouncy Basement](bouncy_basement.md)
