# SnAPI::GameFramework::GameRuntime

World runtime host that centralizes bootstrap and per-frame orchestration.

Ownership:
- owns `World`
- world-owned `NetworkSystem` owns networking resources when enabled

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
### `float SnAPI::GameFramework::GameRuntime::m_fixedAccumulator`

Accumulated fixed-step time.
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
### `void SnAPI::GameFramework::GameRuntime::Update(float DeltaSeconds)`

Run one application frame.

Network session pumping is handled by `World` lifecycle (`Tick` / `EndFrame`), not by `GameRuntime`.

**Parameters**

- `DeltaSeconds`: Frame delta time.
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

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameRuntime::EnsureBuiltinTypesRegistered()`
</div>
