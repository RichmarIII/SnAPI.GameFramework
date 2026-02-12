# SnAPI::GameFramework::MethodInfo

Reflection metadata for a method.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::MethodInfo::Name`

Method name as registered.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::MethodInfo::ReturnType`

Return type id.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::MethodInfo::ParamTypes`

Parameter type ids.
</div>
<div class="snapi-api-card" markdown="1">
### `MethodInvoker SnAPI::GameFramework::MethodInfo::Invoke`

Invocation callback.
</div>
<div class="snapi-api-card" markdown="1">
### `MethodFlags SnAPI::GameFramework::MethodInfo::Flags`

Method flags (rpc, etc.).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MethodInfo::IsConst`

True if method is const-qualified.
</div>
