# SnAPI::GameFramework::ValueCodecRegistry

Runtime registry that binds reflected `TypeId`s to concrete value codecs.

Reflection-based systems such as payload serialization, replication, and reflected RPC need to encode values when they only know the runtime `TypeId`. `ValueCodecRegistry` bridges that gap by turning `TValueCodec<T>` specializations into runtime dispatch entries.

Core semantics:
- Registration stores function pointers for encode, decode, and decode-into operations.
- `Version()` increments on every registration and can be used by higher-level caches to invalidate any memoized codec lookup state.
- Missing codecs fail at runtime with `EErrorCode::NotFound`.

Ownership and lifetime:
- The registry is a process-wide singleton.
- Registered callbacks are static function pointers derived from `TValueCodec<T>` and therefore do not capture user state.

Threading:
- Not internally synchronized.
- Registration and lookup must not race. In practice, register codecs during startup before multiple threads begin using the registry.

## Contents

- **Type:** SnAPI::GameFramework::ValueCodecRegistry::CodecEntry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ValueCodecRegistry::EncodeFn = TExpected<void>(*)(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)`

Runtime function signature used to encode one type-erased value.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ValueCodecRegistry::DecodeFn = TExpected<Variant>(*)(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)`

Runtime function signature used to decode one value into a `Variant`.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ValueCodecRegistry::DecodeIntoFn = TExpected<void>(*)(void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)`

Runtime function signature used to decode one value directly into caller-provided storage.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, CodecEntry, UuidHash> SnAPI::GameFramework::ValueCodecRegistry::m_entries`

Runtime dispatch table keyed by reflected `TypeId`.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ValueCodecRegistry::m_version`

Monotonic mutation version used by higher-level caches.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `ValueCodecRegistry & SnAPI::GameFramework::ValueCodecRegistry::Instance()`

Access the process-wide value codec registry.

**Returns:** Singleton registry instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ValueCodecRegistry::Register()`

Register the default `TValueCodec<T>` under `StaticTypeId<T>()`.

Re-registering the same type replaces the existing callbacks and increments the registry version.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ValueCodecRegistry::RegisterAs(const TypeId &Type)`

Register the default `TValueCodec<T>` under an explicit reflected type id.

This is primarily used when the reflected field type is not expressed directly as `StaticTypeId<T>()`, such as generated or aliased reflected container types.

**Parameters**

- `Type`: Reflected type id to bind. `TypeId{}` is ignored.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ValueCodecRegistry::Has(const TypeId &Type) const`

Check whether a runtime codec exists for one reflected type.

**Parameters**

- `Type`: Reflected type id to query.

**Returns:** `true` when the registry currently has a dispatch entry for `Type`.
</div>
<div class="snapi-api-card" markdown="1">
### `const ValueCodecRegistry::CodecEntry * SnAPI::GameFramework::ValueCodecRegistry::FindEntry(const TypeId &Type) const`

Look up the raw runtime dispatch entry for one reflected type.

**Parameters**

- `Type`: Reflected type id to query.

**Returns:** Pointer to the registry entry, or `nullptr` when the type is not registered.

**Notes**

- The returned pointer is borrowed and becomes invalid if the registry storage rehashes due to later registrations.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ValueCodecRegistry::Version() const`

Return the registry mutation version.

Higher-level caches use this value to detect stale `TypeId` to codec-entry bindings.

**Returns:** Monotonic counter incremented on each successful registration call.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ValueCodecRegistry::Encode(const TypeId &Type, const void *Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context) const`

Encode a type-erased value using its reflected type id.

**Parameters**

- `Type`: Reflected type id of the value.
- `Value`: Pointer to the value storage. Must match `Type`.
- `Archive`: Destination archive.
- `Context`: Borrowed serialization context.

**Returns:** `Ok()` on success or an error when no codec is registered or the codec rejects the value.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::ValueCodecRegistry::Decode(const TypeId &Type, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context) const`

Decode a value by reflected type id into a `Variant`.

**Parameters**

- `Type`: Reflected type id of the value to decode.
- `Archive`: Source archive.
- `Context`: Borrowed serialization context.

**Returns:** `Variant` containing the decoded value on success, or an error when no codec is registered or decode fails.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ValueCodecRegistry::DecodeInto(const TypeId &Type, void *Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context) const`

Decode a value by reflected type id directly into existing storage.

**Parameters**

- `Type`: Reflected type id of the value to decode.
- `Value`: Destination storage. Must point to an object compatible with `Type`.
- `Archive`: Source archive.
- `Context`: Borrowed serialization context.

**Returns:** `Ok()` on success or an error when no codec is registered or decode fails.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ValueCodecRegistry::EncodeImpl(const void *Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context)`

Template encoder implementation.

**Parameters**

- `Value`: 
- `Archive`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::ValueCodecRegistry::DecodeImpl(cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Template decoder implementation.

**Parameters**

- `Archive`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ValueCodecRegistry::DecodeIntoImpl(void *Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Template decode-into implementation.

**Parameters**

- `Value`: 
- `Archive`: 
- `Context`:
</div>
