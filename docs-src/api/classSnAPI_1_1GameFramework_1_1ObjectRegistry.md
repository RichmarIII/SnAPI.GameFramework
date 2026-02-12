# SnAPI::GameFramework::ObjectRegistry

Global registry mapping UUIDs to live object pointers.

## Contents

- **Type:** SnAPI::GameFramework::ObjectRegistry::Entry

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::ObjectRegistry::m_mutex`

Protects the registry map.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<Uuid, Entry, UuidHash> SnAPI::GameFramework::ObjectRegistry::m_entries`

UUID -> Entry map.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static ObjectRegistry & SnAPI::GameFramework::ObjectRegistry::Instance()`

Access the singleton registry instance.

**Returns:** Reference to the registry.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterNode(const Uuid &Id, BaseNode *Node)`

Register a node with the registry.

**Parameters**

- `Id`: UUID of the node.
- `Node`: Pointer to the node.

**Notes**

- Id must not be nil.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterComponent(const Uuid &Id, IComponent *Component)`

Register a component with the registry.

**Parameters**

- `Id`: UUID of the component.
- `Component`: Pointer to the component.

**Notes**

- Id must not be nil.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::Register(const Uuid &Id, T *Object)`

Register an arbitrary object with the registry.

**Parameters**

- `Id`: UUID of the object.
- `Object`: Pointer to the object.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::Unregister(const Uuid &Id)`

Unregister an object by UUID.

**Parameters**

- `Id`: UUID to remove.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::Resolve(const Uuid &Id) const`

Resolve a UUID to a typed pointer.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Pointer to the object, or nullptr if not found/type mismatch.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ObjectRegistry::IsValid(const Uuid &Id) const`

Check if a UUID resolves to a live object of type T.

**Parameters**

- `Id`: UUID to check.

**Returns:** True when Resolve<T> returns non-null.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterInternal(const Uuid &Id, EObjectKind Kind, BaseNode *Node, IComponent *Component, void *Other, std::type_index Type)`

Internal insert/update for registry entries.

**Parameters**

- `Id`: UUID key.
- `Kind`: Object kind.
- `Node`: Node pointer.
- `Component`: Component pointer.
- `Other`: Opaque pointer.
- `Type`: type_index for Other.

**Notes**

- Id must not be nil.
</div>
