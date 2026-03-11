# Conduit Editor Design

## Purpose

`Conduit` already has a runtime and a low-level authored asset/compiler path.
What it does not have yet is the real authoring system that game teams will live in every day.
This document defines that editor architecture.

This is the editor-side design for:

- authored graph UX
- asset/document model
- reflection-driven schema expansion
- compile diagnostics
- editor service boundaries
- relationship to the existing Conduit runtime

Read [Conduit.md](/mnt/Dev/CodeProjects/SnAPI.GameFramework/Docs/GameFramework/Conduit.md) first for the runtime model.
Read this document for the authoring model built on top of it.

## Goals

The Conduit editor must be:

- asset-backed
- reflection-driven
- fast enough to author large graphs comfortably
- structured so runtime execution stays separate from editor UX
- open-ended for any reflected type that Conduit can actually compile and execute
- compatible with the existing editor shell, asset system, property inspector, and undo stack

## Non-Goals

This first editor layer is not trying to:

- make the low-level `GraphBuilder` API user-facing
- store only runtime-lowered data and reconstruct a graph UI from it later
- use the generic asset inspector modal as the primary authoring surface
- hardcode one-off node resolvers for every gameplay type
- route hot runtime execution through editor abstractions

## Core Principle

There are two different systems here and they must stay separate:

1. authored graph editing
2. compiled graph execution

The authored graph needs rich UX state:

- node positions
- comment boxes
- bookmarks
- selection
- palette metadata
- compile diagnostics
- document revisioning

The runtime graph needs:

- validated slot layout
- cached reflected bindings
- program-counter execution
- no editor-only baggage

The editor compiles authored assets into runtime Conduit graphs.
It does not replace the runtime graph model.

## Layering

The Conduit stack should be understood as five layers.

### 1. Reflection layer

Owned by `SnAPI::GameFramework`.
This is the source of truth for:

- reflected types
- reflected fields
- reflected methods
- runtime type ops
- handle family resolution rules

Conduit editor code must consume this layer, not duplicate it.

### 2. Conduit runtime layer

Owned by `SnAPI::GameFramework::Conduit`.
This is already implemented and includes:

- `GraphBuilder`
- `CompiledGraph`
- `GraphInstance`
- `ExecutionFrame`
- intrinsics
- control-flow primitives
- reflected field and method nodes
- handle-family based target resolution

This is the execution layer.

### 3. Authored asset layer

Owned by `SnAPI::GameFramework::Conduit`.
This is the persistent asset payload used by packs, the asset pipeline, and save/load.

This layer stores:

- authored nodes
- authored slots
- authored graph variables with typed defaults
- stable authored ids
- editor metadata such as layout and viewport state

The authored asset is persistent data, not the live editor document.

### 4. Conduit editor layer

Owned by `SnAPI::GameFramework::Conduit::Editor`.
This is the live authoring layer and currently owns:

- open graph documents
- transient selection state
- compile diagnostics
- reflection-driven schema catalogs
- the compile bridge back into runtime Conduit

This layer is the bridge between editor UX and runtime compilation.

### 5. Editor shell integration

Owned by `SnAPI::GameFramework::Editor` and consumed by `Conduit::Editor`.
This layer provides:

- document hosting
- docking/tab layout
- asset browsing
- save/open flows
- property inspection
- undo/redo infrastructure

Conduit editor functionality should plug into this shell rather than bypass it.

## Why Conduit Needs A Real Document Editor

The current generic asset inspector flow is property-centric.
That is the wrong interaction model for a graph language.

Conduit authoring requires:

- a center canvas
- pan and zoom
- graph selection
- wire manipulation
- palette/search spawning
- contextual compile feedback
- multiple open tabs

That means Conduit must behave like a document editor, not like a modal property form.

## Editor Layout Model

The target Conduit editor layout is:

- left: node palette, search, favorites, graph outline
- center: graph canvas
- right: inspector/details panel
- bottom: compile errors, warnings, and search results
- top of the document tab: graph title, dirty indicator, compile status, quick actions

The graph canvas is the primary surface.
The property inspector is secondary and selection-driven.

## Asset Model

