# SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodePayloadSerializer

AssetPipeline serializer for NodePayload.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodePayloadSerializer::GetTypeId() const override`

Get the payload type id.

**Returns:** Payload type id for Level.
</div>
<div class="snapi-api-card" markdown="1">
### `const char * SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodePayloadSerializer::GetTypeName() const override`

Get the payload type name.

**Returns:** Payload type name string.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodePayloadSerializer::GetSchemaVersion() const override`

Get the payload schema version.

**Returns:** Schema version for Level payloads.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodePayloadSerializer::SerializeToBytes(const void *Object, std::vector< uint8_t > &OutBytes) const override`

Serialize a NodePayload into bytes.

**Parameters**

- `Object`: Pointer to NodePayload.
- `OutBytes`: Output byte buffer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodePayloadSerializer::DeserializeFromBytes(void *Object, const uint8_t *Bytes, std::size_t Size) const override`

Deserialize a NodePayload from bytes.

**Parameters**

- `Object`: Pointer to destination payload.
- `Bytes`: Byte buffer.
- `Size`: Byte count.

**Returns:** True on success.
</div>
