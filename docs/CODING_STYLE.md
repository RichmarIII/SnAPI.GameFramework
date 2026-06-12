# Coding Style And Source Layout

## Read This When

- you are adding, moving, renaming, or refactoring source files, public headers,
  module folders, dependency wiring, or helper types
- you need naming conventions, file granularity rules, source-tree rules, public
  header grouping, or internal system grouping

## Naming And File Granularity Rules

GameFramework is the gameplay/session layer. Names should express gameplay,
editor, asset, build, or integration ownership directly. Do not name a type after
the old renderer or another module's internal workaround when the type is really
a GameFramework abstraction.

Files should be intentionally small. The normal shape is one primary public class
or type per header/source pair. Put supporting public types in their own files.
Keep private helper structs/classes inside the owning `.cpp` only when they are
true implementation details for that one primary type. Do not create large
catch-all files that bundle unrelated systems, settings, payloads, graph
definitions, serializers, or orchestration together.

Use include-what-you-use throughout the tree. A source or header must include the
exact header that declares each type, function, constant, or template
specialization it uses. Do not rely on transitive includes from unrelated
headers, and do not add umbrella or compatibility headers to hide moved
declarations unless an active migration plan explicitly requires it.

Include paths must be rooted at the owning module's configured `Public/` or
`Private/` include root, not written as parent-relative paths. Public
cross-module includes must come from the linked module target's `Public/`
headers; private implementation includes must stay inside the owning module's
`Private/` tree.

Organize related files into fine-grained subsystem and feature-family
subdirectories once a folder contains multiple systems or effect groups. For
example, rendering integration types should live under a rendering family, editor
services under an editor family, asset payloads under an asset family, and
Conduit editor/runtime types under focused Conduit folders.

## Source Tree, Module Boundaries, And Stripability

The target source layout is a hard-cut module layout, not a compatibility layer
over the old `include/` and `src/` roots.

```text
SnAPI.GameFramework/
  CMakeLists.txt
  cmake/
  docs/
    site/
  docs-src/
  Modules/
    GameFramework/
      Public/
      Private/
      Dependencies.cmake
      CMakeLists.txt
    Editor/
      Public/
      Private/
      Dependencies.cmake
      CMakeLists.txt
    Runtime/
      Public/
      Private/
      Dependencies.cmake
      CMakeLists.txt
    Build/
      Public/
      Private/
      Dependencies.cmake
      CMakeLists.txt
  examples/
  tests/
  tools/
```

The exact module split may change by active plan, but every compiled module must
live under `Modules/` and own:

```text
Public/
Private/
Dependencies.cmake
CMakeLists.txt
```

Top-level modules may omit an extra family segment when the module itself is the
family. Category folders may have coordinator `CMakeLists.txt` files, but they
are not compiled module roots unless they also own the full compiled-module
contract.

The root `CMakeLists.txt` owns global options, shared dependency policy,
top-level `add_subdirectory()` calls, examples, tests, tools, and docs. A
module's own `CMakeLists.txt` owns its target, sources, include directories,
compile features, definitions, private/public linkage, and feature-gated
participation. A module's `Dependencies.cmake` owns dependency discovery and
dependency application for that module.

Current `include/` and `src/` folders are migration debt. Do not expand them as
the desired design. New broad source work should either happen under an active
module-layout plan or be explicitly recorded as temporary debt.

## Naming Conventions

- type names use `PascalCase`
- template types use a `T` prefix where the codebase already follows that SnAPI
  convention, such as `THandle<>`, `TFlags<>`, or `TTypeBuilder<>`
- ordinary types must not use an `F` prefix unless an external compatibility
  boundary explicitly requires it
- private and protected fields use `m_PascalCase`
- public fields in POD structs/classes use `PascalCase`
- parameter names use `camelCase`
- local variables use `camelCase`
- default member initializers and new initialization code should prefer brace
  initialization, such as `std::uint32_t Count{0};`
- avoid global mutable state
- avoid loose helper functions; helper behavior belongs to named utility classes
  with static methods or narrow local lambdas when the behavior is truly local

## Accessor Rules

- Avoid `Get...` and `Set...` prefixes for ordinary cheap accessors.
- Prefer overload-style accessors such as `Color color = object.Color();` and
  `object.Color(Color{});` when this matches the surrounding style.
- Method names should communicate cost and side effects. Use names like
  `FindNode`, `ResolveHandle`, `CreateNode`, `CookAsset`, or `SubmitRenderWork`
  when the operation searches, resolves, creates, cooks, or submits.

## Ownership Rules

- `World` owns gameplay objects and subsystem adapters for the active session.
- Handles are durable identity and serialization boundaries.
- Borrowed pointers are short-lived views.
- Asset source documents, cooked payloads, runtime instances, and renderer
  objects are separate concepts and should have separate types.
- GameFramework may shape its own renderer-facing API around Renderer.New, but
  Renderer.New must remain independent of GameFramework.
