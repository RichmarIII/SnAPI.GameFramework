# SnAPI::GameFramework::ComponentTypeRegistry

Global allocator for compact component-type bit indices.

`ComponentTypeRegistry` turns arbitrary reflected component `TypeId`s into dense bit positions used by masks and query acceleration structures. The assigned index for a type remains stable for the lifetime of the process.

Threading:
- Not generally thread-safe.
- Internal `GameMutex` use validates affinity only.

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::ComponentTypeRegistry::m_mutex`

Protects registry state.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, uint32_t, UuidHash> SnAPI::GameFramework::ComponentTypeRegistry::m_typeToIndex`

TypeId -> bit index.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ComponentTypeRegistry::m_version`

Version counter.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static uint32_t SnAPI::GameFramework::ComponentTypeRegistry::TypeIndex(const TypeId &Id)`

Get the existing bit index for a component type, or assign a new one.

**Parameters**

- `Id`: Component type id.

**Returns:** Bit index for the type.
</div>
<div class="snapi-api-card" markdown="1">
### `static uint32_t SnAPI::GameFramework::ComponentTypeRegistry::Version()`

Get the current mutation version of the registry.

**Returns:** Version counter.
</div>
<div class="snapi-api-card" markdown="1">
### `static size_t SnAPI::GameFramework::ComponentTypeRegistry::WordCount()`

Get the number of 64-bit words needed to represent the current type set.
</div>
