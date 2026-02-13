# Physics System and Components

This page is the beginner-to-practical guide for GameFramework physics integration.
By the end, you should understand:

1. how physics is owned and stepped in `World` / `GameRuntime`
2. how built-in physics components map onto runtime behavior
3. when to use forces, impulses, or direct velocity control
4. how to avoid the most common setup mistakes

## 1. Physics Architecture in GameFramework

At runtime, physics is world-owned:

- `World` owns one `PhysicsSystem`.
- `PhysicsSystem` owns one active `SnAPI::Physics::IPhysicsScene`.
- Nodes/components access physics through `Owner()->World()->Physics()`.

This follows the same subsystem pattern as networking and audio:

- no globals
- explicit lifetime
- one clear ownership path

## 2. Bootstrap Physics Through `GameRuntimeSettings`

If you use `GameRuntime` (recommended for apps/examples), physics is configured in `GameRuntimeSettings::Physics`.

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "PhysicsSandbox";
Settings.RegisterBuiltins = true;

Settings.Tick.EnableFixedTick = true;
Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
Settings.Tick.MaxFixedStepsPerUpdate = 4;

GameRuntimePhysicsSettings Physics{};
Physics.TickInFixedTick = true;
Physics.TickInVariableTick = false;

Physics.Scene.Gravity = SnAPI::Physics::Vec3{0.0f, -9.81f, 0.0f};
Physics.Scene.MaxBodies = 20000;
Physics.Scene.CollisionSteps = 1;

// Current default backend is Jolt for all domains.
Physics.Routing.RigidBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.QueryBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.CharacterBackend = SnAPI::Physics::EPhysicsBackend::Jolt;

Settings.Physics = Physics;

auto InitResult = Runtime.Init(Settings);
if (!InitResult)
{
    // handle init error
}
```

Key points:

- if `Settings.Physics` is `std::nullopt`, no world physics scene is created.
- if enabled, `GameRuntime::Init(...)` initializes world physics before networking.
- `GameRuntime::Shutdown()` shuts physics down cleanly.

## 3. Tick Policy: Fixed, Variable, or Manual

`PhysicsBootstrapSettings` gives two tick switches:

- `TickInFixedTick`
- `TickInVariableTick`

Recommended defaults:

- gameplay simulation: `TickInFixedTick = true`, `TickInVariableTick = false`
- quick sandbox/prototyping: variable stepping can be acceptable

Manual mode is also possible:

1. set both flags to `false`
2. call `World::Physics().Step(DeltaSeconds)` yourself

That is useful when you need custom simulation phases.

## 4. First Scene: Ground + Falling Box

This is the minimum complete setup.

```cpp
auto& WorldRef = Runtime.World();

// Ground node
auto GroundNodeResult = WorldRef.CreateNode<BaseNode>("Ground");
auto* Ground = GroundNodeResult ? GroundNodeResult->Borrowed() : nullptr;
if (!Ground)
{
    return;
}

auto GroundTransform = Ground->Add<TransformComponent>();
GroundTransform->Position = Vec3{0.0f, -1.0f, 0.0f};

auto GroundCollider = Ground->Add<ColliderComponent>();
GroundCollider->EditSettings().Shape = 1; // box
GroundCollider->EditSettings().HalfExtent = Vec3{30.0f, 1.0f, 30.0f};
GroundCollider->EditSettings().Layer = 1u;

auto GroundBody = Ground->Add<RigidBodyComponent>();
GroundBody->EditSettings().BodyType = 0; // static
GroundBody->RecreateBody();

// Falling box node
auto BoxNodeResult = WorldRef.CreateNode<BaseNode>("FallingBox");
auto* Box = BoxNodeResult ? BoxNodeResult->Borrowed() : nullptr;
if (!Box)
{
    return;
}

auto BoxTransform = Box->Add<TransformComponent>();
BoxTransform->Position = Vec3{0.0f, 6.0f, 0.0f};

auto BoxCollider = Box->Add<ColliderComponent>();
BoxCollider->EditSettings().Shape = 1; // box
BoxCollider->EditSettings().HalfExtent = Vec3{0.5f, 0.5f, 0.5f};

