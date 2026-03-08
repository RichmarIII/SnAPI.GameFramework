# SnAPI::GameFramework::ScriptComponent

Component that binds one gameplay object to a backend script instance.

`ScriptComponent` is the standard engine-side bridge from the Node/Component model to the scripting runtime. Users configure a script module path in `ScriptModule` and an optional entry point name in `ScriptType`; the component then creates and maintains a live `IScript` instance on demand.

Design intent:
- let gameplay authors attach script behavior declaratively to Nodes
- keep script lifetime aligned with component lifetime
- allow hot reload and editor property edits to rebind safely without forcing the rest of the engine to know about backend details

Core semantics:
- `OnCreate()` does not bind immediately. It marks the component as needing a create hook and defers the actual bind until a later tick or editor-triggered rebind.
- Backend selection is derived from `ScriptModule`. The current implementation treats an empty extension or `.lua` as `EScriptBackend::Lua`; any other extension resolves to `EScriptBackend::None`.
- Rebinding occurs when the backend changes, the module path changes, the entry point changes, or the backing module's hot-reload generation changes.
- Script hook failures are logged to `stderr` and swallowed so the owning World can continue ticking.

Ownership and lifetime:
- The component owns no backend runtime itself.
- The bound script instance is held by `std::shared_ptr<IScript>` and released on destroy or rebind.
- `Instance` is a diagnostic runtime id only; `0` means "currently unbound".

Threading:
- Main-thread/world-thread only.
- Not thread-safe; it mutates runtime bindings and may touch World-owned services.

Performance:
- Tick hooks may trigger lazy binding and therefore can allocate, initialize a backend, or reload modules on the first use after configuration changes.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::ScriptComponent::kTypeName`

Stable type name for reflection.
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptComponent::ScriptModule`

Backend-visible script module path or module identifier.

Resolved through `PathResolver` before backend selection and instance creation.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptComponent::ScriptType`

Optional backend-specific entry point, such as a Lua table or factory field inside `ScriptModule`.
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptInstanceId SnAPI::GameFramework::ScriptComponent::Instance`

Runtime instance id of the currently bound script.

`0` means no live script instance is bound.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<IScript> SnAPI::GameFramework::ScriptComponent::m_script`

Shared handle to the currently bound script instance.

Empty when unbound.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptComponent::m_boundModule`

Module path used to create the current script instance.

Compared against `ScriptModule` to decide whether rebinding is required.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptComponent::m_boundEntryPoint`

Entry point used to create the current script instance.

Compared against `ScriptType` to decide whether rebinding is required.
</div>
<div class="snapi-api-card" markdown="1">
### `EScriptBackend SnAPI::GameFramework::ScriptComponent::m_boundBackend`

Backend used by the current binding.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::ScriptComponent::m_boundModuleGeneration`

Hot-reload generation captured when the current script instance was created.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ScriptComponent::m_pendingCreateHook`

True when the next successful bind must deliver `EScriptHook::OnCreate`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ScriptComponent::m_createHookDelivered`

True after `EScriptHook::OnCreate` has been delivered to the current binding.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ScriptComponent::m_bindFailureLogged`

Suppresses repeated identical bind warnings until configuration changes or a new bind succeeds.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::OnCreate(IWorld &WorldRef)`

Begin the component's script lifecycle for a new engine lifetime.

This call resets the runtime instance id, marks the component as needing an `EScriptHook::OnCreate`, and clears one-shot bind-failure suppression flags. It does not create a script instance immediately.

**Parameters**

- `WorldRef`: Owning World. Present for lifecycle symmetry; the current implementation defers actual binding to later calls.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::OnDestroy(IWorld &WorldRef)`

Tear down the currently bound script instance, if any.

**Parameters**

- `WorldRef`: Owning World.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::PreTick(IWorld &WorldRef, float DeltaSeconds)`

Forward the engine pre-tick phase to the bound script.

**Parameters**

- `WorldRef`: Owning World.
- `DeltaSeconds`: Elapsed time in seconds since the previous pre-tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::Tick(IWorld &WorldRef, float DeltaSeconds)`

Forward the engine main tick phase to the bound script.

**Parameters**

- `WorldRef`: Owning World.
- `DeltaSeconds`: Elapsed time in seconds since the previous variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::FixedTick(IWorld &WorldRef, float DeltaSeconds)`

Forward the engine fixed-timestep phase to the bound script.

**Parameters**

- `WorldRef`: Owning World.
- `DeltaSeconds`: Fixed simulation step in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::LateTick(IWorld &WorldRef, float DeltaSeconds)`

Forward the engine late-tick phase to the bound script.

**Parameters**

- `WorldRef`: Owning World.
- `DeltaSeconds`: Elapsed time in seconds since the previous late-tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::PostTick(IWorld &WorldRef, float DeltaSeconds)`

Forward the engine post-tick phase to the bound script.

**Parameters**

- `WorldRef`: Owning World.
- `DeltaSeconds`: Elapsed time in seconds since the previous post-tick.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `EScriptBackend SnAPI::GameFramework::ScriptComponent::ResolveBackend() const`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::ScriptComponent::EnsureBound(IWorld &WorldRef)`

**Parameters**

- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::Unbind(bool InvokeDestroyHook)`

**Parameters**

- `InvokeDestroyHook`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptComponent::InvokeHook(IWorld &WorldRef, EScriptHook Hook, std::span< const Variant > Args={})`

**Parameters**

- `WorldRef`: 
- `Hook`: 
- `Args`:
</div>
