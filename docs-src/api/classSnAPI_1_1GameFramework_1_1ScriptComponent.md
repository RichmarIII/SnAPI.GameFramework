# SnAPI::GameFramework::ScriptComponent

Component that binds a node to a script instance.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::ScriptComponent::kTypeName`

Stable type name for reflection.
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptComponent::ScriptModule`

Backend-defined module identifier/path used for loading.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ScriptComponent::ScriptType`

Backend-defined type/class identifier instantiated from module.
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptInstanceId SnAPI::GameFramework::ScriptComponent::Instance`

Live runtime instance id (0 indicates not currently bound).
</div>
