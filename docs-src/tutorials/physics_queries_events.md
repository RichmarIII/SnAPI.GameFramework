# Physics Queries and Events

This page covers the direct-physics API path for advanced gameplay:

1. raycasts, sweeps, overlaps
2. contact/trigger event draining
3. backend routing and domain couplings
4. performance and threading patterns

Use this page after [Physics System and Components](physics.md).

## 1. Accessing the Scene Safely

`World::Physics()` gives access to the adapter and scene pointer:

```cpp
auto* Scene = WorldRef.Physics().Scene();
if (!Scene)
{
    return; // physics not initialized
}
```

From there, use domain interfaces:

- `Scene->Rigid()`
- `Scene->Query()`
- `Scene->Character()`
- `Scene->Vehicle()`
- `Scene->SoftBody()`
- `Scene->Cloth()`

Most gameplay code in GameFramework currently interacts with:

- `Rigid()` through `RigidBodyComponent`
- `Query()` directly or through helpers like `CharacterMovementController`

## 2. Raycast Example

```cpp
#include <array>

std::array<SnAPI::Physics::RaycastHit, 1> Hits{};

SnAPI::Physics::RaycastRequest Request{};
Request.Origin = SnAPI::Physics::Vec3{0.0f, 3.0f, 0.0f};
Request.Direction = SnAPI::Physics::Vec3{0.0f, -1.0f, 0.0f};
Request.Distance = 50.0f;
Request.Mask = 0xFFFFFFFFu;
Request.Mode = SnAPI::Physics::EQueryMode::ClosestHit;

const std::uint32_t Count = Scene->Query().Raycast(Request, std::span<SnAPI::Physics::RaycastHit>(Hits.data(), Hits.size()));
if (Count > 0)
{
    const auto& Hit = Hits[0];
    // Hit.Body, Hit.Position, Hit.Normal, Hit.Distance
}
```

Practical notes:

- keep `Direction` normalized to avoid ambiguous distance interpretation
- use narrow masks to reduce query work in large scenes
- `ClosestHit` is cheaper and usually what gameplay needs

## 3. Sweep and Overlap Patterns

Sweep checks motion of a shape along a direction.
Overlap checks which shapes currently intersect a pose.

```cpp
SnAPI::Physics::SweepRequest Sweep{};
Sweep.Shape.Type = SnAPI::Physics::EShapeType::Capsule;
Sweep.Shape.Capsule.Radius = 0.4f;
Sweep.Shape.Capsule.HalfHeight = 0.9f;
Sweep.Start.Position = SnAPI::Physics::Vec3{0.0f, 1.0f, 0.0f};
Sweep.Direction = SnAPI::Physics::Vec3{1.0f, 0.0f, 0.0f};
Sweep.Distance = 2.0f;
Sweep.Mode = SnAPI::Physics::EQueryMode::ClosestHit;

SnAPI::Physics::SweepHit SweepHit{};
const std::uint32_t SweepCount = Scene->Query().Sweep(Sweep, std::span<SnAPI::Physics::SweepHit>(&SweepHit, 1));
```

```cpp
SnAPI::Physics::OverlapRequest Overlap{};
Overlap.Shape.Type = SnAPI::Physics::EShapeType::Sphere;
Overlap.Shape.Sphere.Radius = 3.0f;
Overlap.Pose.Position = SnAPI::Physics::Vec3{0.0f, 0.0f, 0.0f};
Overlap.Mask = 0xFFFFFFFFu;

std::array<SnAPI::Physics::OverlapHit, 64> Overlaps{};
const std::uint32_t OverlapCount = Scene->Query().Overlap(
    Overlap,
    std::span<SnAPI::Physics::OverlapHit>(Overlaps.data(), Overlaps.size()));
```

When to use each:

- raycast:
    - line-of-sight checks
    - ground probes
    - interaction traces
- sweep:
    - predictive collision checks for moving volumes
    - custom controller motion validation
- overlap:
    - area triggers
    - nearby object scans

## 4. Draining Collision and Trigger Events

`PhysicsSystem` exposes an event queue drain API.

