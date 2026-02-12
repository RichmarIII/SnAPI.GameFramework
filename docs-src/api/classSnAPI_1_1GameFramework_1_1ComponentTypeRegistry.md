# SnAPI::GameFramework::ComponentTypeRegistry

Global registry for component type indices and masks.

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::ComponentTypeRegistry::m_mutex`

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

Get or assign a bit index for a component type.

**Parameters**

- `Id`: Component type id.

**Returns:** Bit index for the type.
</div>
<div class="snapi-api-card" markdown="1">
### `static uint32_t SnAPI::GameFramework::ComponentTypeRegistry::Version()`

Get the current registry version.

**Returns:** Version counter.
</div>
<div class="snapi-api-card" markdown="1">
### `static size_t SnAPI::GameFramework::ComponentTypeRegistry::WordCount()`

Get the number of 64-bit words required for the mask.

**Returns:** Word count for the current type set.
</div>
