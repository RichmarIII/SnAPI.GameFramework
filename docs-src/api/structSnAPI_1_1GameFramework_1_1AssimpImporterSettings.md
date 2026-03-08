# SnAPI::GameFramework::AssimpImporterSettings

Typed importer settings for Assimp-driven mesh and animation imports.

`AssimpImporterSettings` is the reflected settings object used by editor import flows and any import path that wants a strongly typed replacement for stringly typed build options.

Ownership and lifetime:
- Callers typically pass this through `IAssetImportSettings` ownership channels.
- `Clone()` returns a deep copy suitable for persistence by the asset pipeline.

## Public Members

<div class="snapi-api-card" markdown="1">
### `MeshImportSettingsPayload SnAPI::GameFramework::AssimpImporterSettings::Mesh`

Mesh-import policy flags forwarded into the source payload.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::AssimpImporterSettings::LogicalNameOverride`

Optional asset display-name override applied during import.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::AssimpImporterSettings::DefaultShaderModule`

Shader module used when imported materials need a default runtime material.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::AssimpImporterSettings::DefaultShadingModel`

Shading model used when imported materials need a default runtime material.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetImportSettings > SnAPI::GameFramework::AssimpImporterSettings::Clone() const override`

Deep-clone this settings object through the `IAssetImportSettings` interface.
</div>
