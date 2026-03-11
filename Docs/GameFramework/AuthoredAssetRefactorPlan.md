# Authored Asset Refactor Plan

## Purpose

This document is the working execution plan for the authored-asset refactor in
`SnAPI.GameFramework`.

It exists so the work order stays explicit even if implementation spans many turns or the working
context is compacted.

The goal is to get to a coherent system where:

- the editor is source-asset based
- authored assets derive from `IAsset`
- authored source files are JSON
- import goes through `SnAPI.AssetPipeline::IAssetImporter`
- source import is tolerant and reflection-driven
- cooked assets remain strict, binary, and runtime-fast
- `Load<>` / `Get<>` stay focused on final runtime types

## Current Problems

The main issues that need to be fixed together are:

1. reflection does not cleanly support abstract/interface types
2. `IAsset` handling is currently coupled into `TypeBuilder` / `TypeInfo`
3. source-asset JSON import/save concerns are mixed into reflection internals
4. editor content discovery and workflows must fully stop treating cooked packs as source-of-truth content
5. authored asset families are only partially moved to a uniform source-asset model
6. Conduit/editor work was started before asset creation/open/save was actually stable

These problems overlap, so the order matters.

## Non-Goals

This refactor is not trying to:

- make source assets load through the runtime binary codec path
- make cooked assets schema-tolerant
- make `TValueCodec` carry field names for source import
- bypass `IAssetImporter` and `IAssetCooker`
- keep editor workflows centered on `.snpak`

## Progress Snapshot

- Step 1: complete
- Step 2: complete
- Step 3: complete
- Step 4: complete
- Step 5: complete
- Step 6: complete
- Step 7: complete
- Step 8: complete
- Step 9: in progress

Step 7 partially had to land before Step 6 could close:

- `NodeAsset`, `LevelAsset`, and `WorldAsset` were added as authored source assets first
- project startup/open was then switched from `.snpak` startup levels to `.level` source assets
- legacy `startupLevelPack` project settings are only read as a compatibility fallback

## Work Order

### 1. Reflection Core Cleanup

Add proper abstract/interface reflection support and remove authored-asset coupling from reflection
core.

Deliverables:

- add explicit interface/abstract registration support
- add `Interface<>` alongside `Base<>` in `TTypeBuilder`
- allow registering abstract/interface reflected types without pretending they are constructible
- remove `IAsset` upcast/save/load hooks from `TypeBuilder`
- remove authored-asset JSON hooks from `TypeInfo`

Why this comes first:

- `IAsset` should not be a special case hidden in reflection-builder internals
- authored asset import should be built on top of reflection, not baked into it

### 2. `IAsset` Source-Asset Contract

Define the final authored-source-asset interface and metadata contract.

Deliverables:

- `IAsset` virtual interface
- metadata methods:
  - `DisplayName()`
  - `Category()`
  - `FileExtension()`
  - `EditorMode()`
  - `CanCreate/Save/Rename/Delete()`
- authored/cooked asset kind metadata
- `Save(std::ostream&)` for source-asset JSON write

Why this comes second:

- importer, registry, and editor workflows all depend on the final source-asset contract

### 3. Authored Asset Registry

Build one cached GameFramework-side registry for authored asset types.

Deliverables:

- startup bootstrap:
  - `TypeAutoRegistry::EnsureAll()`
  - `TypeRegistry::Derived(StaticTypeId<IAsset>())`
  - default-construct each concrete asset type once
- cache metadata by:
  - reflected type id
  - file extension
  - source asset kind
  - cooked asset kind
- validation:
  - duplicate extensions
  - duplicate authored identities
  - non-reflected asset types
  - invalid concrete asset definitions

Why this comes third:

- importer registration and editor source workflows need one authoritative asset-type catalog

### 4. Source Save / Tolerant Import Split

Separate strict save from tolerant source import.

Deliverables:

- `IAsset::Save(std::ostream&)` uses cereal JSON output with named fields
- source import uses `nlohmann::json`
- tolerant import contract:
  - missing fields keep defaults
  - unknown fields are ignored
  - field-level failures become diagnostics, not whole-asset failure by default
  - optional aliases and schema migrations

Why this comes fourth:

- source assets need schema-tolerant import
- cooked/runtime serializers do not

### 5. AssetPipeline Integration

Move authored source import fully into AssetPipeline.

Deliverables:

