# SnAPI::GameFramework::WorldPayload

Serialized envelope for one World's root-node set.

`WorldPayload` stores the World name plus the serialized root Node subtrees. Existing destination World contents are cleared before deserialization populates the new roots.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::WorldPayload::Name`

Serialized World name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodePayload> SnAPI::GameFramework::WorldPayload::Nodes`

Serialized payloads for the World's root Nodes.
</div>
