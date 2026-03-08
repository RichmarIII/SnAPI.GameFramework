# SnAPI::GameFramework::TValueCodec

Compile-time customization point for serializing one value type.

`TValueCodec<T>` defines how a concrete C++ value is encoded into a binary archive and decoded back out. `ValueCodecRegistry` binds these compile-time functions to runtime `TypeId`s so reflection-based systems can serialize arbitrary reflected fields through a common dispatch path.

Default behavior covers:
- `std::string`
- `std::vector<uint8_t>`
- `Uuid`
- `Vec3`
- `Quat`
- `NodeHandle`
- `ComponentHandle`
- trivially copyable types as raw binary blobs

Handle semantics during decode:
- `NodeHandle` first applies `NodeIdRemap`, then attempts runtime resolution through `World`, then `Graph`, then the global `ObjectRegistry`, and finally falls back to a UUID-only handle when no live object can be resolved.
- `ComponentHandle` first applies `ComponentIdRemap`, then attempts `ObjectRegistry` resolution, and finally falls back to a UUID-only handle.

Specialize this template when a type needs:
- versioned or packed wire storage
- custom pointer or asset resolution rules
- a format that is more stable than raw memory layout

Threading:
- The codec itself is stateless, but it may consult objects referenced by `TSerializationContext`. Any required synchronization is the caller's responsibility.

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< T >::Encode(const T &Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context)`

Encode one value into a cereal binary archive.

**Parameters**

- `Value`: Value to serialize.
- `Archive`: Destination archive.
- `Context`: Borrowed serialization context used for handle-aware codecs.

**Returns:** `Ok()` on success or an error when the type has no supported default codec.
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< T > SnAPI::GameFramework::TValueCodec< T >::Decode(cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Decode one value from a cereal binary archive.

**Parameters**

- `Archive`: Source archive positioned at the value payload.
- `Context`: Borrowed serialization context used for handle remap and lookup.

**Returns:** Decoded value on success or an error when the type has no supported default codec.
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< T >::DecodeInto(T &Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Decode one value directly into existing storage.

**Parameters**

- `Value`: Destination object to overwrite.
- `Archive`: Source archive positioned at the value payload.
- `Context`: Borrowed serialization context used for handle remap and lookup.

**Returns:** `Ok()` on success or an error when the type has no supported default codec.
</div>
