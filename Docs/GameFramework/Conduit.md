# Conduit

## Overview

Conduit is the visual scripting runtime layer for `SnAPI.GameFramework`.

This document covers the runtime architecture.
For the authored graph editor/document design, read [ConduitEditor.md](/mnt/Dev/CodeProjects/SnAPI.GameFramework/Docs/GameFramework/ConduitEditor.md).

It is designed around one central idea:

- author graphs in a flexible, reflection-driven tool
- bind them once into a compact runtime form
- execute them through slot storage and cached node thunks

Conduit is not intended to run by rebuilding `Variant` arrays and doing fresh reflection lookup on every node execution.
Its purpose is to turn reflection metadata into an efficient runtime representation.

Today, the public Conduit runtime surface is the low-level binding layer:

- `Conduit::GraphBuilder`
- `Conduit::GraphAsset`
- `Conduit::ClassAsset`
- `Conduit::ClassComponent`
- `Conduit::CompiledClass`
- `Conduit::CompiledGraph`
- `Conduit::GraphInstance`
- `Conduit::FrameLayout`
- `Conduit::FrameStorage`

The authored graph asset layer now exists in a deliberately low-level first form and compiles directly down to these runtime primitives.

## Design Goals

Conduit is designed to satisfy these constraints:

- reflection-driven
- fast enough for game logic, not just tooling
- generic across reflected types
- explicit about value storage vs object/handle references
- safe against common runtime failures such as unresolved handles and runaway loops

In practice, that means:

- slots are preallocated in one frame buffer
- field and method metadata is bound ahead of execution
- control flow is explicit
- basic logic/math is handled by intrinsics instead of requiring reflected helper methods
- handles are resolved by handle family, not hardcoded per gameplay type

## Mental Model

Think of Conduit as a small compiled dataflow/control-flow runtime.

The important layers are:

1. Reflected types and members
2. `GraphBuilder`
3. `CompiledGraph`
4. `GraphInstance`
5. `ExecutionContext`

### Reflected types and members

Conduit does not invent its own type system.

It relies on the existing reflection system for:

- `TypeInfo`
- `FieldInfo`
- `MethodInfo`
- `TypeRuntimeOps`

That gives Conduit:

- field binding
- method binding
- raw lifecycle ops for slot storage
- equality fallback through `RuntimeOps->Equals`

### GraphBuilder

`GraphBuilder` is the current compile/bind API.

It is intentionally low-level.

It does not represent an end-user graph editor object.
It represents the compiler target that an authored graph asset should lower into.

`GraphBuilder` is responsible for:

- allocating slots
- binding reflected fields and methods
- emitting constants
- emitting intrinsics
- emitting control flow
- creating and resolving labels
- validating type compatibility up front

### CompiledGraph

`CompiledGraph` is the runtime product of the builder.

It contains:

- a `FrameLayout`
- a dense array of `BoundNode`
- the max scratch arg count needed for method invocation

This is the immutable runtime plan.

### GraphInstance

`GraphInstance` owns the live execution state for one compiled graph.

It contains:

- one `FrameStorage`
- one reusable scratch arg array

It does not own the compiled graph itself.

### ExecutionContext

`ExecutionContext` carries all external runtime inputs that should not live in frame slots:

- optional `Self`
- optional `SelfType`
- optional custom handle resolver
- `MaxNodeExecutions`

The frame stores graph data.
The context stores execution-environment data.

## Slot Model

Slots are the primary data currency of Conduit.

Each slot has:

- a reflected type
- a kind
- a byte offset in the frame
- size/alignment metadata

The two slot kinds are:

- `Value`
- `Handle`

### Value slots

These are the default slots.

Use them for:

- bools
- ints
- floats
- strings
- enums
- vectors
- colors
- reflected structs
- explicit object-handle values

Value slots are owned by the frame.

### Handle slots

A handle slot still stores an owned value in the frame, but Conduit interprets that value as a reference carrier.

Examples:

- `NodeHandle`
- `ComponentHandle`
- future custom handle families

At runtime, a handle slot can be resolved into a live instance pointer when a node needs to act on an object.

Conduit deliberately does not store raw borrowed pointers in frame slots.

## Frame Storage

`FrameLayout` and `FrameStorage` are the reason Conduit can stay efficient.

### FrameLayout

`FrameLayout` is computed once.

It assigns:

- slot ids
- aligned offsets
- total byte size

### FrameStorage

`FrameStorage` owns one aligned allocation for all slots in one graph instance.

Slot values are managed through `TypeRuntimeOps`.

That gives Conduit:

- no per-node heap allocation for normal execution
- contiguous data
- explicit initialized/uninitialized state
- proper destruction/reset for non-trivial types

Important rules:

- reading an uninitialized slot is an error
- overwriting a slot destroys the previous live value first
- raw slot pointers are internal execution helpers, not durable references

