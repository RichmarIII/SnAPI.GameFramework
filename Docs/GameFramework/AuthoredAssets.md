# Authored Source Assets

## Purpose

This document defines the intended authored-asset model for `SnAPI.GameFramework`.

The short version is:

- editor-authored assets are not runtime pack assets
- editor-authored assets are source assets
- source assets are saved as JSON files
- source assets flow through `import -> cook -> pak -> runtime`
- runtime `Load<>` and `Get<>` continue to operate on final game-ready runtime types

This is the correct model for `Conduit`, future authored materials, and other editor-created
content that should support diffs, merges, and just-in-time cooking for editor preview or PIE.

## Problem Statement

The current editor asset workflow mixes several different concepts:

- authored editor documents such as `Conduit::GraphAsset`
- authored prefab-like content such as `NodePrefabAsset`
- directly consumable runtime assets such as material/mesh/texture runtime payloads
- content-browser create/open/save logic that branches manually by asset family

That creates the wrong pressure:

- content-browser create paths become hardcoded
- new authored asset types require editor switch statements
- authored assets get pushed into the same lane as cooked runtime assets
- git-friendly source content is harder to support cleanly

The framework needs a clear split between:

1. authored source assets
2. runtime object serialization that is not an authored asset
3. cooked runtime assets

## Core Model

### Source Assets

Source assets are editor-authored content documents.

Examples:

- `NodeAsset`
- `LevelAsset`
- `WorldAsset`
- `Conduit::GraphAsset`
- `Conduit::ClassAsset`
- `MaterialAsset`
- `MaterialInstanceAsset`
- future authored mesh/texture import descriptors

Source assets:

- derive from `IAsset`
- are reflected
- are saved as JSON
- live in source-content directories
- are opened and edited directly in the editor
- are imported and cooked into runtime-ready payloads

### Cooked Assets

Cooked assets are runtime-ready payloads.

They are:

- optimized for runtime use
- packable into `.snpak`
- loaded by `AssetManager`
- consumed through `Load<>` and `Get<>`

Cooked assets should already be in the right runtime form.
They are not intended to be editor-authored documents.

### Runtime Object Serialization

Runtime object serialization still exists, but it is not the editor's primary authored-content lane.

Examples:

- transient snapshot/capture formats
- internal binary runtime payloads
- debugger or migration helpers

Prefab, level, and world authoring are now part of the authored source-asset lane through the
`NodeAsset` family rather than being treated as a separate editor serialization model.

## `IAsset`

`IAsset` belongs in `SnAPI::GameFramework`, not `SnAPI.AssetPipeline`.

It is the common authored-source-asset interface.

It should be:

- virtual
- reflected
- cheap to default-construct
- side-effect free to default-construct

Default construction is important because the editor can bootstrap asset metadata by:

1. calling `TypeAutoRegistry::Instance().EnsureAll()`
2. enumerating `TypeRegistry::Instance().Derived(StaticTypeId<IAsset>())`
3. default-constructing each concrete asset type once
4. querying its virtual metadata methods
5. caching the result in a registry

That is acceptable because the number of authored asset types is expected to be small and startup-only.

### Proposed Interface Shape

The exact names can change, but the responsibilities should look like this:

```cpp
namespace SnAPI::GameFramework
{
    class IAsset
    {
    public:
        virtual ~IAsset() = default;

        virtual std::string_view DisplayName() const = 0;
        virtual std::string_view Category() const = 0;
        virtual std::string_view FileExtension() const = 0;

        virtual bool CanCreate() const { return true; }
        virtual bool CanSave() const { return true; }
        virtual bool CanRename() const { return true; }
        virtual bool CanDelete() const { return true; }

        virtual EAssetEditorMode EditorMode() const = 0;

        virtual TExpected<void> Save(std::ostream& Output) const = 0;

        virtual ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const = 0;
        virtual ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const = 0;

        virtual ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const
        {
            return SourceAssetKind();
        }

        virtual ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const
        {
            return SourcePayloadType();
        }
    };
}
```

Important notes:

