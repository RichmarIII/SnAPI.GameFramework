# SnAPI::GameFramework::IWorld

Root world contract for graph ownership, subsystem access, and frame execution.

`IWorld` is the central runtime abstraction that every higher-level gameplay or editor system talks to. It owns the authoritative node/component graph, exposes the optional subsystems bound into that graph, and defines the frame lifecycle used by `GameRuntime` and editor tooling.

Design intent:
- centralize object ownership so nodes and components have one authoritative lifetime
- separate public world semantics from the concrete `World` implementation
- let runtime, editor, and PIE share one API while using different execution profiles

Ownership and lifetime:
- The world owns node storage, runtime node/component records, and subsystem instances.
- Pointers and references returned from world lookup APIs are borrowed views.
- Implementations may defer actual destruction until `EndFrame` to preserve frame-stable handles.

Threading model:
- Unless a method explicitly says otherwise, graph mutation and direct node/component access are main-thread only.
- The interface itself is not generally thread-safe; external synchronization is required for concurrent use.

Invariants:
- `NodeHandle` / `ComponentHandle` are the stable public identity boundary.
- Execution policy queries (`ShouldRunGameplay()`, `ShouldTickUI()`, and similar) describe what the world will do this frame.
- Subsystem accessors return live subsystem instances owned by the world.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::IWorld::RuntimeChildVisitor = void(*)(void* UserData, RuntimeNodeHandle Child)`

Callback used when iterating runtime child nodes without allocating a snapshot array.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::IWorld::NodeVisitor = void(*)(void* UserData, const NodeHandle& Handle, BaseNode& Node)`

Callback used when iterating concrete world-owned node objects.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IWorld::~IWorld()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual EWorldKind SnAPI::GameFramework::IWorld::Kind() const =0`

World role classification.

**Returns:** Active world kind.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldRunGameplay() const =0`

Whether high-level gameplay orchestration should run for this world.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldTickInput() const =0`

Whether input pumping should run during variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldTickUI() const =0`

Whether UI context tick should run during variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldPumpNetworking() const =0`

Whether networking queues/session pumps should run.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldSimulatePhysics() const =0`

Whether physics simulation stepping should run.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldAllowPhysicsQueries() const =0`

Whether physics query access should be considered valid.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldTickAudio() const =0`

Whether audio subsystem update should run.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldRunNodeEndFrame() const =0`

Whether node/component end-frame flush should run.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldBuildUiRenderPackets() const =0`

Whether UI render packet generation/queueing should run.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::ShouldRenderFrame() const =0`

Whether renderer end-frame submission should run.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TObjectPool< BaseNode > & SnAPI::GameFramework::IWorld::NodePool()=0`

Access world-owned node pool storage.

**Returns:** Mutable node pool reference.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const TObjectPool< BaseNode > & SnAPI::GameFramework::IWorld::NodePool() const =0`

Access world-owned node pool storage (const).

**Returns:** Const node pool reference.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::ForEachNode(NodeVisitor Visitor, void *UserData)=0`

Iterate all world-owned nodes.

**Parameters**

- `Visitor`: Callback invoked for each node.
- `UserData`: Opaque callback context pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< NodeHandle > SnAPI::GameFramework::IWorld::NodeHandleById(const Uuid &Id) const =0`

Resolve node handle by UUID (slow path).

**Parameters**

- `Id`: Node UUID.

**Returns:** Node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< NodeHandle > SnAPI::GameFramework::IWorld::CreateNode(const TypeId &Type, std::string Name)=0`

Create a node by reflected type.

**Parameters**

- `Type`: Reflected node type id.
- `Name`: Node name.

**Returns:** Node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< NodeHandle > SnAPI::GameFramework::IWorld::CreateNodeWithId(const TypeId &Type, std::string Name, const Uuid &Id)=0`

Create a node by reflected type with explicit UUID.

**Parameters**

- `Type`: Reflected node type id.
- `Name`: Node name.
- `Id`: Explicit node UUID.

**Returns:** Node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::DestroyNode(const NodeHandle &Handle)=0`

Destroy a node.

**Parameters**