The Conduit asset is a real asset and should be treated like other authored assets.
It must preserve both runtime-meaningful data and authoring metadata.

### Persistent authored data

This is the data needed to reconstruct authored intent:

- graph name
- self type
- authored slots
- authored nodes
- stable authored node ids
- member references
- control-flow labels
- authored literal payloads

### Persistent editor metadata

This is editor-only but still belongs in the asset because it is part of authoring state:

- node positions
- node collapsed state
- viewport pan
- viewport zoom
- comment boxes
- bookmarks

This data should serialize with the asset and round-trip through the asset pipeline.

### What does not belong in the asset

The asset should not store:

- transient selection
- hover state
- drag previews
- runtime-compiled frame storage
- compiled graph instances
- temporary diagnostics from a previous editor session unless deliberately cached

Those belong to the live editor document.

## Stable Authored Identity

The runtime layer can execute nodes by linear program order.
The editor layer cannot rely only on vector indices.

Authored nodes need stable ids because the editor must track:

- layout entries
- comment membership
- bookmarks and jump-to-node behavior
- diagnostics mapped back to nodes
- clipboard and duplicate operations
- future diff/merge tools

This is why `GraphNodeAsset` now carries a durable `Uuid`.

## Current GraphAsset Shape

The Conduit asset now has two responsibilities:

1. low-level authored graph data that compiles directly into `GraphBuilder`
2. editor metadata under `EditorState`

That is intentional for the current phase.

It means:

- runtime compilation still stays simple
- editor state persists with the asset
- higher-level authored nodes can be layered later without discarding the current runtime model

## Live Document Model

`Conduit::Editor::GraphDocument` is the live document model for one open graph tab.

It owns:

- the editor asset key
- UI title
- a mutable working copy of `GraphAsset`
- authored graph variables and their typed defaults
- transient selection state
- dirty tracking
- a monotonic revision counter
- the last compile result

It does not own:

- asset discovery
- file I/O
- docking/tab behavior
- runtime graph execution

Those belong elsewhere.

## Document Dirty Model

The intended rule is simple:

- any semantic or layout change touches the document
- variable add/remove/rename/default edits also invalidate cached compile output
- touching the document increments its revision and marks it dirty
- successful saves clear the dirty flag
- compile operations do not automatically clear dirty state

Compiling is validation, not persistence.

## Editor Service Boundary

`Conduit::Editor::ConduitEditorService` is the root editor service for this subsystem.

Its current responsibilities are:

- own open Conduit documents
- own the Conduit schema registry
- surface lightweight workspace metadata such as slot, variable, and node counts
- own the compile bridge
- normalize editor metadata when documents open

It is intentionally small right now.
That is correct.

The service should become the coordination point for future:

- document tabs
- palette queries
- compile commands
- diagnostics panes
- breakpoint/tracing state

## Why This Is A Service

The editor already uses service composition for major systems.
Conduit should fit into that same model.

Benefits:

- explicit lifetime
- dependency declaration
- no hidden global singleton
- easy access from layout, inspector, and command systems
- clean shutdown and document cleanup

The service currently depends on:

- `EditorCommandService`
- `EditorAssetService`

That is the correct minimum.

## Schema Registry

The Conduit editor cannot hardcode node catalogs per gameplay type.
That would destroy the whole point of reflection-driven scripting.

Instead, the editor must build schema data from two sources:

1. builtin authoring nodes
2. authored graph variables
3. reflected fields and methods

`Conduit::Editor::SchemaRegistry` now provides that boundary.

### Builtin nodes

The builtin catalog covers the nodes Conduit always understands regardless of game types:

- entry nodes
- constants
- branch
- jump
- unary intrinsics
- binary intrinsics

These are authored node templates, not runtime instances.

### Graph variable nodes

For the currently authored `GraphAsset`, the schema registry also expands graph-owned instance
variables into:

- `Get <Variable>`
- `Set <Variable>`

These are generated from authored variable declarations, not from reflection, and they lower to
fixed slot copies in the compiled runtime graph.

### Reflected nodes

For a self type or instance owner type, the schema registry expands reflection metadata into node templates for:

