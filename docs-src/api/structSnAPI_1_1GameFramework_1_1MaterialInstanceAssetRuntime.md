# SnAPI::GameFramework::MaterialInstanceAssetRuntime

Runtime representation of a material-instance asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `TAssetRef<MaterialAssetRuntime> SnAPI::GameFramework::MaterialInstanceAssetRuntime::ParentMaterial`

Referenced parent base material.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MaterialScalarParamPayload> SnAPI::GameFramework::MaterialInstanceAssetRuntime::Scalars`

Scalar parameter overrides.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MaterialVectorParamPayload> SnAPI::GameFramework::MaterialInstanceAssetRuntime::Vectors`

Vector parameter overrides.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::string> SnAPI::GameFramework::MaterialInstanceAssetRuntime::TextureSlots`

Texture-slot names paired with `Textures` by index.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TAssetRef<RuntimeTextureAsset> > SnAPI::GameFramework::MaterialInstanceAssetRuntime::Textures`

Texture references paired with `TextureSlots` by index.
</div>
