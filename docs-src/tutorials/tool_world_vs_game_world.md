# Tool World vs Game World

This tutorial exists because tools and runtime worlds are not the same thing, and treating them as the same is where a lot of initialization bugs come from.

## The Core Difference

A runtime world wants to run gameplay.
An editor world wants to expose data safely.

The framework models that with:

- `EWorldKind`
- `WorldExecutionProfile`
- deferred bootstrap behavior for `OnCreate`

## 1. Runtime Profile

A normal game/runtime world uses `WorldExecutionProfile::Runtime()`.

That means:

- gameplay ECS phases run
- input, UI, networking, physics, audio, and renderer work can run if initialized
- end-frame flush runs normally

## 2. Editor Profile

An editor world typically uses `WorldExecutionProfile::Editor()`.

That means:

- gameplay runtime phases are disabled
- physics simulation is disabled
- query-style physics access can still be allowed
- editor-safe ticking can happen instead of gameplay ticking

Example setup on a plain world:

```cpp
World ToolWorld("ToolWorld");
ToolWorld.SetWorldKind(EWorldKind::Editor);
ToolWorld.SetExecutionProfile(WorldExecutionProfile::Editor());
```

## 3. Why This Matters For `OnCreate`

Tool worlds often create UI and viewport state lazily.

If a node tries to touch render-dependent state from `OnCreate` before the viewport exists, you get invalid assumptions and startup races.

That is why bootstrap now supports:

- deferred node `OnCreate`
- suppressed component `OnCreate`
- explicit flush after layout/viewport creation is ready

## 4. `UIRenderViewport` Is The Smoking Gun

`UIRenderViewport` does not create its renderer viewport at C++ construction time.

It creates and syncs it from layout and paint.

That means editor/bootstrap code must allow:

- UI tree creation
- layout and paint
- lazy viewport materialization
- then render-dependent node initialization

## 5. Practical Guidance For Tool Authors

### Safe assumptions

- world-owned services can exist before gameplay runs
- editor worlds may disable gameplay phases entirely
- viewport-backed UI elements are lazy

### Unsafe assumptions

- every renderer viewport exists during scene deserialization
- `OnCreate` always means "all editor viewports are ready"
- tool worlds should run the same frame phases as game worlds

## 6. Good Architectural Placement

Put these kinds of things in services or tool bootstrap code, not random node constructors:

- editor viewport binding
- inspector refresh orchestration
- delayed scene bootstrap
- UI context creation

Put these in gameplay objects only when they genuinely belong there:

- world data
- gameplay state
- object-local visual configuration that can tolerate deferred application

## 7. A Useful Test Mindset

If you change editor bootstrap or viewport code, validate these cases:

1. scene loads before the game viewport paints
2. `WorldRenderSettings` does not race viewport creation
3. editor execution profile does not accidentally run gameplay runtime phases
4. deferred `OnCreate` flush happens after the dependent bootstrap stage

Continue with [Architecture](../architecture.md) if you want the full frame-order picture.
