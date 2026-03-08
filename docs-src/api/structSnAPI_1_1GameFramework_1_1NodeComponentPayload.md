# SnAPI::GameFramework::NodeComponentPayload

Serialized representation of one Component attached to a Node.

This is the smallest Component-level payload unit carried inside a `NodePayload`. `Bytes` contains the Component's type-specific field data; construction policy and ownership still come from the surrounding Node and World deserializer.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::NodeComponentPayload::ComponentId`

Serialized Component UUID.

May be remapped during deserialization when `RegenerateObjectIds` is enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::NodeComponentPayload::ComponentType`

Reflected concrete Component type used to recreate and deserialize the instance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::NodeComponentPayload::Bytes`

Raw serialized Component payload bytes produced by `ComponentSerializationRegistry`.
</div>
