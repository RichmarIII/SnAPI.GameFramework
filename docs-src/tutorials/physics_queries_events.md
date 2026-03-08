# Physics Queries and Events

Simulation is only half of the physics story. The other half is asking questions and reacting to events.

The world-owned `PhysicsSystem` gives you both.

## 1. The Scene Is Borrowed From `PhysicsSystem`

```cpp
auto* Scene = Runtime.World().Physics().Scene();
if (!Scene)
{
    return;
}
```

That pointer is borrowed and only valid while the physics system remains initialized.

## 2. Queries Live On The Scene

The exact query domain is exposed by the physics backend scene. In practice, this is where raycasts, sweeps, overlaps, and other scene queries live.

The GameFramework-side rule is simple:

- get the scene from `World::Physics()`
- issue queries against the scene's query-facing API
- keep world/physics coordinate conversion rules consistent by going through the world physics system

## 3. Use Queries For Gameplay Decisions

Typical uses:

- grounding probes
- line-of-sight checks
- weapon hitscan
- trigger-like checks without adding new bodies
- spawn validation

`CharacterMovementController` itself uses a downward probe to determine grounded state.

That is a good example of the intended pattern: gameplay systems should query the world physics scene instead of inventing local collision guesses.

## 4. Event Drain Flow

`PhysicsSystem::Step(...)` does more than advance the simulation.

It also:

- fetches results
- drains backend events
- stores a pending event queue
- notifies listeners

You can consume the pending queue explicitly with `DrainEvents(...)`.

```cpp
std::array<SnAPI::Physics::PhysicsEvent, 64> Events{};
const std::uint32_t Count = Runtime.World().Physics().DrainEvents(Events);
for (std::uint32_t Index = 0; Index < Count; ++Index)
{
    const auto& Event = Events[Index];
    (void)Event;
}
```

## 5. Listener-Based Flow

There are two listener styles:

- general event listeners for all drained events
- body sleep listeners for one body's sleep/wake notifications

That split is intentional.

Use general listeners for gameplay systems that care about broad scene events.
Use body sleep listeners for body-local state transitions.

## 6. Query Availability In Editor Worlds

The execution profile can disable simulation while still allowing query-style access.

That is important in editor/tooling workflows where you want:

- collider previews
- selection hit tests
- placement validation

without running actual gameplay physics simulation.

## 7. Practical Guidance

### Use explicit listeners for long-lived systems

Good examples:

- gameplay analytics
- trigger routers
- editor visualization systems

### Use `DrainEvents(...)` for frame-local consumers

Good examples:

- a debug overlay that prints contacts this frame
- a test case validating specific event ordering

### Do not confuse event drain with backend polling

`DrainEvents(...)` consumes the GameFramework-owned pending queue, not the raw backend directly.

## What To Read Next

- [Testing and Validation](testing.md)
- [Bouncy Basement](bouncy_basement.md)
