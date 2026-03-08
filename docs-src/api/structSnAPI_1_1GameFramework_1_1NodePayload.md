# SnAPI::GameFramework::NodePayload

Recursive serialized representation of one Node subtree.

A `NodePayload` carries everything needed to recreate one Node and its descendants:
- Node identity and reflected type
- name and active state
- optional reflected field bytes for the Node itself
- serialized attached Components
- recursive child payloads

Deserialization semantics:
- The subtree structure is created first.
- Node field bytes and Component payloads are then applied in a second pass.
- Deferred node and component `OnCreate` hooks are requested only after field population.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::NodePayload::NodeId`

Serialized Node UUID.

May be remapped during deserialization when `RegenerateObjectIds` is enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::NodePayload::NodeType`

Reflected concrete Node type id.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::NodePayload::NodeTypeName`

Reflected type name fallback used when `NodeType` is missing or unresolved in the receiving runtime.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::NodePayload::Name`

Node name to assign after creation.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodePayload::Active`

Serialized active-state flag restored after the Node is created.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodePayload::HasNodeData`

Indicates whether `NodeBytes` contains serialized reflected Node fields.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::NodePayload::NodeBytes`

Raw serialized Node field bytes emitted by reflection-based field walking.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeComponentPayload> SnAPI::GameFramework::NodePayload::Components`

Serialized Components that should be attached to this Node.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodePayload> SnAPI::GameFramework::NodePayload::Children`

Serialized child subtrees.

Editor-transient children are intentionally omitted from serializer output.
</div>
