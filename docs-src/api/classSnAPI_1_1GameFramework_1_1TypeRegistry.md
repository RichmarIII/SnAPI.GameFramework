# SnAPI::GameFramework::TypeRegistry

Global runtime registry of reflected type metadata.

`TypeRegistry` is the canonical metadata index keyed by deterministic `TypeId`.

Read/write model:
- Unfrozen mode: registration and lookup use mutex protection, and `Find*()` may trigger lazy auto-registration on misses.
- Frozen mode: registration is rejected, no lazy auto-registration is attempted, and lookup uses a lock-free fast path over the already-populated maps.

This allows startup/bootstrap code to remain flexible while hot lookup paths in replication, serialization, and tooling avoid lock contention once bootstrap is complete.

Ownership and lifetime:
- Stored `TypeInfo` records live for the process lifetime.
- Returned pointers remain valid for the lifetime of the process once registration succeeds.

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::TypeRegistry::m_mutex`

Guards registry mutation and non-frozen lookups.
</div>
<div class="snapi-api-card" markdown="1">
### `std::atomic<bool> SnAPI::GameFramework::TypeRegistry::m_frozen`

Frozen state flag controlling read/write mode behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, TypeInfo, UuidHash> SnAPI::GameFramework::TypeRegistry::m_types`

Primary metadata store keyed by TypeId.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::string, TypeId, TransparentStringHash, TransparentStringEqual> SnAPI::GameFramework::TypeRegistry::m_nameToId`

Secondary name index for lookup by stable type name.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TypeRegistry & SnAPI::GameFramework::TypeRegistry::Instance()`

Access the singleton TypeRegistry instance.

**Returns:** Reference to the registry.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< TypeInfo * > SnAPI::GameFramework::TypeRegistry::Register(TypeInfo Info)`

Register a new type record.

Registration fails when:
- the registry is frozen
- the `TypeId` already exists

The implementation currently only checks duplicate ids, not duplicate names, so callers should still treat reflected names as globally unique.

**Parameters**

- `Info`: Owning metadata payload to store.

**Returns:** Pointer to the stored `TypeInfo` or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeInfo * SnAPI::GameFramework::TypeRegistry::Find(const TypeId &Id) const`

Find a reflected type by `TypeId`.

In unfrozen mode, a miss triggers `TypeAutoRegistry::Ensure(Id)` before the lookup is retried.

**Parameters**

- `Id`: Type id to look up.

**Returns:** Pointer to the stored metadata or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeInfo * SnAPI::GameFramework::TypeRegistry::FindByName(std::string_view Name) const`

Find a reflected type by stable name.

In unfrozen mode, a miss deterministically derives `TypeIdFromName(Name)` and tries lazy auto-registration before retrying the lookup.

**Parameters**

- `Name`: Fully qualified type name.

**Returns:** Pointer to the stored metadata or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeRegistry::IsA(const TypeId &Type, const TypeId &Base) const`

Check the reflected inheritance relationship between two types.

**Parameters**

- `Type`: Candidate derived type id.
- `Base`: Candidate base type id.

**Returns:** `true` when `Type == Base` or the reflected base graph reaches `Base`.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< const TypeInfo * > SnAPI::GameFramework::TypeRegistry::Derived(const TypeId &Base) const`

Enumerate all currently registered types derived from a base type.

The result excludes the base type itself and includes transitive descendants.

**Parameters**

- `Base`: Base type id.

**Returns:** Vector of pointers into the current registry snapshot.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< ReflectedFieldRef > SnAPI::GameFramework::TypeRegistry::CollectFields(const TypeId &Type, bool IncludeBaseTypes=true) const`

Collect reflected fields for a type.

The function first ensures the type exists through `Find(Type)` before walking the registry snapshot.

**Parameters**

- `Type`: Type to inspect.
- `IncludeBaseTypes`: `true` to include inherited fields in base-to-derived order.

**Returns:** Field view entries with declaring owner type.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< ReflectedMethodRef > SnAPI::GameFramework::TypeRegistry::CollectMethods(const TypeId &Type, bool IncludeBaseTypes=true) const`

Collect reflected methods for a type.

When inherited methods are included, derived declarations hide base declarations with the same method name, matching C++ name-hiding behavior.

**Parameters**

- `Type`: Type to inspect.
- `IncludeBaseTypes`: `true` to include inherited methods in base-to-derived order.

**Returns:** Method view entries with declaring owner type.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TypeRegistry::Freeze(bool Enable)`

Enable or disable frozen lookup mode.

Freezing prevents future registration and disables lazy auto-registration side effects during lookup. Unfreezing re-enables mutation and lock-based lazy lookup behavior.

**Parameters**

- `Enable`: `true` to freeze the registry.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeRegistry::IsFrozen() const`

Check whether the registry is currently frozen.

**Returns:** `true` when frozen.
</div>
