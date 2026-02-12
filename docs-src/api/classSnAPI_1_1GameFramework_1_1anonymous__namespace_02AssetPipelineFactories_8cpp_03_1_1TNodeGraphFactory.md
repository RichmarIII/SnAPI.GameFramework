# SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TNodeGraphFactory

AssetFactory for NodeGraph runtime objects.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TNodeGraphFactory::GetCookedPayloadType() const override`

Get the cooked payload type handled by this factory.

**Returns:** Payload TypeId.
</div>

## Protected Functions

<div class="snapi-api-card" markdown="1">
### `std::expected< NodeGraph, std::string > SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TNodeGraphFactory::DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext &Context) override`

Load a NodeGraph from cooked data.

**Parameters**

- `Context`: Asset load context.

**Returns:** Loaded NodeGraph or error.
</div>
