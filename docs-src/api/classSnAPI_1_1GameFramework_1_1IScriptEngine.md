# SnAPI::GameFramework::IScriptEngine

Interface for a scripting backend (Lua, C#, etc).

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IScriptEngine::~IScriptEngine()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void > SnAPI::GameFramework::IScriptEngine::Initialize()=0`

Initialize the scripting runtime.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void > SnAPI::GameFramework::IScriptEngine::Shutdown()=0`

Shutdown the scripting runtime.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void > SnAPI::GameFramework::IScriptEngine::LoadModule(const std::string &Path)=0`

Load a script module from disk.

**Parameters**

- `Path`: Module path.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void > SnAPI::GameFramework::IScriptEngine::ReloadModule(const std::string &Path)=0`

Reload a script module from disk.

**Parameters**

- `Path`: Module path.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< ScriptInstanceId > SnAPI::GameFramework::IScriptEngine::CreateInstance(const std::string &TypeName)=0`

Create a script instance of a type.

**Parameters**

- `TypeName`: Fully qualified script type name.

**Returns:** Script instance id or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void > SnAPI::GameFramework::IScriptEngine::DestroyInstance(ScriptInstanceId Instance)=0`

Destroy a script instance.

**Parameters**

- `Instance`: Instance id to destroy.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< Variant > SnAPI::GameFramework::IScriptEngine::Invoke(ScriptInstanceId Instance, std::string_view Method, std::span< const Variant > Args)=0`

Invoke a method on a script instance.

**Parameters**

- `Instance`: Instance id.
- `Method`: Method name.
- `Args`: Argument list.

**Returns:** Variant result or error.
</div>
