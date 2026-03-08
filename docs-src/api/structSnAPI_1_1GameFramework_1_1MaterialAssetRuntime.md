# SnAPI::GameFramework::MaterialAssetRuntime

Runtime representation of a base material asset.

This is the resolved form consumed by renderer/material binding helpers rather than the on-disk serialized payload object.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::MaterialAssetRuntime::ShaderModule`

Renderer shader module name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::MaterialAssetRuntime::ShadingModel`

Renderer shading-model name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureAlbedoMap`

Enables albedo texture features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureNormalMap`

Enables normal texture features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureRoughnessMap`

Enables roughness texture features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureMetalnessMap`

Enables metalness texture features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureOcclusionMap`

Enables occlusion texture features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureAlphaTest`

Enables alpha-test rendering behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureAlphaBlend`

Enables alpha-blend rendering behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureDoubleSided`

Enables double-sided rendering when supported.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::FeatureInstancing`

Enables per-instance data support.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialAssetRuntime::bLegacyInferFeaturesFromTextures`

Compatibility flag for older material assets that infer features from bound textures.
</div>
