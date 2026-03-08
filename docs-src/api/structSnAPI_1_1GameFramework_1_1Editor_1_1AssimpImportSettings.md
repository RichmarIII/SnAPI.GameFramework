# SnAPI::GameFramework::Editor::AssimpImportSettings

Reflected settings for Assimp-driven model imports.

These settings are stored as editor-facing import metadata and translated into the concrete Assimp importer configuration used during import or reimport. They do not directly mutate already cooked assets; changes take effect on the next import or reimport.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::Editor::AssimpImportSettings::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::GenerateNormals`

Generate missing normals during import when source data does not provide them.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::GenerateTangents`

Generate tangent frames needed for normal mapping workflows.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::FlipUVs`

Flip imported UVs vertically before cooking.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::OptimizeMeshes`

Allow the importer to merge or optimize mesh data for runtime use.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::ForceSkeletal`

Treat the import as skeletal even when source heuristics would not.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::ForceStatic`

Treat the import as static even when source heuristics would suggest a skeletal asset.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::ImportMaterials`

Create or import material assets referenced by the source file.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::ImportTextures`

Create or import texture assets referenced by the source file.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::ImportAnimations`

Import animation clips when present in the source file.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::AssimpImportSettings::ImportSkeleton`

Import skeleton or bone hierarchy data when present.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::AssimpImportSettings::MaxBonesPerVertex`

Maximum bone influences kept per vertex after import truncation.
</div>
