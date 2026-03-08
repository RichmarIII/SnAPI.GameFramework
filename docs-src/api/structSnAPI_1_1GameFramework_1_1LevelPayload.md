# SnAPI::GameFramework::LevelPayload

Serialized envelope for one Level's root-node set.

`LevelPayload` stores the Level name plus the serialized subtrees rooted directly under the Level. Existing destination Level contents are destroyed before deserialization populates the new roots.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::LevelPayload::Name`

Serialized Level name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodePayload> SnAPI::GameFramework::LevelPayload::Nodes`

Serialized payloads for the Level's effective root Nodes.
</div>
