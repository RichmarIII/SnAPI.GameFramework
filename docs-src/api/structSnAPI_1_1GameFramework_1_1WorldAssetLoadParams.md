# SnAPI::GameFramework::WorldAssetLoadParams

Runtime load parameters for world assets.

World payloads can either be loaded as detached runtime objects or deserialized directly into an existing destination world.

## Public Members

<div class="snapi-api-card" markdown="1">
### `World* SnAPI::GameFramework::WorldAssetLoadParams::TargetWorld`

Borrowed destination world that will receive the payload contents, or `nullptr` for detached loads.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldAssetLoadParams::InstantiateAsCopy`

When true, regenerate node/component UUIDs during load to avoid collisions.
</div>
