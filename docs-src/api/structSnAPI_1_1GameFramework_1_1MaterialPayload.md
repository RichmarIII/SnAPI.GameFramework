# SnAPI::GameFramework::MaterialPayload

Cooked payload describing a base material contract.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::MaterialPayload::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::MaterialPayload::ShaderModule`

Renderer shader module name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::MaterialPayload::ShadingModel`

Renderer shading-model name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureAlbedoMap`

Enables albedo-map sampling features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureNormalMap`

Enables normal-map sampling features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureRoughnessMap`

Enables roughness-map sampling features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureMetalnessMap`

Enables metalness-map sampling features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureOcclusionMap`

Enables occlusion-map sampling features.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureAlphaTest`

Enables alpha-test behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureAlphaBlend`

Enables alpha-blend behavior.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureDoubleSided`

Disables backface culling when supported.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::FeatureInstancing`

Enables per-instance data support.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialPayload::operator==(const MaterialPayload &) const =default`
</div>
