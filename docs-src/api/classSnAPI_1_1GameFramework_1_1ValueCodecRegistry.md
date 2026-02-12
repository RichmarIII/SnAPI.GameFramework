# SnAPI::GameFramework::ValueCodecRegistry

Registry for value codecs used by reflection serialization.

## Contents

- **Type:** SnAPI::GameFramework::ValueCodecRegistry::CodecEntry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ValueCodecRegistry::EncodeFn = TExpected<void>(*)(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)`

Encoder function signature.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ValueCodecRegistry::DecodeFn = TExpected<Variant>(*)(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)`

Decoder function signature.

**Returns:** Variant containing decoded value or error.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ValueCodecRegistry::DecodeIntoFn = TExpected<void>(*)(void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)`

Decode-into function signature.

**Returns:** Success or error.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, CodecEntry, UuidHash> SnAPI::GameFramework::ValueCodecRegistry::m_entries`

Codec map by TypeId.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ValueCodecRegistry::m_version`
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `ValueCodecRegistry & SnAPI::GameFramework::ValueCodecRegistry::Instance()`

Access the singleton registry.

**Returns:** Registry instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ValueCodecRegistry::Register()`

Register a codec for type T.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ValueCodecRegistry::Has(const TypeId &Type) const`

Check if a codec exists for a type.

**Parameters**

- `Type`: TypeId to query.

**Returns:** True if registered.
</div>
<div class="snapi-api-card" markdown="1">
### `const ValueCodecRegistry::CodecEntry * SnAPI::GameFramework::ValueCodecRegistry::FindEntry(const TypeId &Type) const`

Lookup the codec entry for a type.

**Parameters**

- `Type`: TypeId to query.

**Returns:** Pointer to codec entry or nullptr if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ValueCodecRegistry::Version() const`

Get the codec registry version.

**Returns:** Version counter incremented on registration.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ValueCodecRegistry::Encode(const TypeId &Type, const void *Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context) const`

Encode a value by type id.

**Parameters**

- `Type`: TypeId of the value.
- `Value`: Pointer to the value.
- `Archive`: Output archive.
- `Context`: Serialization context.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::ValueCodecRegistry::Decode(const TypeId &Type, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context) const`

Decode a value by type id.

**Parameters**

- `Type`: TypeId of the value.
- `Archive`: Input archive.
- `Context`: Serialization context.

**Returns:** Variant containing decoded value or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::ValueCodecRegistry::DecodeInto(const TypeId &Type, void *Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context) const`

Decode a value by type id directly into memory.

**Parameters**

- `Type`: TypeId of the value.
- `Value`: Output pointer.
- `Archive`: Input archive.
- `Context`: Serialization context.

**Returns:** Success or error.
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
