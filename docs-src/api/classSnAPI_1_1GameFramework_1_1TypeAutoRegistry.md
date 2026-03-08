# SnAPI::GameFramework::TypeAutoRegistry

Registry of lazy reflection-registration callbacks keyed by deterministic `TypeId`.

`TypeAutoRegistry` decouples cheap static initialization from expensive `TypeRegistry` mutation. Each reflected type installs an ensure callback during static initialization, and the callback is executed only when some runtime path first needs the metadata.

Core semantics:
- `Register()` stores the first callback for a `TypeId`.
- A second registration for the same `TypeId` is ignored and debug-asserted unless it is the same callback.
- `Ensure()` looks up the callback without holding the lock during execution.
- `EnsureAll()` snapshots the current key set and attempts every ensure callback, returning the first error but continuing best-effort.

Threading model:
- Thread-safe. Internal maps are guarded by `GameMutex`.
- Ensure callbacks themselves must still be idempotent and safe for repeated calls.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TypeAutoRegistry::EnsureFn = Result(*)()`

Ensure callback signature.

Implementations should be idempotent and return `Ok()` if the type is already registered.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::TypeAutoRegistry::m_mutex`

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

Access the process-wide singleton.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TypeAutoRegistry::Register(const TypeId &Id, std::string_view Name, EnsureFn Fn)`

Register an ensure callback for a `TypeId`.

The first callback wins. Later registrations for the same id are ignored; in debug builds the registry asserts if the callback pointer differs.

**Parameters**

- `Id`: Stable type id.
- `Name`: Stable type name (for diagnostics).
- `Fn`: Ensure function pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::TypeAutoRegistry::Ensure(const TypeId &Id) const`

Ensure that a `TypeId` has registered metadata in `TypeRegistry`.

Returns `NotFound` when no auto-registration entry exists for the supplied id.

**Parameters**

- `Id`: Type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::TypeAutoRegistry::EnsureAll() const`

Ensure every currently registered auto-type has been registered with `TypeRegistry`.

The registry continues best-effort after the first failure so later entries still get a chance to register.

**Returns:** Success or the first encountered error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeAutoRegistry::Has(const TypeId &Id) const`

Check whether an ensure callback exists for a `TypeId`.

**Parameters**

- `Id`:
</div>
