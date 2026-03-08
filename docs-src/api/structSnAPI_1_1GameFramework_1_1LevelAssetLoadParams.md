# SnAPI::GameFramework::LevelAssetLoadParams

Runtime load parameters for level assets.

When `TargetWorld` is supplied, the level asset factory creates a new level in that world and deserializes the cooked payload into it.

## Public Members

<div class="snapi-api-card" markdown="1">
### `World* SnAPI::GameFramework::LevelAssetLoadParams::TargetWorld`

Borrowed destination world that will receive a newly created level, or `nullptr` for detached loads.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::LevelAssetLoadParams::NameOverride`

Optional replacement name for the created level.

Empty keeps the payload or catalog name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::LevelAssetLoadParams::InstantiateAsCopy`

When true, regenerate node/component UUIDs during load to avoid collisions.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle* SnAPI::GameFramework::LevelAssetLoadParams::OutCreatedLevel`

Optional out-pointer receiving the created level handle when deserialized into a world.
</div>