- `FileExtension()` identifies authored source files quickly without parsing JSON.
- `Save(std::ostream&)` should write the authored source document as JSON through cereal.
- `SourceAssetKind()` / `SourcePayloadType()` describe the authored-source identity.
- `CookedAssetKind()` / `CookedPayloadType()` describe the cooked runtime identity.
- the default cooked identity is the same as the source identity; assets only override it when the
  cooked runtime product differs from the authored/source payload.

## Registry Bootstrap

The framework should build a cached authored-asset registry at startup.

Suggested flow:

1. `TypeAutoRegistry::Instance().EnsureAll()`
2. query `Derived(StaticTypeId<IAsset>())`
3. skip abstract or non-default-constructible types
4. default-construct one instance of each asset type
5. query virtual metadata once
6. store cached metadata and ops in a registry

### Cached Metadata

At minimum, cache:

- authored asset type id
- display name
- category
- file extension
- source asset kind
- source payload type
- cooked asset kind
- cooked payload type
- create/save/rename/delete flags
- editor mode

### Cached Lookups

The registry should expose fast lookup by:

- reflected asset type id
- file extension
- source asset kind
- source payload type
- cooked asset kind
- cooked payload type

It should also expose whether the discovered authored asset set is valid and any diagnostics
produced while building the cache so bad asset definitions can be surfaced without requiring the
editor to guess why a type was skipped.

### Validation

The registry should fail fast on invalid definitions:

- duplicate file extensions
- duplicate source asset kinds
- duplicate source payload types
- duplicate cooked asset kinds when uniqueness is required
- duplicate cooked payload types when uniqueness is required
- missing display name or extension
- missing source or cooked asset/payload identity
- non-reflected asset types
- missing default constructors

In practice, the registry can implement this by collecting diagnostics during bootstrap and
reporting the registry invalid when any such issue is found.

## Source File Format

All `IAsset` source assets should be stored as JSON files.

That gives:

- human-readable source content
- better git diffs
- simpler merges
- easier tooling and search

Even if the actual storage backend is common, each asset type should still expose a distinct
`FileExtension()` so tooling can classify files without parsing the document body.

Examples:

- `.conduitgraph`
- `.conduitclass`
- `.material`
- `.materialinstance`

The extension identifies the authored asset type.
The file contents remain JSON.

## Source Save And Import Contract

Source asset handling should be asymmetric:

- `Save` is strict and deterministic
- `Import` is tolerant and reflection-driven

That is intentional.

### Save

`IAsset::Save(std::ostream&)` should:

- emit JSON
- use cereal JSON archives
- use named fields
- produce stable field names and object structure
- keep source-facing dynamic values structural where needed
  - example: `Conduit::SerializedValue` should save as `Type + Value`, not `Type + Bytes`

This makes source assets:

- readable
- diffable
- easy to classify by extension

For authored assets that are fully cereal-serializable, `Save` can just forward into cereal.

### Import

Import should not be a strict mirror of `Save`.

The importer should:

1. resolve the asset type from `FileExtension()`
2. default-construct the asset
3. parse the JSON document
4. walk reflected fields by name
5. assign only the fields that exist and deserialize successfully
6. ignore unknown JSON fields
7. leave missing fields at their default-constructed values

That gives the source-asset system schema tolerance, which is the main reason to keep source assets
as self-describing JSON instead of opaque binary.

## Tolerant Source Import Contract

This section defines the exact behavior expected from authored source-asset import.

### Missing Fields

If a reflected field is absent from the JSON:

- import does not fail
- the field keeps its default-constructed value

This lets new fields be added to asset types without breaking older source files.

### Unknown Fields

If the JSON contains a field that no longer exists in the reflected asset type:

- import does not fail
- the field is ignored

This lets old source files survive field removals or refactors.

### Field Type Changes

If a field exists by name but cannot be imported into the current reflected field type:

- import should record a field-level diagnostic
- import should leave the target field at its default value
- whole-asset import should continue unless the field is explicitly marked required

Import diagnostics should be collectable by callers such as:

- the authored asset importer
- editor document open/load paths
- validation tools

This keeps source import resilient while still surfacing real migration problems.

### Per-Field Failure Policy

Field-level import failures should be non-fatal by default.

Whole-asset import should fail only when:

