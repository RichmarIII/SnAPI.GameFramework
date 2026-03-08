# SnAPI::GameFramework::ScriptCreateInfo

Parameters required to create one script instance.

`ScriptCreateInfo` describes what module to load, which entry point inside that module to instantiate, and which gameplay objects should be exposed to the script as context.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptCreateInfo::ScriptPath`

Backend-specific script module path or module identifier.

Typically a path that has already been normalized or can be normalized by the backend.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptCreateInfo::EntryPoint`

Optional backend-specific class, table, or factory name inside the module.

Empty means "use the module root/default entry".
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptInstanceContext SnAPI::GameFramework::ScriptCreateInfo::Context`

Borrowed World and owner-object context to inject into the script instance.
</div>
