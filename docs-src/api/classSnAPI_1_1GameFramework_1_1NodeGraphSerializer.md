# SnAPI::GameFramework::NodeGraphSerializer

Serializer for NodeGraph to/from NodeGraphPayload.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::NodeGraphSerializer::kSchemaVersion`

Current schema version for NodeGraph payloads.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< NodeGraphPayload > SnAPI::GameFramework::NodeGraphSerializer::Serialize(const NodeGraph &Graph)`

Serialize a graph to a payload.

**Parameters**

- `Graph`: Source graph.

**Returns:** Payload or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::NodeGraphSerializer::Deserialize(const NodeGraphPayload &Payload, NodeGraph &Graph)`

Deserialize a graph from a payload.

**Parameters**

- `Payload`: Payload to read.
- `Graph`: Destination graph.

**Returns:** Success or error.
</div>
