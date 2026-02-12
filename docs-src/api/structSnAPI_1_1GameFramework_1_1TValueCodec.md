# SnAPI::GameFramework::TValueCodec

Customization point for value serialization.

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
