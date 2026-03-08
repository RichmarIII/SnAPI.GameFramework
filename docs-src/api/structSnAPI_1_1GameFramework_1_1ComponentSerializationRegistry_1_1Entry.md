# SnAPI::GameFramework::ComponentSerializationRegistry::Entry

Registry entry bundling create, serialize, deserialize, and deferred-lifecycle callbacks for one Component type.

## Public Members

<div class="snapi-api-card" markdown="1">
### `CreateFn SnAPI::GameFramework::ComponentSerializationRegistry::Entry::Create`

Creation callback for the registered Component type.
</div>
<div class="snapi-api-card" markdown="1">
### `CreateWithIdFn SnAPI::GameFramework::ComponentSerializationRegistry::Entry::CreateWithId`

Creation callback that preserves an explicit Component UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `SerializeFn SnAPI::GameFramework::ComponentSerializationRegistry::Entry::Serialize`

Serializer that emits the Component's raw payload bytes.
</div>
<div class="snapi-api-card" markdown="1">
### `DeserializeFn SnAPI::GameFramework::ComponentSerializationRegistry::Entry::Deserialize`

Deserializer that populates an existing Component from raw payload bytes.
</div>
<div class="snapi-api-card" markdown="1">
### `OnCreateFn SnAPI::GameFramework::ComponentSerializationRegistry::Entry::OnCreate`

Deferred lifecycle callback invoked after deserialize has populated the Component's fields.
</div>
