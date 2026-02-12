# SnAPI::GameFramework::TypeRegistry

Global registry for reflected types.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::TypeRegistry::m_mutex`

Protects registry maps.
</div>
<div class="snapi-api-card" markdown="1">
### `std::atomic<bool> SnAPI::GameFramework::TypeRegistry::m_frozen`

If true, reads skip locking and registration is disabled.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, TypeInfo, UuidHash> SnAPI::GameFramework::TypeRegistry::m_types`

TypeId -> TypeInfo.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::string, TypeId, TransparentStringHash, TransparentStringEqual> SnAPI::GameFramework::TypeRegistry::m_nameToId`

Name -> TypeId.
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

Register a new type.

**Parameters**

- `Info`: Type metadata.

**Returns:** Pointer to the stored TypeInfo or error.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeInfo * SnAPI::GameFramework::TypeRegistry::Find(const TypeId &Id) const`

Find a type by TypeId.

**Parameters**

- `Id`: TypeId to lookup.

**Returns:** Pointer to TypeInfo or nullptr if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeInfo * SnAPI::GameFramework::TypeRegistry::FindByName(std::string_view Name) const`

Find a type by name.

**Parameters**

- `Name`: Fully qualified type name.

**Returns:** Pointer to TypeInfo or nullptr if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeRegistry::IsA(const TypeId &Type, const TypeId &Base) const`

Check inheritance between two types.

**Parameters**

- `Type`: Derived type id.
- `Base`: Base type id.

**Returns:** True if Type is-a Base.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< const TypeInfo * > SnAPI::GameFramework::TypeRegistry::Derived(const TypeId &Base) const`

Get all types derived from a base.

**Parameters**

- `Base`: Base type id.

**Returns:** Vector of derived type infos.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TypeRegistry::Freeze(bool Enable)`

Enable or disable lock-free reads.

**Parameters**

- `Enable`: True to freeze the registry (no further registration).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeRegistry::IsFrozen() const`

Check if the registry is frozen.

**Returns:** True if frozen.
</div>