- readable fields
- writable fields
- raw-invokable methods

This keeps node discovery automatic for any type already registered with the reflection system.

### Important restriction

The schema only exposes what the runtime can compile today.

That means:

- field read nodes appear only when Conduit has a supported read lane
- field write nodes appear only when Conduit has a supported write lane
- method call nodes appear only when `RawInvoke` exists

This is deliberate.
The editor must not offer nodes that the runtime cannot actually execute.

## Schema Descriptor Model

The schema layer describes nodes in editor terms, not runtime terms.

Each descriptor includes:

- stable template id
- display name
- category path
- tooltip
- node family
- purity
- whether it requires later specialization
- reflected owner/member info when applicable
- pin descriptors
- direct lowering opcode when one exists

This is enough for:

- palette lists
- search
- context spawning
- pin rendering
- early compatibility validation

## Pins And Type Semantics

Pins need richer meaning than just a raw `TypeId`.
Conduit authoring currently models three relevant dimensions:

- value vs handle
- data vs exec
- fixed-type vs polymorphic

That is why schema pins carry:

- `TypeId`
- `ESlotKind`
- `IsExec`
- `IsPolymorphic`

### Value pins

These carry owned slot values.
Examples:

- bool
- int
- float
- string
- reflected structs

### Handle pins

These carry durable object identity, not borrowed pointers.
They are resolved through handle-family resolvers at runtime.

### Exec pins

These encode control flow and should never be confused with data pins.

### Polymorphic pins

These represent nodes that are authored before their exact types are fixed.
Examples:

- generic constant nodes
- generic intrinsic nodes

The eventual graph UI/compiler can specialize them later based on links or explicit authoring choices.

## Self Nodes vs Instance Nodes

The schema distinguishes two major reflection contexts.

### Self nodes

These operate against the graph's `SelfType`.
They do not expose an explicit target-handle pin.

Examples:

- `Get Health`
- `Set Health`
- `AddHealth(...)`

### Instance nodes

These operate against a handle-resolved target instance.
They do expose a `Target` handle pin.

Examples:

- `Get Charge` on a component handle
- `Set Score` on a node handle
- `AddPower(...)` on a resolved gameplay object handle

This split mirrors the runtime API and keeps authoring semantics honest.

## Compile Bridge

`Conduit::Editor::CompilerBridge` exists so the editor does not call `CompileGraphAsset(...)` directly everywhere.

Current responsibilities:

- compile a `GraphAsset`
- compile a `GraphDocument`
- produce `CompileOutput`
- cache diagnostics and graphs on the document through the service

Future responsibilities:

- lower higher-level authored nodes into low-level asset ops
- preserve authored-to-runtime mapping tables
- map diagnostics back to exact authored nodes and pins
- compile partial subgraphs for previews or analysis

The bridge is thin today by design.
That does not make it unnecessary.
It prevents the UI from hardcoding compile behavior.

## Diagnostics Model

Diagnostics are editor-facing and should not just be raw strings thrown at logs.

The document compile result now tracks:

- severity
- optional authored node id
- message
- optional compiled graph on success

Current compile failures are still mostly graph-level because the low-level runtime compiler returns one error string.
That is acceptable for the current phase.

The important part is that the editor layer already has the right container for richer diagnostics later.

## Automatic Editor Metadata Normalization

When a Conduit document opens, the editor service normalizes editor metadata.
That currently means:

- every authored node gets a stable id
- duplicate or missing ids are repaired
- missing node layout records are synthesized
- viewport zoom is clamped to a sane default
- bookmarks and comment boxes get ids if missing
- comment membership is filtered to live nodes

This matters because existing low-level assets may not yet carry complete editor metadata.
The editor should repair them on open rather than fail.

## Inspector Integration

The right inspector should remain part of the shared editor shell.
Conduit should use it, not replace it.

Selection rules should be:

- no selection: graph properties
- single node: node properties and reflected metadata
- single pin: pin defaults and metadata
- multi-select: batch actions only
- comment box: title, color, and bounds

This keeps the graph canvas focused while reusing the existing reflection/property tooling.

## Undo/Redo Model

All graph mutations should eventually flow through `EditorCommandService`.
That includes:

