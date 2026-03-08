# SnAPI::GameFramework::MaterialInstancePayload

Cooked payload for a material-instance asset.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::MaterialInstancePayload::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `AssetRefPayload SnAPI::GameFramework::MaterialInstancePayload::ParentMaterial`

Referenced parent base material asset.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MaterialScalarParamPayload> SnAPI::GameFramework::MaterialInstancePayload::Scalars`

Scalar parameter overrides.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MaterialVectorParamPayload> SnAPI::GameFramework::MaterialInstancePayload::Vectors`

Vector parameter overrides.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MaterialTextureParamPayload> SnAPI::GameFramework::MaterialInstancePayload::Textures`

Texture parameter overrides.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MaterialInstancePayload::operator==(const MaterialInstancePayload &) const =default`
</div>
