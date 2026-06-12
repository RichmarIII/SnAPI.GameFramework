# Conduit And Scripting

Read this when:

- changing Conduit runtime graphs, authored graph documents, compiler bridges,
  scripting ABI, Lua/SWIG integration, reflection binding, or generated type
  registration

Related context:

- `../ARCHITECTURE.md`
- `RUNTIME_CORE.md`
- `BUILD_FLAGS_AND_DEPENDENCIES.md`

## Conduit Runtime

Conduit is the reflection-driven visual scripting runtime for GameFramework. It
uses authored graph assets, compiled frame slots, cached reflected field/method
bindings, explicit control-flow primitives, builtin intrinsics, and
handle-family-based instance resolution.

Conduit runtime behavior should stay independent of editor graph UI. The editor
can author graph documents and compile them, but runtime execution must be
testable without the editor shell.

## Reflection

Reflection metadata powers serialization, editor property panels, Conduit,
replication, reflected RPC, and scripting bridges. Reflection changes are shared
behavior changes and need broad validation.

Generated reflection output is build output. Do not edit generated reflection
files as source.

## Scripting

Lua scripting currently depends on SWIG bindings when enabled. Scripting APIs
should expose stable GameFramework contracts and avoid binding private storage or
implementation details directly.

## Target Module Layout

Conduit runtime contracts, scripting contracts, and reflection public contracts
should live in module `Public/` roots. Compiler bridges, editor graph canvas
implementation, generated binding internals, and scripting backend details should
live in `Private/` roots or focused optional modules.