auto BoxBody = Box->Add<RigidBodyComponent>();
BoxBody->EditSettings().BodyType = 2; // dynamic
BoxBody->RecreateBody();
```

Why call `RecreateBody()`?

- `OnCreate()` creates a body immediately with current settings.
- if you change settings after `Add<RigidBodyComponent>()`, call `RecreateBody()` to apply them now.

## 5. `ColliderComponent::Settings` Explained

`ColliderComponent` is pure data that `RigidBodyComponent` consumes.

Core fields:

- `Shape`: `0=sphere`, `1=box`, `2=capsule`
- `HalfExtent`, `Radius`, `HalfHeight`: shape dimensions
- `LocalPosition`, `LocalRotation`: collider offset relative to node transform
- `Density`, `Friction`, `Restitution`: contact/material behavior
- `Layer`, `Mask`: broad filtering
- `IsTrigger`: overlap-only sensor behavior

Practical defaults:

- start with box colliders while gameplay logic is still changing
- keep layers/masks simple early, then tighten filtering later
- only enable trigger mode where needed (interaction volumes, checkpoints, etc.)

## 6. `RigidBodyComponent` Semantics

`RigidBodyComponent` maps node state to backend body state.

Body type mapping:

- `BodyType = 0`: static
- `BodyType = 1`: kinematic
- `BodyType = 2`: dynamic

Sync behavior in fixed tick:

- dynamic (`2`): component pulls transform from physics (`SyncFromPhysics`)
- static/kinematic (`0/1`): component pushes node transform into physics (`SyncToPhysics`)

Important implications:

- for dynamic actors, gameplay should usually read/write transform through physics-aware paths
- for kinematic actors, gameplay can author transform and let the component push it

## 7. Motion APIs: Force vs Impulse vs Velocity

`RigidBodyComponent` gives three common control paths:

```cpp
// Continuous force (accumulates through simulation integration)
Body->ApplyForce(Vec3{0.0f, 0.0f, 10.0f}, false);

// Instant impulse
Body->ApplyForce(Vec3{0.0f, 4.5f, 0.0f}, true);

// Explicit mode
Body->ApplyForce(Vec3{1.0f, 0.0f, 0.0f}, SnAPI::Physics::EForceMode::VelocityChange);

// Direct velocity set
Body->SetVelocity(Vec3{3.0f, 0.0f, 0.0f});
```

When to use what:

- force:
    - physically-plausible acceleration over time
    - good for thrusters, wind-like effects
- impulse:
    - one-shot kicks (jump, explosion, hit reactions)
- velocity set:
    - deterministic controller behavior (character steering, authoritative corrections)

## 8. Character Movement Controller

`CharacterMovementController` is a gameplay convenience component that expects:

- sibling `RigidBodyComponent`
- usually sibling `ColliderComponent`
- usually sibling `TransformComponent`

Setup:

```cpp
auto Player = WorldRef.CreateNode<BaseNode>("Player");
auto* PlayerNode = Player ? Player->Borrowed() : nullptr;
if (!PlayerNode)
{
    return;
}

auto T = PlayerNode->Add<TransformComponent>();
T->Position = Vec3{0.0f, 1.0f, 0.0f};

auto C = PlayerNode->Add<ColliderComponent>();
C->EditSettings().Shape = 1;
C->EditSettings().HalfExtent = Vec3{0.4f, 0.9f, 0.4f};
C->EditSettings().Layer = 2u;

auto B = PlayerNode->Add<RigidBodyComponent>();
B->EditSettings().BodyType = 2;
B->EditSettings().Mass = 70.0f;
B->RecreateBody();

auto M = PlayerNode->Add<CharacterMovementController>();
M->EditSettings().MoveForce = 60.0f;
M->SetMoveInput(Vec3{1.0f, 0.0f, 0.0f});
```

Runtime control:

```cpp
M->SetMoveInput(Vec3{InputX, 0.0f, InputZ});
if (WantsJump)
{
    M->Jump();
}
```

Controller behavior:

- applies horizontal movement through rigid body velocity control
- estimates vertical velocity from transform deltas so steering does not kill falling/rising behavior
- performs a downward ray probe (`GroundProbeDistance`) to evaluate grounded state

## 9. Replication Notes for Physics Components

Physics components are reflected, so they can participate in replication.
But keep this detail in mind:

- `ColliderComponent` and `RigidBodyComponent` expose `Settings` as replication-visible fields.
- nested setting members are currently not individually marked with `EFieldFlagBits::Replication`.

That means:

- if no `TValueCodec<Settings>` is provided, nested-field replication depends on nested flags.
- if you want full-struct replication, add a `TValueCodec<YourSettingsType>` or flag nested members explicitly.

Also remember global replication gates still apply:

- `Node->Replicated(true)`
- `Component->Replicated(true)`

## 10. Common Setup Mistakes

- Physics scene never starts:
    - forgot to set `Settings.Physics` before `Runtime.Init(...)`
- Bodies do not use updated settings:
    - edited settings after add, but forgot `RecreateBody()`
- Object never falls:
    - body type is static/kinematic instead of dynamic
    - no physics stepping configured
- Character cannot jump:
    - grounded probe mask/layer excludes valid ground
    - no collider/rigid body present on player
- Transform flicker:
    - two systems fighting transform writes
    - clarify whether object is physics-driven or script-driven

## 11. What to Read Next

- [Physics Queries and Events](physics_queries_events.md)
- [Reflection and Serialization](reflection_serialization.md)