## Runtime Node Categories

Current Conduit node categories are:

- constants
- intrinsics
- control flow
- reflected self operations
- reflected instance operations

### Constants

Constant nodes materialize a baked serialized reflected value into an output slot.

That matters because it keeps constants generic and safe for non-trivial reflected types such as strings and reflected structs.

### Intrinsics

Intrinsics are built-in operations that avoid requiring reflected helper methods for basic logic.

Current unary intrinsics:

- `LogicalNot`
- `Negate`

Current binary intrinsics:

- `Add`
- `Subtract`
- `Multiply`
- `Divide`
- `Equal`
- `NotEqual`
- `Less`
- `LessEqual`
- `Greater`
- `GreaterEqual`
- `LogicalAnd`
- `LogicalOr`

Current support model:

- arithmetic/order ops: supported numeric primitive types
- logical ops: `bool`
- equality: specialized support where available, otherwise reflected `RuntimeOps->Equals`

That last point matters.
Conduit equality is not limited to hardcoded primitives.
If a reflected type exposes meaningful equality through `TypeRuntimeOps`, Conduit can use it.

### Control flow

Conduit now has explicit control flow primitives:

- `Branch`
- `Jump`
- labels

This is the correct low-level foundation for authored nodes like:

- `If`
- `While`
- `For`
- `ForEach`
- `DoWhile`
- early-out/guard patterns

Those higher-level authored nodes should compile down to label + branch + jump rather than bypassing the primitive runtime model.

## Graph Assets And Class Assets

Conduit now has two persistent authored asset layers:

1. `GraphAsset`
2. `ClassAsset`

### GraphAsset

`GraphAsset` is the logic body.

It contains:

- authored slots
- authored graph variables
- authored nodes
- persisted editor metadata
- optional declared `SelfType`

`CompileGraphAsset(...)` lowers it directly into `CompiledGraph`.

### ClassAsset

`ClassAsset` is the first concrete step toward graph-backed gameplay classes.

It contains:

- `HostType`
- `TAssetRef<GraphAsset>`
- authored class name

It does not generate a native C++ subclass.
It binds one real reflected host node type to one graph asset and establishes what `self`
means for that graph at runtime.

`CompileClassAsset(...)` works like this:

1. resolve the referenced `GraphAsset` through `AssetManager`
2. validate that `HostType` derives from `BaseNode`
3. if the graph has no `SelfType`, inject `HostType`
4. if the graph already has `SelfType`, require `HostType` to derive from it
5. compile the resolved graph into `CompiledGraph`
6. return `CompiledClass`

`CompiledClass` contains:

- the reflected host type
- the effective self type
- the resolved source graph
- the compiled runtime graph

This is the right model for Conduit-backed gameplay types because it gives you:

- real reflected `self`
- asset-backed logic classes
- no per-concrete-type resolver explosion
- no fake story about graphs becoming native subclasses

### ClassComponent

`ClassComponent` is the first live runtime host for `ClassAsset`.

It attaches to a node, resolves/compiles the referenced class asset through the default
`AssetManager` resolver, and executes the bound graph against the owning node as `self`.

Current behavior:

- the owning node must satisfy the authored `HostType`
- one `GraphInstance` is retained per component, so frame slots persist across executions
- built-in lifecycle entrypoints such as `OnCreate`, `Tick`, and `OnDestroy` are compiled out of the graph itself
- tick-like entrypoints may declare a typed `DeltaSeconds` slot, and `ClassComponent` injects the current phase delta before dispatch
- custom named entrypoints are callable externally by name without re-binding the graph instance

This is intentionally component-based rather than a world special case.
It proves the node-attached Conduit-class model without forcing graph assets to pretend they
are native subclasses.

### Reflected self operations

These operate on `ExecutionContext::Self`.

They are useful when a graph is effectively "attached" to one owning object.

Current supported self operations:

- field read
- field write
- method call

### Reflected instance operations

These operate on a handle slot that resolves to a runtime object.

Current supported instance operations:

- field read
- field write
- method call

This is how Conduit reaches arbitrary reflected nodes/components without turning every runtime access into string lookup.

## Handle Resolution

One of the most important Conduit design decisions is this:

new reflected gameplay types should not require new Conduit resolver code every time.

The actual boundary is not "one resolver per concrete reflected type."
The boundary is "one resolver per handle family."

That means:

- reflected value types need no resolver
- `NodeHandle` works automatically for any reflected node type
- `ComponentHandle` works automatically for any reflected component type
- a new custom handle family would need one family resolver registration

This is the right split because reflection can tell Conduit what a handle value is, but it cannot guess how an arbitrary opaque reference carrier maps to a live runtime instance.

## Execution Model

Conduit execution is program-counter based.

This is important.
It is no longer just a blind linear walk over nodes.

