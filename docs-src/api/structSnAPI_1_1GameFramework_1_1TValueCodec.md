# SnAPI::GameFramework::TValueCodec

Customization point for value serialization.

Default behavior includes:
- common framework scalar/value types
- UUID/handle binary encoding
- trivially-copyable fallback for POD-like values

For high-performance hot paths, prefer custom codecs to avoid generic reflection walk costs.

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< T >::Encode(const T &Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context)`

**Parameters**

- `Value`: 
- `Archive`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< T > SnAPI::GameFramework::TValueCodec< T >::Decode(cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

**Parameters**

- `Archive`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< T >::DecodeInto(T &Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

**Parameters**

- `Value`: 
- `Archive`: 
- `Context`:
</div>
