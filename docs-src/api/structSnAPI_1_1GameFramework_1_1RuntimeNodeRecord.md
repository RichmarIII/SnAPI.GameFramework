# SnAPI::GameFramework::RuntimeNodeRecord

Minimal runtime-owned metadata stored for each ECS runtime node.

This is the compact node-side record tracked by `WorldNodeRuntime`. Higher-level node behavior may still live elsewhere; this struct only carries the identity and flags needed by the dense runtime layer itself.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::RuntimeNodeRecord::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::RuntimeNodeRecord::Name`

Debug/editor-facing node label.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::RuntimeNodeRecord::Type`

Reflected runtime node type.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::RuntimeNodeRecord::Active`

Runtime active flag available to higher-level systems.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::RuntimeNodeRecord::Replicated`

Replication intent flag for networking layers.
</div>
