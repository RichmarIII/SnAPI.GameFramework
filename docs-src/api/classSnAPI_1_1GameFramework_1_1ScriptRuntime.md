# SnAPI::GameFramework::ScriptRuntime

Wrapper owning a scripting engine instance.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<IScriptEngine> SnAPI::GameFramework::ScriptRuntime::m_engine`

Owned engine instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScriptRuntime::ScriptRuntime(std::shared_ptr< IScriptEngine > Engine)`

Construct with an engine instance.

**Parameters**

- `Engine`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ScriptRuntime::Initialize()`

Initialize the runtime.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ScriptRuntime::Shutdown()`

Shutdown the runtime.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr< IScriptEngine > SnAPI::GameFramework::ScriptRuntime::Engine() const`

Get the underlying engine.

**Returns:** Shared pointer to the engine.
</div>
