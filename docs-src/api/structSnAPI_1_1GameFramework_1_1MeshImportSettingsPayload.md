# SnAPI::GameFramework::MeshImportSettingsPayload

Source-import policy flags for mesh assets.

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::GenerateNormals`

Generate normals when the source data does not provide them.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::GenerateTangents`

Generate tangent frames when needed.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::FlipUVs`

Flip V texture coordinates during import.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::OptimizeMeshes`

Run mesh optimization passes when supported by the importer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::ForceSkeletal`

Force skeletal import even if the source could be treated as static.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::ForceStatic`

Force static import even if the source contains skeletal data.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::ImportMaterials`

Import source materials.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::ImportTextures`

Import referenced textures.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::ImportAnimations`

Import animation clips.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshImportSettingsPayload::ImportSkeleton`

Import or generate a skeleton asset.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::MeshImportSettingsPayload::MaxBonesPerVertex`

Maximum number of bone influences retained per vertex.
</div>
