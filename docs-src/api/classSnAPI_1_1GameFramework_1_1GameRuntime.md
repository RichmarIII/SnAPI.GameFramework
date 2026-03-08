# SnAPI::GameFramework::GameRuntime

High-level application host for one running GameFramework session.

`GameRuntime` wraps the repetitive application-shell work around a `World`: creation, subsystem bootstrap, gameplay-host lifetime, per-frame orchestration, optional input/UI bridging, and shutdown ordering. It exists so examples, tools, tests, and games can share one consistent startup/update/shutdown contract.

Core semantics:
- `Init()` creates and configures one world instance.
- `Update()` runs one frame and returns whether the app should continue running.
- `Shutdown()` tears down gameplay first, then world-owned objects while subsystems are still alive.

Ownership and lifetime:
- `GameRuntime` owns the `World`.
- `GameRuntime` owns the optional `GameplayHost`.
- All pointers returned from `WorldPtr()` or `Gameplay()` are non-owning and become invalid after `Shutdown()`.

Threading model:
- Main-thread only for `Init()`, `Update()`, and `Shutdown()`.
- The class does not internally synchronize public API access.

## Private Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::GameRuntime::FrameClock = std::chrono::steady_clock`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameRuntimeSettings SnAPI::GameFramework::GameRuntime::m_settings`

Last initialization settings snapshot.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<class World> SnAPI::GameFramework::GameRuntime::m_world`

Owned runtime world instance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<GameplayHost> SnAPI::GameFramework::GameRuntime::m_gameplayHost`

Optional gameplay orchestration host.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::GameRuntime::m_fixedAccumulator`

Accumulated fixed-step time.
</div>
<div class="snapi-api-card" markdown="1">
### `FrameClock::duration SnAPI::GameFramework::GameRuntime::m_framePacerStep`

Current pacing step duration derived from max-FPS setting.
</div>
<div class="snapi-api-card" markdown="1">
### `FrameClock::time_point SnAPI::GameFramework::GameRuntime::m_nextFrameDeadline`

Next target frame-present deadline used by runtime frame pacer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntime::m_framePacerArmed`

True once pacing deadline baseline has been initialized.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameRuntime::Init(const GameRuntimeSettings &Settings)`

Initialize runtime from settings.

**Parameters**

- `Settings`: 

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntime::Shutdown()`

Shutdown runtime and release world/network resources.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntime::IsInitialized() const`

Check if runtime currently owns a valid world.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntime::Update(float DeltaSeconds)`

Run one application frame.

**Parameters**

- `DeltaSeconds`: Frame delta time.

**Returns:** `true` to continue running; `false` when runtime requests app exit.
</div>
<div class="snapi-api-card" markdown="1">
### `World * SnAPI::GameFramework::GameRuntime::WorldPtr()`

Get mutable world pointer.

**Returns:** World pointer or nullptr when not initialized.
</div>
<div class="snapi-api-card" markdown="1">
### `const World * SnAPI::GameFramework::GameRuntime::WorldPtr() const`

Get const world pointer.

**Returns:** World pointer or nullptr when not initialized.
</div>
<div class="snapi-api-card" markdown="1">
### `World & SnAPI::GameFramework::GameRuntime::World()`

Get mutable world reference.

**Returns:** World reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const World & SnAPI::GameFramework::GameRuntime::World() const`

Get const world reference.

**Returns:** World reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameRuntimeSettings & SnAPI::GameFramework::GameRuntime::Settings() const`

Access current runtime settings snapshot.
</div>
<div class="snapi-api-card" markdown="1">
### `GameplayHost * SnAPI::GameFramework::GameRuntime::Gameplay()`

Access gameplay host.

**Returns:** Gameplay host pointer or nullptr when gameplay is not configured.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameplayHost * SnAPI::GameFramework::GameRuntime::Gameplay() const`

Access gameplay host (const).

**Returns:** Gameplay host pointer or nullptr when gameplay is not configured.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::GameRuntime::StartGameplayHost()`

Start gameplay host from current runtime gameplay settings.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntime::StopGameplayHost()`

Shutdown and detach current gameplay host if present.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntime::ApplyFramePacing(FrameClock::time_point FrameStart)`

Apply end-of-frame pacing for max-FPS limiting.

**Parameters**

- `FrameStart`: Runtime update start timestamp.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntime::ShouldCapFrameRate() const`

Check whether frame pacing cap should run this frame.

**Returns:** True when `MaxFpsWhenVSyncOff` is configured and VSync is currently off.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntime::EnsureBuiltinTypesRegistered()`
</div>
