# SnAPI::GameFramework::WorldSerializer

Converts one live World to and from `WorldPayload`.

`WorldSerializer` is the top-level graph serializer. It captures the World's effective root Nodes and delegates subtree work to `NodeSerializer`. During deserialization it clears the destination World before recreating the payload roots.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::WorldSerializer::kSchemaVersion`

Current schema version for `WorldPayload`.

Consumers can use this for out-of-band format compatibility checks.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< WorldPayload > SnAPI::GameFramework::WorldSerializer::Serialize(const World &WorldRef)`

Serialize one World into a `WorldPayload`.

**Parameters**

- `WorldRef`: Source World.

**Returns:** Serialized payload on success or an error when any root subtree fails to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::WorldSerializer::Deserialize(const WorldPayload &Payload, World &WorldRef, const TDeserializeOptions &Options={})`

Replace a World's current contents with a serialized payload.

**Parameters**

- `Payload`: Serialized World payload to load.
- `WorldRef`: Destination World to overwrite.
- `Options`: Identity-remap behavior applied during decode.

**Returns:** `Ok()` on success or an error when root recreation or subtree deserialization fails.
</div>