- the file cannot be parsed as JSON
- the root document shape is invalid for source-asset import
- required structural metadata is missing
- a type-specific importer explicitly declares the error fatal

### Default Construction Requirement

Because tolerant import relies on filling in what it can, authored asset types must:

- be default-constructible when concrete
- establish meaningful defaults in their constructors

This is not optional.
Without stable defaults, missing-field tolerance produces undefined authored state.

### Renames And Aliases

Field renames should be supported through importer aliases or migration metadata.

Expected behavior:

- importer first checks the canonical reflected field name
- if missing, importer may check a list of legacy aliases
- if an alias matches, the importer loads into the current field

This avoids hard breaks when field names are cleaned up for readability.

### Versioning And Migrations

Source assets should optionally carry a schema version field.

Recommended flow:

1. parse JSON
2. read source schema version if present
3. apply type-specific migration steps if needed
4. run tolerant reflected field import

Migrations are for semantic changes.
Tolerant field import is for structural drift.
They solve different problems and should both exist.

### Deterministic Save After Import

After importing an older or partially mismatched source asset, saving it again should:

- write the current canonical field names
- omit removed legacy fields
- preserve surviving values
- write defaults for fields now present on the type

That gives a natural upgrade path:

- import old source
- tolerate what no longer matches
- save once
- source file is normalized to the current schema

## Import, Cook, Pack, Runtime

`IAsset` source assets should move through the same high-level pipeline as external DCC content.

### Import

For authored source assets, the importer should be simple but tolerant:

- inspect file extension
- resolve the authored asset type from the registry
- default-construct that asset type
- parse JSON
- import reflected fields by name using the tolerant source-import contract

This is still an importer.
It just happens to import from your own source-asset format instead of an external DCC format.

### Cook

The cooker transforms authored source assets into final runtime payloads.

Examples:

- `Conduit::GraphAsset` -> compiled graph payload
- authored material asset -> runtime material payload
- authored mesh source descriptor -> cooked mesh stream payload

The cooker should produce the final game-ready runtime representation.

### Pack

Only cooked runtime payloads belong in packs.

Authored `IAsset` JSON documents do not belong in shipping packs.

They belong in source-content roots and are consumed by the import/cook stages.

### Runtime

Runtime uses `AssetManager`.

`Load<>` and `Get<>` continue to operate on final runtime types.

That means:

- `Load<>` creates a runtime instance/copy
- `Get<>` returns a shared cached runtime object

Neither API should be forced to load editor-authored document objects at gameplay runtime unless
the authored object itself is intentionally the final runtime type.

## `Load<>` And `Get<>`

The runtime semantics remain:

- `Load<T>()` for instance/copy-style runtime creation
- `Get<T>()` for shared cached runtime objects
- logical source names may JIT import/cook/load the final runtime type on first use
- repeated `Get<T>()` calls return the cached shared runtime object
- repeated `Load<T>()` calls create fresh runtime instances

Related non-goal:

- discovery/query APIs such as `FindAsset()` and `FindAssetCatalog()` do not implicitly JIT source assets
- editor source discovery remains an editor-side responsibility

Those APIs should keep targeting final runtime products.

Examples:

- texture runtime objects
- mesh streams
- skeleton/animation runtime payloads
- future `Conduit::CompiledGraph`

This is important because many assets should already be runtime-ready and should not require an
extra authored-document instantiation step at gameplay runtime.

## When Authored And Runtime Types Match

Some assets may be loadable directly as their authored type.

That is allowed.

Examples:

- lightweight authored config assets
- simple reflected document payloads that are already runtime-usable

In those cases:

- source asset type
- cooked payload type
- runtime load type

may all be the same or nearly the same.

The architecture must allow that, but it must not require it.

## When Authored And Runtime Types Differ

This is the expected case for many important assets.

Examples:

- `Conduit::GraphAsset` authored source -> cooked compiled graph payload -> runtime `CompiledGraph`
- authored material graph -> cooked runtime material descriptor
- authored mesh/texture descriptor -> cooked GPU-ready runtime data

This is the primary reason authored assets should be treated as source assets rather than pack payloads.

## Conduit Example

