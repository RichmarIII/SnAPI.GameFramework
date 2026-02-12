# SnAPI::GameFramework::ObjectRegistry::Entry

Registry entry payload.

## Public Members

<div class="snapi-api-card" markdown="1">
### `EObjectKind SnAPI::GameFramework::ObjectRegistry::Entry::Kind`

Object kind.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode* SnAPI::GameFramework::ObjectRegistry::Entry::Node`

Node pointer if Kind == Node.
</div>
<div class="snapi-api-card" markdown="1">
### `IComponent* SnAPI::GameFramework::ObjectRegistry::Entry::Component`

Component pointer if Kind == Component.
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::ObjectRegistry::Entry::Other`

Opaque pointer if Kind == Other.
</div>
<div class="snapi-api-card" markdown="1">
### `std::type_index SnAPI::GameFramework::ObjectRegistry::Entry::Type`

Concrete type for Other.
</div>
