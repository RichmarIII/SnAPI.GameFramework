# SnAPI::GameFramework::NodeAssetLoadParams

Runtime load parameters for node assets.

`NodeAssetLoadParams` controls whether a node asset is returned as an unattached heap object or is instantiated directly into a live world. Asset factories inspect this payload through the generic `AssetLoadContext::Params` channel.

Core semantics:
- When `TargetWorld` is null, the node asset factory returns a detached runtime object.
- When `TargetWorld` is non-null, deserialization materializes the node subtree directly into that world under `Parent` or the world root.
- `InstantiateAsCopy` controls UUID regeneration during deserialization and should be left enabled for most editor placement and duplication paths.

## Public Members

<div class="snapi-api-card" markdown="1">
### `IWorld* SnAPI::GameFramework::NodeAssetLoadParams::TargetWorld`

Borrowed target world that will receive the instantiated node subtree, or `nullptr` for detached loads.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::NodeAssetLoadParams::Parent`

Parent node under which the created root should be attached.

A null handle means the world root.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodeAssetLoadParams::InstantiateAsCopy`

When true, regenerate node/component UUIDs during load to avoid collisions.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle* SnAPI::GameFramework::NodeAssetLoadParams::OutCreatedRoot`

Optional out-pointer receiving the created node handle when instantiated into a world.
</div>