The long-term Conduit path should be:

1. author `Conduit::GraphAsset` as a JSON source asset
2. importer loads that source asset into an authored graph object
3. cooker compiles the graph into a runtime compiled-graph payload
4. that payload is packed
5. gameplay runtime executes `Load<CompiledGraph>()`

That means runtime does not need a separate authoring/build phase.

Editor preview and PIE can still:

- import the authored source
- cook it just-in-time
- load the runtime compiled graph

This is the right model for Conduit because it aligns editor authoring with final runtime execution.

## Prefab Assets And Runtime Serialization

Prefab-like authored node content should be `IAsset`.

That means:

- `NodePrefabAsset : IAsset`
- node prefabs are saved as JSON source assets
- editor content creation/open/save works uniformly across Conduit, materials, and prefabs
- cook/JIT load can still instantiate runtime node graphs from that authored source asset

- content-browser `Create Asset` should be driven by `Derived(IAsset)`
- prefab creation should not be a separate hardcoded editor lane

This still preserves the difference between:

- authored source content
- runtime object serialization used for save/load, duplication, or transient world state

Level/world runtime serialization can continue to exist separately.
Those runtime graph dumps are not automatically authored source assets unless they are wrapped in an
authored `IAsset` document type.

## Editor Workflow

The editor should treat `IAsset` files as source content.

That means:

- create source asset file
- open source asset editor by file extension / asset type
- save source asset back to JSON
- request import/cook for preview, PIE, or export

The editor should not treat authored source assets as runtime-memory cooked assets just to make
editing work.

## Serialization Backend

`SnAPI.GameFramework` currently uses cereal binary archives in its reflected runtime serializer path.

For authored source assets, cereal does support JSON archives via:

- `cereal::JSONOutputArchive`
- `cereal::JSONInputArchive`

using:

```cpp
#include <cereal/archives/json.hpp>
```

So cereal is a valid backend choice for JSON source-asset serialization.

Two practical notes:

- the current GameFramework serializer code is binary-oriented, so source-asset JSON support will
  likely live in a separate source-asset serialization layer instead of reusing the existing binary
  payload path directly
- if stable diffs become important, output conventions should be standardized so formatting and field
  naming stay consistent across saves

## Recommended Registry Split

The framework should eventually have two distinct registries:

### 1. Authored Asset Registry

GameFramework-owned.

Responsibilities:

- enumerate `IAsset` source asset types
- cache metadata from virtual methods
- resolve file extension to authored asset type
- support content-browser create/open/save/editor routing
- drive generic JSON source import for reflected authored assets

### 2. Runtime Asset Registration

Built on top of the existing `AssetPipeline` runtime/factory layer.

Responsibilities:

- register cooked payload serializers
- register runtime loaders/factories
- support `Load<>` and `Get<>` for final runtime types
- stay focused on cooked/runtime content rather than authored editor documents

This preserves the strengths of the current `AssetManager` model while fixing the authored-content side.

## Migration Direction

The clean migration path is:

1. introduce `IAsset`
2. introduce the authored-asset registry
3. move `NodePrefabAsset`, `Conduit::GraphAsset`, and `Conduit::ClassAsset` onto `IAsset`
4. add cereal JSON save plus tolerant source import for authored assets
5. route content-browser asset creation through `Derived(IAsset)`
6. evolve cookers so authored assets produce final runtime payloads
7. move runtime `Load<>` / `Get<>` of systems like Conduit toward final compiled runtime types

## Bottom Line

The intended model is:

- `IAsset` is a source-asset authoring interface
- `IAsset` files are saved as JSON
- authored assets are discovered by reflected type and file extension
- authored assets flow through import/cook like any other source content
- authored source import is tolerant: unknown fields are ignored and missing fields keep defaults
- only cooked runtime payloads belong in packs
- `Load<>` and `Get<>` remain runtime-facing APIs for final game-ready types
- authored prefabs are source assets too
- runtime object serialization still exists as a separate runtime-state lane

That gives the framework:

- automatic authored asset discovery
- better source control ergonomics
- a clean content-browser create/open/save path
- room for Conduit and future authored systems to cook into optimized runtime payloads
