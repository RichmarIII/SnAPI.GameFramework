# SnAPI::GameFramework::ComponentSerializationRegistry

## Contents

- **Type:** SnAPI::GameFramework::ComponentSerializationRegistry::Entry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::CreateFn = std::function<TExpected<void*>(NodeGraph& Graph, NodeHandle Owner)>`

Callback to create a component on a graph.

**Returns:** Pointer to created component (type-erased).
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::CreateWithIdFn = std::function<TExpected<void*>(NodeGraph& Graph, NodeHandle Owner, const Uuid& Id)>`

Callback to create a component with explicit UUID.

**Returns:** Pointer to created component (type-erased).
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::SerializeFn = std::function<TExpected<void>(const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)>`

Callback to serialize a component instance.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::DeserializeFn = std::function<TExpected<void>(void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)>`

Callback to deserialize a component instance.

**Returns:** Success or error.
</div>

## Friends

<div class="snapi-api-card" markdown="1">
### `friend class NodeGraphSerializer`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::ComponentSerializationRegistry::m_mutex`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, Entry, UuidHash> SnAPI::GameFramework::ComponentSerializationRegistry::m_entries`

Registry map by TypeId.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `ComponentSerializationRegistry & SnAPI::GameFramework::ComponentSerializationRegistry::Instance()`

Access the singleton registry.

**Returns:** Registry instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ComponentSerializationRegistry::Register()`

Register a component type using reflection serialization.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ComponentSerializationRegistry::RegisterCustom(SerializeFn Serialize, DeserializeFn Deserialize)`

Register a component type with custom serialization.

**Parameters**

- `Serialize`: 
- `Deserialize`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ComponentSerializationRegistry::Has(const TypeId &Type) const`

Check if a component type is registered.

**Parameters**

- `Type`: Component TypeId.

**Returns:** True if registered.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void * > SnAPI::GameFramework::ComponentSerializationRegistry::Create(NodeGraph &Graph, NodeHandle Owner, const TypeId &Type) const`

Create a component by type id.

**Parameters**

- `Graph`: Owning graph.
- `Owner`: Owner node handle.
- `Type`: Component TypeId.

**Returns:** Pointer to created component or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void * > SnAPI::GameFramework::ComponentSerializationRegistry::CreateWithId(NodeGraph &Graph, NodeHandle Owner, const TypeId &Type, const Uuid &Id) const`

Create a component by type id with explicit UUID.

**Parameters**

- `Graph`: Owning graph.
- `Owner`: Owner node handle.
- `Type`: Component TypeId.
- `Id`: Component UUID.

**Returns:** Pointer to created component or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::Serialize(const TypeId &Type, const void *Instance, std::vector< uint8_t > &OutBytes, const TSerializationContext &Context) const`

Serialize a component instance to bytes.

**Parameters**

- `Type`: Component TypeId.
- `Instance`: 
- `OutBytes`: Output byte vector.
- `Context`: Serialization context.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::Deserialize(const TypeId &Type, void *Instance, const uint8_t *Bytes, size_t Size, const TSerializationContext &Context) const`

Deserialize a component instance from bytes.

**Parameters**

- `Type`: Component TypeId.
- `Instance`: 
- `Bytes`: Serialized bytes.
- `Size`: Byte count.
- `Context`: Serialization context.

**Returns:** Success or error.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::SerializeByReflection(const TypeId &Type, const void *Instance, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context)`

Reflection-based serialization for a component instance.

**Parameters**

- `Type`: Component TypeId.
- `Instance`: 
- `Archive`: Output archive.
- `Context`: Serialization context.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::DeserializeByReflection(const TypeId &Type, void *Instance, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Reflection-based deserialization for a component instance.

**Parameters**

- `Type`: Component TypeId.
- `Instance`: 
- `Archive`: Input archive.
- `Context`: Serialization context.

**Returns:** Success or error.
</div>
