# SnAPI::GameFramework::ScriptRuntimeService

World-owned coordinator for all registered scripting backends.

This service is the engine-facing entry point for script creation. It stores one backend slot per `EScriptBackend`, initializes backends lazily on first use, and exposes a backend-neutral API to gameplay systems such as `ScriptComponent`.

Core semantics:
- Exactly zero or one backend may be registered for each enum slot.
- Backend registration does not initialize the backend immediately.
- `CreateScript()` ensures the selected backend is initialized before delegating to it.
- `Shutdown()` shuts down initialized backends but keeps the backend objects registered so they can be initialized again later.

Ownership and lifetime:
- The service owns registered backends.
- Returned backend pointers are borrowed and remain valid until the service is destroyed or the backend registration model changes.

Threading:
- Not thread-safe. The service mutates backend state and should be driven from a single owner thread, typically the World thread.

## Contents

- **Type:** SnAPI::GameFramework::ScriptRuntimeService::RuntimeEntry

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::ScriptRuntimeService::kBackendSlotCount`

Size of the fixed backend slot array.

Extend this when new backend enum values are added.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `RuntimeEntry SnAPI::GameFramework::ScriptRuntimeService::m_entries[kBackendSlotCount][kBackendSlotCount]`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScriptRuntimeService::ScriptRuntimeService()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScriptRuntimeService::~ScriptRuntimeService()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScriptRuntimeService::ScriptRuntimeService(const ScriptRuntimeService &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptRuntimeService & SnAPI::GameFramework::ScriptRuntimeService::operator=(const ScriptRuntimeService &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScriptRuntimeService::ScriptRuntimeService(ScriptRuntimeService &&)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptRuntimeService & SnAPI::GameFramework::ScriptRuntimeService::operator=(ScriptRuntimeService &&)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::ScriptRuntimeService::RegisterBackend(std::unique_ptr< IScriptEngineBackend > Backend)`

Register a backend implementation in its enum slot.

**Parameters**

- `Backend`: 

**Returns:** `Ok()` on success or an error when the pointer is null, the backend reports `EScriptBackend::None`, the enum is out of range, or a backend is already registered in that slot.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ScriptRuntimeService::HasBackend(EScriptBackend BackendType) const`

Check whether a backend is registered for one slot.

**Parameters**

- `BackendType`: Backend slot to query.

**Returns:** `true` when a backend object is registered, regardless of initialization state.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< IScriptEngineBackend * > SnAPI::GameFramework::ScriptRuntimeService::Backend(EScriptBackend BackendType)`

Borrow the backend registered for one slot.

**Parameters**

- `BackendType`: Backend slot to query.

**Returns:** Borrowed backend pointer on success or an error when the slot is invalid or unregistered.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< const IScriptEngineBackend * > SnAPI::GameFramework::ScriptRuntimeService::Backend(EScriptBackend BackendType) const`

Borrow the backend registered for one slot through a const view.

**Parameters**

- `BackendType`: Backend slot to query.

**Returns:** Borrowed backend pointer on success or an error when the slot is invalid or unregistered.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::shared_ptr< IScript > > SnAPI::GameFramework::ScriptRuntimeService::CreateScript(EScriptBackend BackendType, const ScriptCreateInfo &CreateInfo)`

Create a script instance using one backend slot.

**Parameters**

- `BackendType`: Backend slot to use.
- `CreateInfo`: Module path, entry point, and owner-object context.

**Returns:** Shared script instance on success or an error when the backend is missing, cannot be initialized, or cannot create the instance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::ScriptRuntimeService::ModuleGeneration(EScriptBackend BackendType, std::string_view ScriptPath) const`

Query a backend's current hot-reload generation for a module path.

**Parameters**

- `BackendType`: Backend slot to query.
- `ScriptPath`: Backend-specific module path or module identifier.

**Returns:** Generation counter, or `0` when the backend slot is invalid or unregistered, or when the backend has no known module record for that path.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::ScriptRuntimeService::TickHotReload()`

Advance hot-reload processing on every initialized backend.

**Returns:** `Ok()` on success or the first backend error encountered.

**Notes**

- Uninitialized backends are skipped.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ScriptRuntimeService::Shutdown()`

Shut down all initialized backends.

Backend objects remain registered after shutdown; only their initialized runtime state is torn down.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::ScriptRuntimeService::BackendIndex(EScriptBackend BackendType)`

**Parameters**

- `BackendType`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::ScriptRuntimeService::EnsureBackendInitialized(RuntimeEntry &Entry)`

**Parameters**

- `Entry`:
</div>
