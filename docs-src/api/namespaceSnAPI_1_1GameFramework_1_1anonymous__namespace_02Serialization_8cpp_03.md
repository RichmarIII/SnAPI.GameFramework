# SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}

## Contents

- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::MemoryReadStreambuf
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SerializableField
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SerializableFieldCacheEntry
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard

## Variables

<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, std::shared_ptr<SerializableFieldCacheEntry>, UuidHash> SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::g_serializableFieldCache`

TypeId -> cached serializable field plan.
</div>
<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::g_serializableFieldMutex`

Guards serializable field cache map.
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::BuildSerializableFields(const TypeId &Type, std::vector< SerializableField > &Out, std::unordered_map< TypeId, bool, UuidHash > &Visited)`

**Parameters**

- `Type`: 
- `Out`: 
- `Visited`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr< const SerializableFieldCacheEntry > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::GetSerializableFieldCache(const TypeId &Type)`

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::HasSerializableFields(const TypeId &Type)`

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SerializeFieldsRecursive(const TypeId &Type, const void *Instance, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context, std::unordered_map< TypeId, bool, UuidHash > &Visited)`

Serialize fields recursively for a type and its bases.

**Parameters**

- `Type`: TypeId to serialize.
- `Instance`: Pointer to instance.
- `Archive`: Output archive.
- `Context`: Serialization context.
- `Visited`: Cycle guard for type traversal.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::DeserializeFieldsRecursive(const TypeId &Type, void *Instance, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context, std::unordered_map< TypeId, bool, UuidHash > &Visited)`

Deserialize fields recursively for a type and its bases.

**Parameters**

- `Type`: TypeId to deserialize.
- `Instance`: Pointer to instance.
- `Archive`: Input archive.
- `Context`: Serialization context.
- `Visited`: Cycle guard for type traversal.

**Returns:** Success or error.
</div>
