# SnAPI::GameFramework::TValueCodec< TAssetRef< TBase, TNameTag > >

`TValueCodec` specialization for asset references.

Asset references serialize by logical asset identity rather than by any loaded runtime object state. The wire format stores both asset name and asset id so loaders can choose whichever identifier is most useful in the current environment.

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< TAssetRef< TBase, TNameTag > >::Encode(const TAssetRef< TBase, TNameTag > &Value, cereal::BinaryOutputArchive &Archive, const TSerializationContext &)`

Serialize asset reference identity fields into the archive.

**Parameters**

- `Value`: 
- `Archive`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< TAssetRef< TBase, TNameTag > > SnAPI::GameFramework::TValueCodec< TAssetRef< TBase, TNameTag > >::Decode(cereal::BinaryInputArchive &Archive, const TSerializationContext &)`

Decode an asset reference from serialized asset name and asset id fields.

**Parameters**

- `Archive`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::TValueCodec< TAssetRef< TBase, TNameTag > >::DecodeInto(TAssetRef< TBase, TNameTag > &Value, cereal::BinaryInputArchive &Archive, const TSerializationContext &)`

Decode an asset reference directly into an existing `TAssetRef` instance.

**Parameters**

- `Value`: 
- `Archive`:
</div>
