# SnAPI::GameFramework::WorldSerializer

Serializer for World to/from WorldPayload.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::WorldSerializer::kSchemaVersion`

Current schema version for World payloads.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< WorldPayload > SnAPI::GameFramework::WorldSerializer::Serialize(const World &WorldRef)`

Serialize a world to a payload.

**Parameters**

- `WorldRef`: Source world.

**Returns:** Payload or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::WorldSerializer::Deserialize(const WorldPayload &Payload, World &WorldRef)`

Deserialize a world from a payload.

**Parameters**

- `Payload`: Payload to read.
- `WorldRef`: Destination world.

**Returns:** Success or error.
</div>
