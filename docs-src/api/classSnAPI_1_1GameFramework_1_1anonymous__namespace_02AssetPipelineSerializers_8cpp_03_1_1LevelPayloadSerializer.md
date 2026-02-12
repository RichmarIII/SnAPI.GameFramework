# SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::LevelPayloadSerializer

AssetPipeline serializer for LevelPayload.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::LevelPayloadSerializer::GetTypeId() const override`

Get the payload type id.
</div>
<div class="snapi-api-card" markdown="1">
### `const char * SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::LevelPayloadSerializer::GetTypeName() const override`

Get the payload type name.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::LevelPayloadSerializer::GetSchemaVersion() const override`

Get the payload schema version.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::LevelPayloadSerializer::SerializeToBytes(const void *Object, std::vector< uint8_t > &OutBytes) const override`

Serialize a LevelPayload into bytes.

**Parameters**

- `Object`: Pointer to LevelPayload.
- `OutBytes`: Output byte buffer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::LevelPayloadSerializer::DeserializeFromBytes(void *Object, const uint8_t *Bytes, std::size_t Size) const override`

Deserialize a LevelPayload from bytes.

**Parameters**

- `Object`: Pointer to destination payload.
- `Bytes`: Byte buffer.
- `Size`: Byte count.

**Returns:** True on success.
</div>
