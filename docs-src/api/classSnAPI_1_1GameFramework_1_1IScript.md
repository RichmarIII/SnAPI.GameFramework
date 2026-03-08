# SnAPI::GameFramework::IScript

Runtime-facing interface for one live script instance.

An `IScript` is the backend-neutral handle returned by the scripting runtime after a module and entry point have been resolved successfully. The object encapsulates backend state such as a Lua registry reference or VM object handle while exposing a uniform engine API.

Ownership and lifetime:
- Instances are returned as `std::shared_ptr<IScript>`.
- Callers may keep shared ownership for as long as the backend remains initialized and the script instance is logically valid.

Threading:
- No thread-safety is guaranteed by this interface.
- In normal GameFramework usage, scripts are invoked from the main/world thread.

Error semantics:
- Hook and member mutators fail by returning `Result`.
- Value-returning operations fail by returning `TExpected<Variant>`.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IScript::~IScript()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual ScriptInstanceId SnAPI::GameFramework::IScript::InstanceId() const =0`

Return the backend-assigned instance id.

**Returns:** Opaque non-persistent identifier for this live instance.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual EScriptBackend SnAPI::GameFramework::IScript::BackendType() const =0`

Return the backend that created this instance.

**Returns:** Backend enum value for the owning script runtime.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::string_view SnAPI::GameFramework::IScript::ScriptPath() const =0`

Return the module path used to create this instance.

**Returns:** Borrowed string view into backend-owned storage. Valid for the lifetime of the script instance.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::uint64_t SnAPI::GameFramework::IScript::ModuleGeneration() const =0`

Return the backend's current generation counter for the underlying module.

**Returns:** Hot-reload generation for the module that produced this instance.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScript::InvokeHook(EScriptHook Hook, std::span< const Variant > Args={})=0`

Invoke one well-known engine lifecycle hook.

**Parameters**

- `Hook`: Hook identifier to run.
- `Args`: Optional arguments supplied by the engine, usually `DeltaSeconds` for tick hooks.

**Returns:** `Ok()` on success or an error describing the backend-side failure.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< Variant > SnAPI::GameFramework::IScript::Invoke(std::string_view Method, std::span< const Variant > Args={})=0`

Invoke an arbitrary backend-visible method on the script instance.

**Parameters**

- `Method`: 
- `Args`: Optional `Variant` argument list in backend-defined order.

**Returns:** Reflected return value on success or an error describing why invocation failed.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< Variant > SnAPI::GameFramework::IScript::GetMember(std::string_view Name) const =0`

Read a named script member.

**Parameters**

- `Name`: Backend-defined member or property name.

**Returns:** Member value on success or an error when the member is absent or not readable.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IScript::SetMember(std::string_view Name, const Variant &Value)=0`

Write a named script member.

**Parameters**

- `Name`: Backend-defined member or property name.
- `Value`: New value to assign.

**Returns:** `Ok()` on success or an error when the member cannot be written.
</div>