- add node
- move node
- delete node
- connect pins
- disconnect pins
- edit constant literal
- edit graph settings
- edit comment box membership

The current scaffold does not implement command objects yet.
That is fine.
The service dependency is already in place so the next layer has a clear home.

## Interaction Model

The eventual graph UI should support at minimum:

- left-click select
- drag move nodes
- box select
- right-click context spawn
- drag from pin to create links
- drag off pin to break links
- inline literal editing for practical value pins
- keyboard delete, duplicate, frame selection, and search

Those are UX expectations, not runtime features.

## Authoring Model vs Runtime Model

A critical rule:

The graph UI should author higher-level concepts when that improves usability, but it should still compile into the existing runtime primitives.

Examples:

- `If` may lower to `Branch`
- `While` may lower to `Label + Branch + Jump`
- `Sequence` may lower to multiple exec links and labels
- `ForEach` may lower to future container intrinsics and control-flow primitives

The runtime should stay small and strict.
The editor may expose friendlier authoring constructs on top.

## Reflection Metadata The Editor Still Needs

The current reflection system is enough to generate a basic schema, but a good editor will need more metadata eventually.

Important future metadata includes:

- display name overrides
- palette category overrides
- tooltip/help text
- keyword search aliases
- hidden/internal flags
- deprecated/replacement info
- compact node title
- advanced-pin flags
- pure/impure overrides where `const` is not enough
- default-to-self behavior
- preferred literal editor widgets
- pin color/icon family hints

Without this, the system remains functional but rough.

## Asset Pipeline Relationship

Conduit graphs are real assets.
That means they participate in:

- asset discovery
- save/load
- pack cooking
- runtime asset references
- editor dirty tracking

The Conduit editor must use `EditorAssetService` for those workflows.
It should not invent a parallel file or tab persistence layer.

## Docked Workspace Integration

The Conduit editor should open through the existing asset browser, but it should not
reuse the legacy asset-inspector modal as its primary UX surface.

The first shell integration pass should behave like this:

1. double-clicking a `Conduit Graph` asset routes through `EditorAssetService`
2. the asset loads into `ConduitEditorService` as a `GraphDocument`
3. the editor shell exposes a docked Conduit workspace tab in the center pane
4. save requests still flow back through `EditorAssetService` and the normal asset pipeline

This preserves one asset workflow while allowing Conduit to grow into a real document editor.

The implemented workspace now follows that sequencing:

- document/service plumbing first
- docked canvas rendering and interaction second
- richer node wiring and authoring controls later

## Conduit Classes

One strong future direction is to let Conduit author graph-backed gameplay "classes".
This idea is good, but it needs to be framed correctly.

What should not happen:

- each graph pretending to be a native compiled C++ subclass automatically
- each graph requiring hand-written resolver code per concrete gameplay type

What should happen instead:

- one Conduit-authored class asset declares a reflected host base type
- that asset owns or references a `Conduit::GraphAsset`
- runtime spawns a real native host object of the chosen base type
- the Conduit graph attaches to that live instance
- `self` inside the graph resolves to that host object

That means the system is closer to:

- an asset-backed reflected class/archetype
- not a literal generated native C++ type

The practical model should be:

1. `GraphAsset`
   - the logic body
   - nodes, slots, editor metadata, compile target
2. `Conduit::ClassAsset`
   - the spawnable authored type
   - reflected base type
   - reference to the logic graph
   - compile step that resolves the graph and establishes the effective `self` type
3. `Conduit::CompiledClass`
   - the bound runtime form
   - resolved `GraphAsset`
   - `CompiledGraph`
   - effective self type used for execution

This gives you the behavior you want:

- spawnable Conduit-authored gameplay types
- real reflected `self`
- integration with world/node/component lifecycles
- no fake "every graph is a native C++ subclass" story

What is implemented now:

- `Conduit::ClassAsset` exists as a real asset payload
- it stores `HostType` and `TAssetRef<GraphAsset>`
- `CompileClassAsset(...)` resolves the referenced graph asset through `AssetManager`
- if the graph has no `SelfType`, compile injects `HostType` as the effective self type
- if the graph already declares `SelfType`, compile requires `HostType` to derive from that type
- the result is `Conduit::CompiledClass`
- graph assets can now declare built-in lifecycle entry nodes and custom named entry nodes
- `Conduit::ClassComponent` can now attach that compiled class to a live host node, dispatch built-in lifecycle entries automatically, and expose custom named entry execution

That is the correct first implementation slice because it proves the semantic model without
pretending that graphs are generating native subclasses.

If later you add native code generation, that can become a separate optimization path.
It should not be the semantic foundation of the system.

## Intended Runtime Workflow

The end-to-end workflow should be:

1. User opens a `Conduit::GraphAsset` in the editor.
2. `ConduitEditorService` opens or focuses a `GraphDocument`.
3. The graph canvas edits the document working copy.
4. Compile requests go through `CompilerBridge`.
5. The bridge lowers and compiles into `CompiledGraph`.
6. Diagnostics map back to the document.
7. Saving persists the updated `GraphAsset` through the asset pipeline.

That is the right separation of responsibilities.

The current implemented source-asset workflow now also includes the class side:

1. User creates or opens a `Conduit::ClassAsset` from the source-first content browser.
2. `EditorAssetService` routes that source asset into `ConduitEditorService::OpenClassDocument(...)`.
3. The Conduit workspace switches into class-document mode instead of the generic inspector modal.
4. Class authoring currently covers:
   - class name
   - concrete reflected host-node type
   - referenced `Conduit::GraphAsset`
5. Saving persists the edited `ClassAsset` back to source JSON through the authored-asset flow.

## First Concrete Slice

The current first slice implemented in code is:

- persistent editor metadata on `GraphAsset`
- stable authored node ids
- `GraphDocument`
- `ClassDocument`
- `SchemaRegistry`
- `CompilerBridge`
- `ConduitEditorService`
- default editor-service registration
- asset-service routing for opening and saving Conduit graph/class documents
- a docked center-pane Conduit workspace in the editor shell
- Conduit workspace class-document mode with host-type and graph-asset selection
- left-side graph-variable management with typed default editing
- schema-backed node palette/search listing
- a real `UIConduitGraphCanvas` center surface with authored node cards
- dock-zone split layout for the graph workspace instead of fixed-width panels
- scrollable inspector and class-authoring panes so long forms do not overlap or clip
- short reflected type labels in the Conduit workspace instead of fully-qualified names
- rendered node pins on the graph canvas
- first-pass control-flow wire rendering for authored jump/branch-to-label routing
- persisted canvas pan/zoom and node-position edits routed back through the editor service
- canvas-backed node selection and remove wiring
- right-side selected-node inspector editing for:
  - custom entrypoint names
  - label names
  - jump target labels
  - branch true/false labels
- document-backed variable and authored-node mutation APIs

It does not yet include:

- command objects
- diagnostics panel UI
- full multi-document Conduit tab management
- generic authored data-wire storage and full node wiring UX
- comment-box editing UX
- broader node-parameter editing UX beyond the current entry/label control-flow fields and variable/default flow

That is the next phase.

## Immediate Next Implementation Steps

The next Conduit editor milestones should be:

1. Add node wiring and pin hit-testing on top of the existing graph canvas.
2. Add command objects for node/layout/link edits.
3. Add compile diagnostics panel and per-node error badges.
4. Add multi-document Conduit tab management inside the center workspace.
5. Extend node-parameter editing beyond the current entry/label control-flow fields into literals and future pin metadata.
6. Add comment-box create/move/resize flows.
7. Add higher-level authored nodes that lower into current runtime primitives.
8. Add spawn helpers and higher-level Conduit-class authoring UX on top of `Conduit::ClassAsset` / `Conduit::ClassComponent`.

## Long-Term Direction

The intended long-term direction is:

- runtime remains lean and execution-focused
- authored assets remain stable and serializable
- editor metadata persists with the asset
- the Conduit editor is fully reflection-driven rather than hand-curated per gameplay type
- graph authoring feels native to the broader editor shell rather than bolted on

That is the correct path if Conduit is going to be a primary game-creation feature.
