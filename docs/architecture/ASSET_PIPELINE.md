# Asset Pipeline

Read this when:

- changing authored assets, source assets, cooked payloads, importers,
  serializers, AssetPipeline factories, or runtime asset loading
- changing render asset source/cooked data flow

Related context:

- `../ARCHITECTURE.md`
- `RENDERING.md`
- `PROJECTS_BUILD_AND_PACKAGING.md`

## Core Model

GameFramework separates source-authored assets from cooked runtime payloads.

- Source assets are editor/authored documents.
- Cooked assets are final runtime payloads consumed by `Load<>` and `Get<>`.
- Importers translate external files or source JSON into source/cook inputs.
- Cookers produce runtime payloads and metadata for the active build profile.
- Runtime asset types should stay focused on game-ready data.

Do not collapse source asset shape and cooked runtime shape into one type just
because old renderer or old asset APIs made that convenient.

## Ownership Boundaries

GameFramework owns the gameplay/editor asset contracts and the integration with
SnAPI.AssetPipeline. SnAPI.AssetPipeline owns generic source discovery, import,
cook, serialization, plugin, and cache infrastructure.

Renderer.New owns renderer-native mesh, material, texture, text, and render
resource contracts. GameFramework owns the mapping from GameFramework authored
render assets to Renderer.New runtime inputs.

## Current Important Areas

- authored asset registry and source JSON loading
- render asset source payloads and runtime payloads
- material, material instance, static mesh, skeletal mesh, skeleton, animation,
  and texture payload serializers
- Assimp import integration
- AssetPipeline plugin/factory registration
- editor asset service and icon/import metadata flow

## Target Module Layout

Asset contracts and payloads should move under a GameFramework module `Public/`
root. Importer/cooker/serializer implementation should move under `Private/` or
an optional import module. Module-local dependency files should own Assimp,
TextureCompressor, cereal, and AssetPipeline dependency application.

## Validation Expectations

Asset changes need focused serialization/import/cook tests where possible. When
render asset payloads change, validate both AssetPipeline output and the
Renderer.New-facing runtime consumption path.