Execution loop:

1. Start at node index `0`
2. Execute the current node
3. Node returns either:
   - fallthrough
   - explicit next node index
4. Stop when the program counter exits the node array
5. Abort with error if `MaxNodeExecutions` is exceeded

### Why the execution cap exists

Visual scripting systems need a failure mode for accidental infinite loops.

Conduit uses `ExecutionContext::MaxNodeExecutions` as a hard guard.

That gives you:

- deterministic failure instead of a hang
- caller control over the allowed budget
- a clean place for future editor/runtime diagnostics

## Reflection Binding Model

Conduit uses reflection in two distinct phases:

### Bind/build time

Reflection is used to:

- resolve fields by name
- resolve methods by name/signature
- validate slot types
- find fast field access paths
- select raw method invoke thunks

### Execute time

Runtime uses:

- already-bound `FieldInfo*`
- already-bound `MethodInfo*`
- precomputed slot ids
- precomputed frame offsets
- precomputed intrinsic function pointers

That is the core performance idea.

## Performance Model

Conduit is designed to be fast for a reflection-driven scripting system, but the intended fast path still matters.

### What Conduit is good at

- reflection-bound gameplay logic
- control flow over value slots
- object interaction through stable handles
- graph constants/defaults
- dataflow over small and medium reflected values

### What keeps it fast

- one contiguous frame allocation
- no per-node lookup by field/method name
- no per-node heap churn in the normal path
- cached node execute callbacks
- raw reflected runtime ops for slot lifecycle
- no `Variant` pack/unpack for ordinary arithmetic/control flow

### What to avoid

- treating borrowed runtime pointers as durable data
- routing every primitive operation through reflected helper methods
- storing large amounts of transient state outside slots
- designing authored nodes that bypass slot/control-flow compilation

## Example: Loop Lowering

A conceptual authored loop like:

```text
while (Health < Limit)
{
    Health = Health + Delta
}
```

should lower roughly like this:

1. mark `LoopCondition`
2. read `Health` into a slot
3. intrinsic `Less(CurrentHealth, Limit) -> Condition`
4. branch `Condition ? LoopBody : Exit`
5. mark `LoopBody`
6. intrinsic `Add(CurrentHealth, Delta) -> NextHealth`
7. write `Health = NextHealth`
8. jump `LoopCondition`
9. mark `Exit`

That is exactly the kind of lowering Conduit now supports.

## Current Limits

Conduit is still early-stage.

Important current limits:

- there is no editor graph schema yet
- there are no first-class authored node definitions yet
- the current authored `GraphAsset` format is intentionally low-level and maps closely to `GraphBuilder`
- `ClassAsset` runtime hosting currently exists only through `ClassComponent`; spawn helpers and higher-level class instantiation policy are still the next layer
- intrinsics currently cover only the first useful core set
- container-specific graph/runtime helpers do not exist yet
- debug stepping/tracing hooks are not yet exposed

These are expected next steps, not design accidents.

## Recommended Usage Today

Use Conduit today for:

- proving the runtime model
- validating reflection binding
- building control-flow and intrinsic semantics
- testing handle resolution behavior
- preparing the authored asset compiler target

Do not treat `GraphBuilder` as the final end-user authoring API.
Treat it as the runtime compiler target that the real asset/editor layer should emit.

## Planned Layering

The intended long-term layering is:

1. authored `Conduit::GraphAsset`
2. authored `Conduit::ClassAsset` where a graph-backed host type is needed
3. graph compiler/binder
4. `CompiledGraph` / `CompiledClass`
5. `GraphInstance`
6. runtime execution in a world/gameplay context

That split is important because it keeps:

- authoring flexible
- runtime strict
- performance predictable

## File Guide

Public Conduit headers:

- `include/Conduit.h`
  Umbrella include and module entry point.
- `include/Conduit/Types.h`
  Core ids, enums, execution context, and slot metadata.
- `include/Conduit/Value.h`
  Durable serialized value payloads used by constants and authored assets.
- `include/Conduit/Resolvers.h`
  Handle-family resolver registration.
- `include/Conduit/Frame.h`
  Frame layout and owned slot storage.
- `include/Conduit/Graph.h`
  Compiled graph types and runtime execution entry points.
- `include/Conduit/Compiler.h`
  `GraphBuilder` and graph binding APIs.
- `include/Conduit/Asset.h`
  Authored graph/class asset payloads and compile helpers.
- `include/Conduit/ClassComponent.h`
  Runtime host component that binds a `ClassAsset` to a live node.

## Bottom Line

Conduit is a compiled, slot-based, reflection-driven visual scripting runtime.

Its current shape is deliberately low-level and runtime-oriented.
That is the right foundation.

The next major layer should not replace this model.
It should sit on top of it:

- authored asset
- editor graph
- compiler
- runtime execution through these primitives
