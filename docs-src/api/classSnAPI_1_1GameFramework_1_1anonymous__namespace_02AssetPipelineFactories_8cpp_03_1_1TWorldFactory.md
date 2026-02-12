# SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TWorldFactory

AssetFactory for World runtime objects.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TWorldFactory::GetCookedPayloadType() const override`

Get the cooked payload type handled by this factory.
</div>

## Protected Functions

<div class="snapi-api-card" markdown="1">
### `std::expected< World, std::string > SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TWorldFactory::DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext &Context) override`

Load a World from cooked data.

**Parameters**

- `Context`: Asset load context.

**Returns:** Loaded World or error.
</div>