- real `SnAPI.AssetPipeline::IAssetImporter` implementation for authored assets
- extension-driven source resolution through the authored asset registry
- importer default-constructs the asset and runs tolerant reflection-driven import
- importer emits the authored/intermediate payload expected by the cooker
- keep `IAssetCooker` responsible for final runtime payload production

Why this comes fifth:

- source assets should follow the same import/cook pipeline as other source content
- editor should not own ad hoc import logic

### 6. Editor Goes Fully Source-First

Make the editor operate on source files, not cooked packs.

Deliverables:

- content browser discovery from source roots
- no pack-driven discovery/search/open/save/rename/delete in editor workflows
- create/open/save/rename/delete operate on source files
- file type classification by extension through the authored asset registry
- JIT import/cook/load only for preview, PIE, and runtime consumers
- project startup/open flows stop treating `.snpak` as the project-level source of truth

Important dependency:

- this step cannot fully complete while project startup still points at a `Level`/`World` that only exists as a cooked pack payload
- either `LevelAsset` / `WorldAsset` (or an equivalent authored startup-level asset) must exist before the last editor `.snpak` dependency can be removed

Completion note:

- this is now satisfied through the `NodeAsset` family plus source-backed project startup levels
- editor project startup uses `startupLevelAsset`
- legacy `startupLevelPack` is only read for backward compatibility and normalized to `.level`

Why this comes sixth:

- this is the user-facing behavior change that makes authored assets usable
- it depends on the source-asset contract and importer path being real first

### 7. Convert Existing Authored Asset Families

Move current authored content into the uniform source-asset flow.

Deliverables:

- `NodeAsset : IAsset`
- `LevelAsset : NodeAsset`
- `WorldAsset : NodeAsset`
- `Conduit::GraphAsset : IAsset`
- `Conduit::ClassAsset : IAsset`
- `MaterialAsset : IAsset`
- `MaterialInstanceAsset : IAsset`

Important note:

- the authored asset type and the final runtime type do not have to match
- prefabs being `IAsset` does not mean runtime nodes are the asset object

Why this comes seventh:

- the infrastructure must be stable before migrating families onto it

### 8. Runtime JIT / Cooked Loading Alignment

Keep runtime loading semantics clean while supporting source fallback.

Deliverables:

- `Load<>` / `Get<>` remain APIs for final runtime types
- source fallback continues through `AssetManager` JIT
- cooked assets remain strict and binary
- source assets do not become the default runtime object lane unless intentionally designed that way

Why this comes eighth:

- runtime behavior must stay coherent while editor/source workflows change underneath

Completion note:

- `Get(name)` now resolves source-backed logical names once, then loads by final asset id
- source logical names are now covered by regression tests for both `Load<>` and `Get<>`
- `Load<>` and `Get<>` are the intended JIT boundary; discovery/query APIs remain non-JIT by design

### 9. Resume Conduit Authoring Work

Only after the asset workflow is real should deeper Conduit editor work continue.

Deliverables:

- actual asset creation from content browser
- actual graph/class asset opening
- actual source save/load through the new asset flow
- then continue with graph editor UX, node creation, variables, entrypoints, and class authoring

Progress note:

- source asset creation is now routed through the authored asset registry
- `Conduit::GraphAsset` and `Conduit::ClassAsset` source assets both open through `ConduitEditorService`
- Conduit graph/class source save now persists through the source JSON path
- class assets now have a real Conduit workspace document mode instead of falling back to the generic inspector
- graph documents now have a working selected-node inspector for custom entrypoints and control-flow labels
- deeper graph UX work remains after that baseline

Why this comes last:

- Conduit editor work on top of broken asset creation/open/save is wasted effort

## Reporting Rule

Implementation updates should report progress by the numbered steps above.

Expected reporting format:

1. step number completed
2. concrete files changed
3. verification run
4. next numbered step starting

## Design Rules That Must Hold Throughout

- `TValueCodec` stays focused on cooked/runtime binary serialization
- source-asset import does not rely on binary codec internals
- authored source import must not write arbitrary blobs directly into object memory
- source assets should be safe to evolve structurally without hard load failure
- cooked runtime payloads should stay strict and fast
- editor workflows should not use `.snpak` as source-of-truth content

## Immediate Next Step

Continue with Step 9:

- resume higher-level Conduit authoring/editor work on top of the corrected source-asset foundation
- keep using source asset creation/open/save flows instead of reviving pack-based editor assumptions
