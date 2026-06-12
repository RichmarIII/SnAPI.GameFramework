# Runtime Core

Read this when:

- changing `GameRuntime`, `World`, `Level`, `BaseNode`, `BaseComponent`, handles,
  dense runtime storage, lifecycle, frame order, or threading behavior
- changing subsystem ownership or the runtime/editor session model

Related context:

- `../ARCHITECTURE.md`
- `../CODING_STYLE.md`
- `TESTING_VALIDATION_AND_DIAGNOSTICS.md`

## Ownership Model

`GameRuntime` is the application/session shell. It owns startup, update,
shutdown, optional frame pacing, platform input forwarding, and the active
`World`.

`World` is the authoritative session owner. It owns:

- root `Level` nodes and root `BaseNode` nodes
- child node hierarchy
- `BaseComponent` attachments
- page-backed dense runtime storage
- deferred destruction queues
- subsystem adapters for input, UI, networking, physics, audio, rendering, and
  scripting
- task dispatch entry points for work that must return to the game thread

`Level` is a node type used for grouping and authored partitions. It is not a
separate ownership graph outside the world.

## Handles And Borrowed Pointers

Handles are the stable public identity boundary. They can be serialized,
replicated, stored, and resolved later.

Borrowed pointers are temporary views into world-owned storage. Do not cache them
across destructive operations, frame boundaries where destruction can flush, or
world/subsystem shutdown.

Hot paths may rehydrate runtime keys inside handles. APIs that may resolve or
repair a runtime key should accept mutable handle references when appropriate.

## Lifecycle

Constructors and destructors should stay side-effect light. World, renderer,
physics, audio, networking, and editor setup belongs in lifecycle callbacks or
world-owned services.

Typical runtime lifecycle:

1. Build `GameRuntimeSettings`.
2. Call `GameRuntime::Init()`.
3. Configure the created `World`.
4. Repeatedly call `GameRuntime::Update(deltaSeconds)`.
5. Call `GameRuntime::Shutdown()`.

Typical object lifecycle:

1. Create or deserialize a node.
2. Attach components.
3. Deliver lifecycle callbacks such as `OnCreate`, tick phases, and `OnDestroy`.
4. Flush deferred destruction during `World::EndFrame()`.

## Frame Order

The runtime should preserve explicit frame phases:

1. gameplay host tick
2. fixed-tick accumulator
3. variable tick
4. late tick
5. end-of-frame work
6. optional platform/UI input forwarding and close handling
7. optional frame pacing

Inside world tick, subsystem ordering must remain deliberate and documented. Do
not add hidden frame work in random node constructors or global callbacks.

## Threading

Default rule: graph mutation is main-thread only.

`GameRuntime::Init()`, `Update()`, and `Shutdown()` are main-thread only unless a
specific API explicitly says otherwise. Background work should marshal results
back through world/task-dispatch paths before touching world-owned state.

## Target Module Layout

Runtime core public contracts should move to a `Modules/GameFramework/Public/`
or equivalent module root. Runtime implementation should move to the matching
`Private/` tree. Current `include/` and `src/` locations are legacy current-state
facts.
