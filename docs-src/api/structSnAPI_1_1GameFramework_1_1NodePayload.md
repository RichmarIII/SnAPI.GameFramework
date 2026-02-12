# SnAPI::GameFramework::NodePayload

Serialized node data within a graph.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::NodePayload::NodeId`

Node UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::NodePayload::NodeType`

Node type id.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::NodePayload::NodeTypeName`

Type name fallback when TypeId is missing.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::NodePayload::Name`

Node name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodePayload::Active`

Active state.
</div>
<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::NodePayload::ParentId`

Parent node UUID (nil if root).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodePayload::HasNodeData`

True if node fields were serialized.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::NodePayload::NodeBytes`

Serialized node field bytes.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeComponentPayload> SnAPI::GameFramework::NodePayload::Components`

Component payloads.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodePayload::HasGraph`

True if node contains a nested graph.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::NodePayload::GraphBytes`

Serialized nested graph bytes.
</div>
