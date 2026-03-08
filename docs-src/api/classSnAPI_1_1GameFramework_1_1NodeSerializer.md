# SnAPI::GameFramework::NodeSerializer

Converts one Node subtree between live objects and `NodePayload`.

`NodeSerializer` is the core graph serializer. It preserves subtree hierarchy, Node and Component identity, and reflected field data while remaining agnostic to the specific gameplay types present in the tree.

Core semantics:
- Serialization skips editor-transient child Nodes.
- Deserialization creates the full Node tree first and applies fields in a second pass.
- Component creation is separated from Component `OnCreate`, which is explicitly deferred until deserialization has populated the Component fields.
- When `RegenerateObjectIds` is enabled, internal Node and Component handles are rewritten through remap tables built from the payload before any objects are created.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::NodeSerializer::kSchemaVersion`

Current schema version for `NodePayload`.

Consumers can use this for out-of-band format compatibility checks.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< NodePayload > SnAPI::GameFramework::NodeSerializer::Serialize(const BaseNode &NodeRef)`

Serialize one live Node subtree into a `NodePayload`.

**Parameters**

- `NodeRef`: Source Node whose subtree should be captured.

**Returns:** Serialized payload on success or an error when the subtree contains unresolved children or Components, or when a field serializer fails.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::NodeSerializer::Deserialize(const NodePayload &Payload, IWorld &WorldRef, const NodeHandle &Parent={}, const TDeserializeOptions &Options={})`

Deserialize one Node subtree into a World.

**Parameters**

- `Payload`: Serialized subtree to materialize.
- `WorldRef`: Destination World used for Node and Component creation.
- `Parent`: Optional parent handle to attach the created root under. A null handle means "spawn as a World root".
- `Options`: Identity-remap behavior applied during decode.

**Returns:** Handle to the created root Node on success or an error when type resolution, creation, field deserialization, or attachment fails.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::NodeSerializer::Deserialize(const NodePayload &Payload, Level &LevelRef, const NodeHandle &Parent={}, const TDeserializeOptions &Options={})`

Deserialize one Node subtree into a Level context.

**Parameters**

- `Payload`: Serialized subtree to materialize.
- `LevelRef`: Destination Level. The Level must already be bound to a World.
- `Parent`: Optional parent handle to attach the created root under. A null handle means "attach under the Level handle when available, otherwise as a level
       root".
- `Options`: Identity-remap behavior applied during decode.

**Returns:** Handle to the created root Node on success or an error when the Level is not bound to a World, or when creation/deserialization fails.
</div>
