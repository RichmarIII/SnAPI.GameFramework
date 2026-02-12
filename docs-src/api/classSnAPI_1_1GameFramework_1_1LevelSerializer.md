# SnAPI::GameFramework::LevelSerializer

Serializer for Level to/from LevelPayload.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::LevelSerializer::kSchemaVersion`

Current schema version for Level payloads.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< LevelPayload > SnAPI::GameFramework::LevelSerializer::Serialize(const Level &LevelRef)`

Serialize a level to a payload.

**Parameters**

- `LevelRef`: Source level.

**Returns:** Payload or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::LevelSerializer::Deserialize(const LevelPayload &Payload, Level &LevelRef)`

Deserialize a level from a payload.

**Parameters**

- `Payload`: Payload to read.
- `LevelRef`: Destination level.

**Returns:** Success or error.
</div>