- `Handle`: Node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::AttachChild(const NodeHandle &Parent, const NodeHandle &Child)=0`

Attach child under parent.

**Parameters**

- `Parent`: Parent node handle.
- `Child`: Child node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::DetachChild(const NodeHandle &Child)=0`

Detach child from parent.

**Parameters**

- `Child`: Child node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void * SnAPI::GameFramework::IWorld::BorrowedComponent(const NodeHandle &Owner, const TypeId &Type)=0`

Borrow component instance by owner/type.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Component pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const void * SnAPI::GameFramework::IWorld::BorrowedComponent(const NodeHandle &Owner, const TypeId &Type) const =0`

Borrow component instance by owner/type (const).

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Component pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::RemoveComponentByType(const NodeHandle &Owner, const TypeId &Type)=0`

Remove a component by owner/type.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void * > SnAPI::GameFramework::IWorld::CreateComponent(const NodeHandle &Owner, const TypeId &Type)=0`

Create a component by owner/type.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Raw component pointer or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< void * > SnAPI::GameFramework::IWorld::CreateComponentWithId(const NodeHandle &Owner, const TypeId &Type, const Uuid &Id)=0`

Create a component by owner/type with explicit UUID.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.
- `Id`: Explicit component UUID.

**Returns:** Raw component pointer or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::RequestNodeOnCreate(const NodeHandle &Handle)=0`

Request node `OnCreate` execution for a world-owned node.

**Parameters**

- `Handle`: Target node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::AreNodeOnCreateCallbacksDeferred() const =0`

Check whether node `OnCreate` invocations are currently deferred.

**Returns:** True when node create callbacks will be queued instead of invoked immediately.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::Tick(float DeltaSeconds)=0`

Per-frame tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::FixedTick(float DeltaSeconds)=0`

Fixed-step tick.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::LateTick(float DeltaSeconds)=0`

Late tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::EndFrame()=0`

End-of-frame processing.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::FixedTickEnabled() const =0`

Report whether the runtime currently drives a fixed-step simulation loop.

**Returns:** True when fixed-step simulation is enabled for the current frame.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual float SnAPI::GameFramework::IWorld::FixedTickDeltaSeconds() const =0`

Get active fixed-step delta seconds.

**Returns:** Fixed simulation step interval in seconds (0 when fixed tick is disabled).
</div>
<div class="snapi-api-card" markdown="1">
### `virtual float SnAPI::GameFramework::IWorld::FixedTickInterpolationAlpha() const =0`

Get current render interpolation alpha between fixed simulation samples.

**Returns:** Alpha in range [0, 1].
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< NodeHandle > SnAPI::GameFramework::IWorld::CreateLevel(std::string Name)=0`

Create a level as a child node.

**Parameters**

- `Name`: Level name.

**Returns:** Handle to the created level or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpectedRef< Level > SnAPI::GameFramework::IWorld::LevelRef(const NodeHandle &Handle)=0`

Access a level by handle.

**Parameters**

- `Handle`: Level handle.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeNodeHandle > SnAPI::GameFramework::IWorld::CreateRuntimeNode(std::string Name, const TypeId &Type)=0`

Create a world-owned runtime node record in ECS storage.

**Parameters**

- `Name`: Node display/debug name.
- `Type`: Runtime type id.

**Returns:** Runtime node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeNodeHandle > SnAPI::GameFramework::IWorld::CreateRuntimeNodeWithId(const Uuid &Id, std::string Name, const TypeId &Type)=0`

Create a world-owned runtime node record with explicit UUID.

**Parameters**

- `Id`: Explicit node UUID.
- `Name`: Node display/debug name.
- `Type`: Runtime type id.

**Returns:** Runtime node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::DestroyRuntimeNode(RuntimeNodeHandle Handle)=0`

Destroy a runtime node (recursive for descendants).

**Parameters**

- `Handle`: Runtime node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::AttachRuntimeChild(RuntimeNodeHandle Parent, RuntimeNodeHandle Child)=0`

Attach a runtime child node to a parent.

**Parameters**

- `Parent`: Parent runtime node handle.
- `Child`: Child runtime node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::DetachRuntimeChild(RuntimeNodeHandle Child)=0`

