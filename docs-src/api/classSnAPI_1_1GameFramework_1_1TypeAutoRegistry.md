# SnAPI::GameFramework::TypeAutoRegistry

Auto-registration registry for reflected types keyed by TypeId.

The intent is:
- Each reflected type registers a lightweight "ensure" callback at static-init time (via SNAPI_REFLECT_TYPE in a .cpp).
- TypeRegistry/serialization can call Ensure(TypeId) on demand when a TypeId is encountered before its TypeInfo has been registered.

This avoids relying on cross-TU static initialization order for the heavy TypeRegistry registration work.

Contract:
- ensure callbacks must be idempotent and thread-safe for repeated calls
- registration collisions are tolerated only when callback identity matches

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TypeAutoRegistry::EnsureFn = Result(*)()`

Ensure callback signature.

Should be idempotent.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::TypeAutoRegistry::m_mutex`

Protects ensure callback and diagnostics maps.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, EnsureFn, UuidHash> SnAPI::GameFramework::TypeAutoRegistry::m_entries`

TypeId -> ensure callback mapping.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, std::string, UuidHash> SnAPI::GameFramework::TypeAutoRegistry::m_names`

Optional diagnostics map of TypeId -> human-readable type name.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TypeAutoRegistry & SnAPI::GameFramework::TypeAutoRegistry::Instance()`

Access singleton instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TypeAutoRegistry::Register(const TypeId &Id, std::string_view Name, EnsureFn Fn)`

Register an ensure callback for a TypeId.

**Parameters**

- `Id`: Stable type id.
- `Name`: Stable type name (for diagnostics).
- `Fn`: Ensure function pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::TypeAutoRegistry::Ensure(const TypeId &Id) const`

Ensure a TypeId has been registered with TypeRegistry.

**Parameters**

- `Id`: Type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeAutoRegistry::Has(const TypeId &Id) const`

Check whether an ensure callback is registered for Id.

**Parameters**

- `Id`:
</div>
