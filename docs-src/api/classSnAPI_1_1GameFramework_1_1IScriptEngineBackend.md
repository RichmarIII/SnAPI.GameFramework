# SnAPI::GameFramework::IScriptEngineBackend

Backend interface implemented by each supported scripting language runtime.

`IScriptEngineBackend` owns the VM-level integration details for one language. The GameFramework runtime manages exactly one backend instance per `EScriptBackend` slot and uses this interface to initialize the VM, load or reload modules, create live script objects, and process hot-reload work.

Ownership:
- `ScriptRuntimeService` takes ownership of backend implementations via `unique_ptr`.
- Backends create and return shared `IScript` instances to callers.

Threading:
- This interface does not promise thread safety.
- Individual backends may add internal locking, but callers should still treat them as runtime services rather than free-threaded utilities.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IScriptEngineBackend::~IScriptEngineBackend()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual EScriptBackend SnAPI::GameFramework::IScriptEngineBackend::BackendType() const =0`

Return the enum slot represented by this backend.

**Returns:** Backend identifier. Must not return `EScriptBackend::None`.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScriptEngineBackend::Initialize()=0`

Initialize the backend runtime.

**Returns:** `Ok()` on success or an error when the runtime cannot be started.

**Notes**

- `ScriptRuntimeService` calls this lazily before the first create or hot-reload use.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScriptEngineBackend::Shutdown()=0`

Shut the backend runtime down.

**Returns:** `Ok()` on success or an error when shutdown reports a backend failure.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScriptEngineBackend::LoadModule(std::string_view ScriptPath)=0`

Load a module into the backend.

**Parameters**

- `ScriptPath`: Backend-specific module path or module identifier.

**Returns:** `Ok()` on success or an error when the module cannot be loaded.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScriptEngineBackend::ReloadModule(std::string_view ScriptPath)=0`

Reload a previously known module.

**Parameters**

- `ScriptPath`: Backend-specific module path or module identifier.

**Returns:** `Ok()` on success or an error when reload fails.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::uint64_t SnAPI::GameFramework::IScriptEngineBackend::ModuleGeneration(std::string_view ScriptPath) const =0`

Query the current hot-reload generation for one module.

**Parameters**

- `ScriptPath`: Backend-specific module path or module identifier.

**Returns:** Monotonic generation counter, or `0` when the backend has no known module record for that path.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< std::shared_ptr< IScript > > SnAPI::GameFramework::IScriptEngineBackend::CreateScript(const ScriptCreateInfo &CreateInfo)=0`

Create one live script instance.

**Parameters**

- `CreateInfo`: Module path, entry point, and owner-object context.

**Returns:** Shared script instance on success or an error explaining why instance creation failed.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScriptEngineBackend::TickHotReload()=0`

Advance any backend-specific hot-reload work.

**Returns:** `Ok()` on success or the first backend error encountered during processing.

**Notes**

- `ScriptRuntimeService` only calls this for backends that have already been initialized.
</div>
