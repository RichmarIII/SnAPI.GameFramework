# SnAPI::GameFramework::TSubClassOf

Reflected handle that stores a subclass selection constrained to a reflected base type.

`TSubClassOf<TBase>` is the type-selection counterpart to `TAssetRef`: instead of pointing to an asset instance, it points to reflected type metadata that must satisfy `TypeRegistry::IsA(Type, TBase)`.

Core semantics:
- The stored `TypeId` is authoritative when valid.
- `TypeName` is a fallback/display string and is refreshed from `TypeRegistry` when a valid type is set.
- `SetTypeByName()` matches either the fully qualified reflected name or the final `::ShortName`.
- `EnumerateTypes()` includes the base type itself and every currently registered derived type.

Threading model:
- Value operations are thread-safe in isolation.
- Validity and enumeration depend on the global `TypeRegistry`.

## Contents

- **Type:** SnAPI::GameFramework::TSubClassOf::TEntry

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TSubClassOf< TBase >::m_typeName`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TSubClassOf< TBase >::m_typeId`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TSubClassOf< TBase >::TSubClassOf()=default`

Construct an empty subclass selection.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TSubClassOf< TBase >::TSubClassOf(const TypeId &Type)`

Construct a subclass selection from a type id.

Invalid ids leave the object empty.

**Parameters**

- `Type`: Candidate reflected type id.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::TSubClassOf< TBase >::GetTypeName() const`

Access the stored fallback/display type name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string & SnAPI::GameFramework::TSubClassOf< TBase >::EditTypeName()`

Mutably access the stored fallback/display type name.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::TSubClassOf< TBase >::GetTypeId() const`

Access the stored type id.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId & SnAPI::GameFramework::TSubClassOf< TBase >::EditTypeId()`

Mutably access the stored type id.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TSubClassOf< TBase >::IsNull() const`

Query whether no subclass is currently selected.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TSubClassOf< TBase >::Clear()`

Clear both the stored type id and fallback/display name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TSubClassOf< TBase >::IsValid() const`

Check whether the stored type id currently resolves to a compatible reflected type.

**Returns:** `true` when the stored id is non-null and `IsA(id, StaticTypeId<TBase>())`.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TSubClassOf< TBase >::ResolvedTypeName() const`

Resolve the best current type name for display.

**Returns:** Reflected name from `TypeRegistry` when the stored id is valid, otherwise the stored fallback name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TSubClassOf< TBase >::SetType(const TypeId &Type)`

Set the subclass selection from a reflected type id.

**Parameters**

- `Type`: Candidate reflected type id.

**Returns:** `true` when the id resolves to a compatible reflected type or when clearing with a nil id.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TSubClassOf< TBase >::SetTypeByName(std::string_view Name)`

Set the subclass selection by reflected name.

**Parameters**

- `Name`: Fully qualified type name or short unqualified type name.

**Returns:** `true` when a compatible reflected type is found.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TSubClassOf< TBase >::SetType()`

Set the subclass selection from a compile-time derived type.

**Returns:** `true` when the reflected type is compatible and available.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TSubClassOf< TBase >::ResolveTypeOr(const TypeId &FallbackType) const`

Resolve the stored type id or fall back to a caller-supplied default.

**Parameters**

- `FallbackType`: Type id returned when the current selection is invalid.

**Returns:** Compatible stored type id or `FallbackType`.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static std::vector< TEntry > SnAPI::GameFramework::TSubClassOf< TBase >::EnumerateTypes()`

Enumerate the currently known compatible reflected types.

Enumeration reflects the current `TypeRegistry` snapshot and therefore grows as more lazy auto-registration callbacks are executed.

**Returns:** Sorted list containing the base type and all currently registered derived types.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::TSubClassOf< TBase >::IsTypeCompatible(const TypeId &Type)`

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::TSubClassOf< TBase >::NameMatches(const std::string &CandidateName, const std::string_view Query)`

**Parameters**

- `CandidateName`: 
- `Query`:
</div>
