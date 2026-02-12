# SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodeGraphPayloadSerializer

AssetPipeline serializer for NodeGraphPayload.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodeGraphPayloadSerializer::GetTypeId() const override`

Get the payload type id.

**Returns:** Payload type id for NodeGraph.
</div>
<div class="snapi-api-card" markdown="1">
### `const char * SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodeGraphPayloadSerializer::GetTypeName() const override`

Get the payload type name.

**Returns:** Payload type name string.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodeGraphPayloadSerializer::GetSchemaVersion() const override`

Get the payload schema version.

**Returns:** Schema version for NodeGraph payloads.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodeGraphPayloadSerializer::SerializeToBytes(const void *Object, std::vector< uint8_t > &OutBytes) const override`

Serialize a NodeGraphPayload into bytes.

**Parameters**

- `Object`: Pointer to NodeGraphPayload.
- `OutBytes`: Output byte buffer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}::NodeGraphPayloadSerializer::DeserializeFromBytes(void *Object, const uint8_t *Bytes, std::size_t Size) const override`

Deserialize a NodeGraphPayload from bytes.

**Parameters**

- `Object`: Pointer to destination payload.
- `Bytes`: Byte buffer.
- `Size`: Byte count.

**Returns:** True on success.
</div>
