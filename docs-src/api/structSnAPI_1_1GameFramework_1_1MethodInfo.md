# SnAPI::GameFramework::MethodInfo

Reflection metadata for one invokable method.

Invocation is expressed in terms of variant-packed arguments and a variant return value so the same metadata can serve scripting, editor tooling, and RPC dispatch.

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
