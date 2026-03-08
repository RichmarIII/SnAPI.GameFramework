# SnAPI::GameFramework::ComponentSerializationRegistry

Registry that knows how to construct and serialize reflected Component types.

`ComponentSerializationRegistry` complements `ValueCodecRegistry`. Instead of handling plain values, it handles Component instances that must be created inside a World, filled from bytes, and then optionally receive deferred `OnCreate` lifecycle callbacks.

Core semantics:
- Registration can install either reflection-based or custom byte serialization.
- Creation is routed by reflected `TypeId`.
- On lookup misses, the registry asks `TypeAutoRegistry` to ensure the reflected type is auto-registered before failing.
- `Deserialize()` only populates fields; `InvokeOnCreate()` is a separate explicit step.

Threading:
- Registry map access is protected by `m_mutex`.
- The callbacks themselves usually mutate World or Component state and therefore are not generally safe to invoke from arbitrary threads. Treat create/deserialize/on-create operations as main-thread/world-thread work.

Ownership:
- The registry is a process-wide singleton.
- It does not own created Components; ownership remains with the World/Node that created them.

## Contents

- **Type:** SnAPI::GameFramework::ComponentSerializationRegistry::Entry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::CreateFn = std::function<TExpected<void*>(IWorld& WorldRef, const NodeHandle& Owner)>`

Callback signature used to create one Component instance inside a World.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::CreateWithIdFn = std::function<TExpected<void*>(IWorld& WorldRef, const NodeHandle& Owner, const Uuid& Id)>`

Callback signature used to create one Component with an explicit UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::SerializeFn = std::function<TExpected<void>(const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)>`

Callback signature used to serialize one type-erased Component instance.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::DeserializeFn = std::function<TExpected<void>(void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)>`

Callback signature used to deserialize bytes into one existing type-erased Component instance.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentSerializationRegistry::OnCreateFn = std::function<TExpected<void>(void* Instance)>`

Callback signature used to deliver deferred post-deserialize component lifecycle.
</div>

## Friends

<div class="snapi-api-card" markdown="1">
### `friend class NodeSerializer`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::ComponentSerializationRegistry::m_mutex`

Guards registry entry map access.

Callback execution happens outside the lock.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, Entry, UuidHash> SnAPI::GameFramework::ComponentSerializationRegistry::m_entries`

Reflected Component type id to registry-entry map.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `ComponentSerializationRegistry & SnAPI::GameFramework::ComponentSerializationRegistry::Instance()`

Access the process-wide Component serialization registry.

**Returns:** Singleton registry instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ComponentSerializationRegistry::Register()`

Register one Component type using default reflection-based serialization.

The default registration installs:
- runtime-Component creation callbacks
- creation-with-explicit-id callbacks
- reflection-based serialize and deserialize callbacks
- a deferred `OnCreate` adapter that calls `T::OnCreate()` or `T::OnCreate(IWorld&)` when available

The current implementation only supports ECS/runtime components that are move constructible and can be added through `AddRuntimeComponent*`.

**Notes**

- If the type is already registered, this function is a no-op.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ComponentSerializationRegistry::RegisterCustom(SerializeFn Serialize, DeserializeFn Deserialize)`

Register one Component type with custom byte serialization callbacks.

Creation and deferred `OnCreate` behavior still use the registry's default runtime component construction logic; only the field byte format is customized.

**Parameters**

- `Serialize`: 
- `Deserialize`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ComponentSerializationRegistry::Has(const TypeId &Type) const`

Check whether one Component type is registered.

**Parameters**

- `Type`: Reflected Component type id.

**Returns:** `true` when the registry has an entry for `Type`.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< TypeId > SnAPI::GameFramework::ComponentSerializationRegistry::Types() const`

Return a snapshot of all currently registered Component type ids.

**Returns:** Copy of the registry's current type-id set.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void * > SnAPI::GameFramework::ComponentSerializationRegistry::Create(IWorld &WorldRef, const NodeHandle &Owner, const TypeId &Type) const`

Create a Component instance by reflected type id.

**Parameters**

- `WorldRef`: Destination World that will own the new Component.
- `Owner`: Owner Node handle that should receive the new Component.
- `Type`: Reflected Component type id.

**Returns:** Borrowed raw pointer to the newly created Component on success, or an error when the type is unknown or cannot be created with the default runtime component path.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void * > SnAPI::GameFramework::ComponentSerializationRegistry::CreateWithId(IWorld &WorldRef, const NodeHandle &Owner, const TypeId &Type, const Uuid &Id) const`

Create a Component instance by reflected type id with an explicit UUID.

**Parameters**

- `WorldRef`: Destination World that will own the new Component.
- `Owner`: Owner Node handle that should receive the new Component.
- `Type`: Reflected Component type id.
- `Id`: Explicit runtime UUID to assign to the created Component.

**Returns:** Borrowed raw pointer to the newly created Component on success, or an error when the type is unknown or cannot be created with the default runtime component path.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::Serialize(const TypeId &Type, const void *Instance, std::vector< uint8_t > &OutBytes, const TSerializationContext &Context) const`

Serialize one Component instance into its raw payload byte form.

**Parameters**

- `Type`: Reflected Component type id.
- `Instance`: 
- `OutBytes`: Destination byte vector. Existing contents are replaced.
- `Context`: Borrowed serialization context.

**Returns:** `Ok()` on success or an error when no serializer is registered, the instance is invalid, or archive serialization throws.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::Deserialize(const TypeId &Type, void *Instance, const uint8_t *Bytes, size_t Size, const TSerializationContext &Context) const`

Deserialize raw payload bytes into an existing Component instance.

**Parameters**

- `Type`: Reflected Component type id.
- `Instance`: 
- `Bytes`: Serialized payload bytes. May be null only when `Size` is zero.
- `Size`: Number of bytes in `Bytes`.
- `Context`: Borrowed serialization context.

**Returns:** `Ok()` on success or an error when no deserializer is registered, the input is invalid, or archive deserialization throws.

**Notes**

- The implementation automatically retries with `UseLegacyFloatVectorDecode=true` when a decode error indicates one of the legacy float-vector compatibility cases.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ComponentSerializationRegistry::InvokeOnCreate(const TypeId &Type, void *Instance) const`

Invoke the deferred `OnCreate` lifecycle hook for one deserialized Component.

**Parameters**

- `Type`: Reflected Component type id.
- `Instance`: 

**Returns:** `Ok()` on success or an error when no callback is registered, the instance is invalid, or the callback throws.
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
