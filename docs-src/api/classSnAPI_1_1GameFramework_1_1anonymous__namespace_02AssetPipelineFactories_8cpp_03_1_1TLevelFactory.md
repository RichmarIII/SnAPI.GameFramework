# SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TLevelFactory

AssetFactory for Level runtime objects.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TLevelFactory::GetCookedPayloadType() const override`

Get the cooked payload type handled by this factory.
</div>

## Protected Functions

<div class="snapi-api-card" markdown="1">
### `std::expected< Level, std::string > SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}::TLevelFactory::DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext &Context) override`

Load a Level from cooked data.

**Parameters**

- `Context`: Asset load context.

**Returns:** Loaded Level or error.
</div>