```cpp
#include <array>

std::array<SnAPI::Physics::PhysicsEvent, 256> Events{};
const std::uint32_t EventCount = WorldRef.Physics().DrainEvents(
    std::span<SnAPI::Physics::PhysicsEvent>(Events.data(), Events.size()));

for (std::uint32_t i = 0; i < EventCount; ++i)
{
    const auto& E = Events[i];
    switch (E.Type)
    {
    case SnAPI::Physics::EPhysicsEventType::ContactBegin:
    case SnAPI::Physics::EPhysicsEventType::ContactPersist:
    case SnAPI::Physics::EPhysicsEventType::ContactEnd:
        // E.BodyA / E.BodyB / E.Contact
        break;
    case SnAPI::Physics::EPhysicsEventType::TriggerBegin:
    case SnAPI::Physics::EPhysicsEventType::TriggerEnd:
        // trigger pair started/stopped overlapping
        break;
    }
}
```

Recommended pattern:

- drain once per frame after stepping (or after `Runtime.Update(...)`)
- translate body handles to gameplay object handles if needed
- process with idempotent logic, especially for `ContactPersist`

## 5. Backend Routing and Couplings

`PhysicsBootstrapSettings` includes:

- `Routing`: choose backend per domain (`Rigid`, `Query`, `Character`, etc.)
- `Couplings`: describe cross-domain interactions

Even if all domains currently use Jolt, keep explicit routing in your settings for clarity and future migration.

```cpp
GameRuntimePhysicsSettings Physics{};
Physics.Routing.RigidBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.QueryBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.CharacterBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.VehicleBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.SoftBodyBackend = SnAPI::Physics::EPhysicsBackend::Jolt;
Physics.Routing.ClothBackend = SnAPI::Physics::EPhysicsBackend::Jolt;

SnAPI::Physics::CouplingDesc ClothToRigid{};
ClothToRigid.A = SnAPI::Physics::EPhysicsDomain::Cloth;
ClothToRigid.B = SnAPI::Physics::EPhysicsDomain::Rigid;
ClothToRigid.Mode = SnAPI::Physics::ECouplingMode::OneWay;
ClothToRigid.MaxPairsPerStep = 8192;

Physics.Couplings.push_back(ClothToRigid);
```

Design guidance:

- keep couplings explicit and bounded
- start with minimal coupling set, then expand by measured need
- avoid hidden coupling assumptions in gameplay systems

## 6. Manual Step Mode for Custom Pipelines

If you need strict control over simulation phase order:

1. configure `TickInFixedTick = false` and `TickInVariableTick = false`
2. call `WorldRef.Physics().Step(FixedDeltaSeconds)` manually
3. run event drain and gameplay reactions in your own sequence

Example:

```cpp
const float FixedDt = 1.0f / 60.0f;

WorldRef.Tick(FrameDt);          // gameplay tick, no physics step here
WorldRef.Physics().Step(FixedDt);
WorldRef.LateTick(FrameDt);
WorldRef.EndFrame();
```

This pattern is useful when integrating deterministic simulation pipelines.

## 7. Threading and Performance Guidance

Current behavior:

- `PhysicsSystem` guards lifecycle/state transitions with a mutex.
- scene/domain objects are backend implementations (Jolt-backed currently).

Practical guidance for gameplay code:

- treat the world thread as the authoritative gameplay mutation thread
- avoid long-lived cached raw scene pointers across ownership/lifecycle transitions
- batch query/event processing where possible instead of many tiny scattered calls

Performance tips:

- keep query masks narrow
- prefer `ClosestHit` unless you truly need all hits
- avoid frequent body destroy/recreate churn in frame loops
- use `SetVelocity` for deterministic controller steering, not repeated recreate/setup

## 8. Common Pitfalls

- Draining no events:
    - scene not initialized
    - no contacts/triggers generated for current setup
- Raycasts always miss:
    - wrong mask/layer assumptions
    - direction/distance values not what you expect
- Unexpected jitter:
    - gameplay transform writes fighting physics sync path
    - mixed stepping configuration or inconsistent fixed delta
- Domain API call returns errors:
    - invalid handles (destroyed or never created)
    - backend/domain not available in current routing

## 9. Validation Checklist

Before merging physics-heavy changes:

1. run `PhysicsIntegrationTests`
2. run full `GameFrameworkTests`
3. sanity-check your runtime tick policy
4. confirm query/event paths under expected load

You can start with:

```bash
ctest --test-dir build/debug --output-on-failure -R "Physics|CharacterMovementController"
```

Next: [Reflection and Serialization](reflection_serialization.md)

