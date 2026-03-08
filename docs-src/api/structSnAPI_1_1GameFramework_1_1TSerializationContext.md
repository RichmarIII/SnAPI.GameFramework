# SnAPI::GameFramework::TSerializationContext

Shared context propagated through value codecs and reflection serializers.

`TSerializationContext` carries the ambient lookup state needed to turn raw UUID-backed payload data into live framework objects. It exists so low-level codecs can resolve `NodeHandle`, `ComponentHandle`, and other graph-relative values without hard-coding one global resolution path.

Core semantics:
- `World` is the primary runtime lookup surface for live handles.
- `Graph` provides an optional level-scoped fallback when a serializer is operating against a `Level`.
- `NodeIdRemap` and `ComponentIdRemap` rewrite serialized source/template ids to fresh runtime ids when deserialization is configured to regenerate object identity.
- `UseLegacyFloatVectorDecode` enables compatibility decoding for older payloads that stored `Vec3` and `Quat` scalars as `float` even when the runtime scalar type is wider.

Ownership and lifetime:
- All pointers are borrowed.
- The pointed-to objects and maps must outlive the encode/decode call using the context.

## Public Members

<div class="snapi-api-card" markdown="1">
### `const IWorld* SnAPI::GameFramework::TSerializationContext::World`

Borrowed World used as the primary runtime lookup surface for Node and Component handles during decode.
</div>
<div class="snapi-api-card" markdown="1">
### `const Level* SnAPI::GameFramework::TSerializationContext::Graph`

Optional borrowed Level used as a secondary graph-local lookup surface when a World lookup is unavailable or insufficient.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::unordered_map<Uuid, Uuid, UuidHash>* SnAPI::GameFramework::TSerializationContext::NodeIdRemap`

Optional borrowed source-node-id to runtime-node-id remap applied before handle resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::unordered_map<Uuid, Uuid, UuidHash>* SnAPI::GameFramework::TSerializationContext::ComponentIdRemap`

Optional borrowed source-component-id to runtime-component-id remap applied before handle resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TSerializationContext::UseLegacyFloatVectorDecode`

Compatibility flag enabling legacy float32 decode for vector and quaternion payloads when the runtime scalar type is wider.
</div>
