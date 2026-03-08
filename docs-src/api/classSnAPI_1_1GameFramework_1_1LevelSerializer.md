# SnAPI::GameFramework::LevelSerializer

Converts one live Level to and from `LevelPayload`.

`LevelSerializer` treats a Level as an envelope around its effective root Nodes. During serialization it collects those roots and delegates each subtree to `NodeSerializer`. During deserialization it destroys existing Level children, flushes the World once, and then recreates the payload's roots.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::LevelSerializer::kSchemaVersion`

Current schema version for `LevelPayload`.

Consumers can use this for out-of-band format compatibility checks.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< LevelPayload > SnAPI::GameFramework::LevelSerializer::Serialize(const Level &LevelRef)`

Serialize one Level into a `LevelPayload`.

**Parameters**

- `LevelRef`: Source Level.

**Returns:** Serialized payload on success or an error when any root subtree fails to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::LevelSerializer::Deserialize(const LevelPayload &Payload, Level &LevelRef, const TDeserializeOptions &Options={})`

Replace a Level's current contents with a serialized payload.

**Parameters**

- `Payload`: Serialized Level payload to load.
- `LevelRef`: Destination Level to overwrite.
- `Options`: Identity-remap behavior applied during decode.

**Returns:** `Ok()` on success or an error when the Level is not bound to a World, when existing children cannot be destroyed, or when any root subtree fails to deserialize.
</div>
