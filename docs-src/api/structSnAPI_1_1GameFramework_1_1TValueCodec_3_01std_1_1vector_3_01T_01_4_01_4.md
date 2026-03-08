# SnAPI::GameFramework::TValueCodec< std::vector< T > >

`TValueCodec` specialization for vectors of codec-supported element types.

The vector wire format stores a 64-bit element count followed by each element encoded through `TValueCodec<T>`.

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< std::vector< T > >::Encode(const std::vector< T > &Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &Context)`

Encode a vector length followed by each element in order.

**Parameters**

- `Value`: 
- `Archive`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< std::vector< T > > SnAPI::GameFramework::TValueCodec< std::vector< T > >::Decode(cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Decode a vector by reading its stored element count and each serialized element.

**Parameters**

- `Archive`: 
- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< std::vector< T > >::DecodeInto(std::vector< T > &Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &Context)`

Replace an existing vector with decoded contents from the archive.

**Parameters**

- `Value`: 
- `Archive`: 
- `Context`:
</div>
