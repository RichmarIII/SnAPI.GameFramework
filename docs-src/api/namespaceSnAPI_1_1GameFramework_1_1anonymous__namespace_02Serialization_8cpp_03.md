# SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}

## Contents

- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::MemoryReadStreambuf
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SerializableField
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SerializableFieldCacheEntry
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::PendingNodeDeserialize
- **Type:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TObjectIdRemap

## Variables

<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, std::shared_ptr<SerializableFieldCacheEntry>, UuidHash> SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::g_serializableFieldCache`

TypeId -> cached serializable field plan.
</div>
<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::g_serializableFieldMutex`

Guards serializable field cache map.
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SupportsLegacyFloatVectorDecode()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::ShouldRetryWithLegacyFloatVectorDecode(const Error &ErrorValue, const TSerializationContext &Context)`

**Parameters**

- `ErrorValue`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::ParseInputReadFailureByteCounts(const Error &ErrorValue, std::uint64_t &OutExpectedBytes, std::uint64_t &OutBytesRead)`

**Parameters**

- `ErrorValue`: 
- `OutExpectedBytes`: 
- `OutBytesRead`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::IsMissingTrailingFieldReadFailure(const Error &ErrorValue)`

**Parameters**

- `ErrorValue`:
</div>
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
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::ResolveNodeForPayload(const NodeHandle &Handle, const IWorld *WorldRef)`

**Parameters**

- `Handle`: 
- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::CountPayloadNodes(const NodePayload &Payload)`

**Parameters**

- `Payload`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::BuildNodePayloadObjectIdRemapRecursive(const NodePayload &Payload, TObjectIdRemap &OutRemap)`

**Parameters**

- `Payload`: 
- `OutRemap`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TypeId > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::ResolveNodeTypeFromPayload(const NodePayload &Payload)`

**Parameters**

- `Payload`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodePayload > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::SerializeNodePayloadRecursive(const BaseNode &NodeRef, const TSerializationContext &Context)`

**Parameters**

- `NodeRef`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::CreateNodePayloadRecursive(const NodePayload &Payload, IWorld &WorldRef, const NodeHandle &Parent, std::vector< PendingNodeDeserialize > &OutPending, const std::unordered_map< Uuid, Uuid, UuidHash > *NodeIdRemap)`

**Parameters**

- `Payload`: 
- `WorldRef`: 
- `Parent`: 
- `OutPending`: 
- `NodeIdRemap`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::DeserializeNodePayloadData(const PendingNodeDeserialize &PendingData, IWorld &WorldRef, const TSerializationContext &Context, const std::unordered_map< Uuid, Uuid, UuidHash > *ComponentIdRemap)`

**Parameters**

- `PendingData`: 
- `WorldRef`: 
- `Context`: 
- `ComponentIdRemap`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< NodeHandle > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::LevelRootNodes(const Level &LevelRef)`

**Parameters**

- `LevelRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< NodeHandle > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::WorldRootNodes(const World &WorldRef)`

**Parameters**

- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::DestroyChildrenAndFlush(Level &LevelRef)`

**Parameters**

- `LevelRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::DeserializeNodePayloadImpl(const NodePayload &Payload, IWorld &WorldRef, const NodeHandle &Parent, const Level *GraphContext, const std::unordered_map< Uuid, Uuid, UuidHash > *NodeIdRemap, const std::unordered_map< Uuid, Uuid, UuidHash > *ComponentIdRemap)`

**Parameters**

- `Payload`: 
- `WorldRef`: 
- `Parent`: 
- `GraphContext`: 
- `NodeIdRemap`: 
- `ComponentIdRemap`:
</div>