Detach a runtime child node from its parent.

**Parameters**

- `Child`: Child runtime node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeNodeHandle > SnAPI::GameFramework::IWorld::RuntimeNodeById(const Uuid &Id) const =0`

Resolve runtime node handle by UUID.

**Parameters**

- `Id`: Runtime node UUID.

**Returns:** Runtime node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual RuntimeNodeHandle SnAPI::GameFramework::IWorld::RuntimeParent(RuntimeNodeHandle Child) const =0`

Get runtime parent for a node.

**Parameters**

- `Child`: Child runtime node handle.

**Returns:** Parent runtime node handle (null when root or invalid).
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::vector< RuntimeNodeHandle > SnAPI::GameFramework::IWorld::RuntimeChildren(RuntimeNodeHandle Parent) const =0`

Get runtime children for a node.

**Parameters**

- `Parent`: Parent runtime node handle.

**Returns:** Child runtime handles.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::ForEachRuntimeChild(RuntimeNodeHandle Parent, RuntimeChildVisitor Visitor, void *UserData) const =0`

Iterate runtime children for a node without allocating snapshots.

**Parameters**

- `Parent`: Parent runtime node handle.
- `Visitor`: Callback invoked for each alive child.
- `UserData`: Opaque callback context pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::vector< RuntimeNodeHandle > SnAPI::GameFramework::IWorld::RuntimeRoots() const =0`

Get runtime root nodes for the world.

**Returns:** Root runtime handles.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeComponentHandle > SnAPI::GameFramework::IWorld::AddRuntimeComponent(RuntimeNodeHandle Owner, const TypeId &Type)=0`

Add a runtime component to a runtime node by reflected type.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** Runtime component handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeComponentHandle > SnAPI::GameFramework::IWorld::AddRuntimeComponentWithId(RuntimeNodeHandle Owner, const TypeId &Type, const Uuid &Id)=0`

Add a runtime component with explicit UUID identity.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.
- `Id`: Explicit runtime component UUID.

**Returns:** Runtime component handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::IWorld::RemoveRuntimeComponent(RuntimeNodeHandle Owner, const TypeId &Type)=0`

Remove a runtime component from a runtime node by type.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IWorld::HasRuntimeComponent(RuntimeNodeHandle Owner, const TypeId &Type) const =0`

Check if runtime node has a runtime component type attached.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** True when attached.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeComponentHandle > SnAPI::GameFramework::IWorld::RuntimeComponentByType(RuntimeNodeHandle Owner, const TypeId &Type) const =0`

Get runtime component handle attached to runtime node by type.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** Runtime component handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void * SnAPI::GameFramework::IWorld::ResolveRuntimeComponentRaw(RuntimeComponentHandle Handle, const TypeId &Type)=0`

Resolve runtime component raw pointer from handle and type.

**Parameters**

- `Handle`: Runtime component handle.
- `Type`: Runtime component type id.

**Returns:** Mutable raw pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const void * SnAPI::GameFramework::IWorld::ResolveRuntimeComponentRaw(RuntimeComponentHandle Handle, const TypeId &Type) const =0`

Resolve runtime component raw pointer from handle and type (const).

**Parameters**

- `Handle`: Runtime component handle.
- `Type`: Runtime component type id.

**Returns:** Const raw pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual WorldEcsRuntime & SnAPI::GameFramework::IWorld::EcsRuntime()=0`

Access world-owned ECS typed storage runtime.

**Returns:** Mutable runtime storage registry.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const WorldEcsRuntime & SnAPI::GameFramework::IWorld::EcsRuntime() const =0`

Access world-owned ECS typed storage runtime (const).

**Returns:** Const runtime storage registry.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual ScriptRuntimeService & SnAPI::GameFramework::IWorld::Scripts()=0`

Access the scripting runtime service for this world.

**Returns:** Mutable scripting runtime service.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const ScriptRuntimeService & SnAPI::GameFramework::IWorld::Scripts() const =0`

Access the scripting runtime service for this world (const).

**Returns:** Const scripting runtime service.
</div>
