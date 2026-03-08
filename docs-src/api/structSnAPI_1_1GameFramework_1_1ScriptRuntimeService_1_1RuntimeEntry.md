# SnAPI::GameFramework::ScriptRuntimeService::RuntimeEntry

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<IScriptEngineBackend> SnAPI::GameFramework::ScriptRuntimeService::RuntimeEntry::Backend`

Owning backend instance for one enum slot.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ScriptRuntimeService::RuntimeEntry::Initialized`

True once `Initialize()` has succeeded and until `Shutdown()` resets the slot.
</div>
