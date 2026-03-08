# SnAPI::GameFramework::World

Concrete world implementation that owns graph storage, subsystems, and frame execution.

`World` is the default implementation behind `IWorld`. It owns the concrete node pool, ECS runtime state, script runtime, task dispatcher, and optional subsystems such as input, UI, networking, physics, audio, and rendering. `GameRuntime` typically owns exactly one `World` instance for the lifetime of a running session.

Design responsibilities:
- own all world-level object storage and identity registration
- drive frame phases (`Tick`, `FixedTick`, `LateTick`, `EndFrame`)
- expose subsystem access through one authoritative root
- mediate editor/runtime/PIE behavior through `WorldExecutionProfile`

Ownership and lifetime:
- `World` owns its subsystem instances directly.
- Nodes and runtime component records are owned by world-managed storage.
- Raw node/component pointers obtained from the world are borrowed and become invalid when the underlying object is destroyed.

Threading model:
- Main-thread only for graph mutation and frame execution.
- Background work should be marshaled back through the task-dispatch APIs instead of mutating world state directly.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::World::WorkTask = std::function<void(World&)>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::World::CompletionTask = std::function<void(const TaskHandle&)>`
</div>

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::World::kTypeName`

Stable type name for reflection.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::World::m_name`

World display/debug name.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<TObjectPool<BaseNode> > SnAPI::GameFramework::World::m_nodePool`

World-owned node storage.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::World::m_rootNodes`

Root nodes in world hierarchy.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::World::m_pendingDestroy`

Deferred node-destroy queue.
</div>
<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::World::m_threadMutex`

World-thread affinity guard for queued task execution.
</div>
<div class="snapi-api-card" markdown="1">
### `TSystemTaskQueue<World> SnAPI::GameFramework::World::m_taskQueue`

Cross-thread task handoff queue for world-thread callbacks.
</div>
<div class="snapi-api-card" markdown="1">
### `JobSystem SnAPI::GameFramework::World::m_jobSystem`

World-scoped job dispatch facade for framework/runtime tasks.
</div>
<div class="snapi-api-card" markdown="1">
### `WorldEcsRuntime SnAPI::GameFramework::World::m_ecsRuntime`

Centralized typed ECS storage owner for node/component runtime refactor.
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptRuntimeService SnAPI::GameFramework::World::m_scriptRuntime`

World-owned scripting runtime service.
</div>
<div class="snapi-api-card" markdown="1">
### `GameplayHost* SnAPI::GameFramework::World::m_gameplayHost`

Non-owning gameplay host pointer for runtime bridge access.
</div>
<div class="snapi-api-card" markdown="1">
### `EWorldKind SnAPI::GameFramework::World::m_worldKind`

Role/classification of this world instance.
</div>
<div class="snapi-api-card" markdown="1">
### `WorldExecutionProfile SnAPI::GameFramework::World::m_executionProfile`

Per-world frame-phase execution policy.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::World::m_pendingNodeOnCreate`

Deferred node OnCreate queue used during bootstrap barriers.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::m_deferNodeOnCreateCallbacks`

True while node OnCreate invocations should be queued.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::m_fixedTickEnabled`

Runtime fixed-step enable state for current frame.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::World::m_fixedTickDeltaSeconds`

Runtime fixed-step interval snapshot for current frame.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::World::m_fixedTickInterpolationAlpha`

Runtime interpolation alpha between fixed samples for current frame.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World()`

Construct a world with default name.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World(std::string Name)`

Construct a world with a name.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::~World() override`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World(const World &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `World & SnAPI::GameFramework::World::operator=(const World &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World(World &&) noexcept=default`
</div>
<div class="snapi-api-card" markdown="1">
### `World & SnAPI::GameFramework::World::operator=(World &&) noexcept=default`
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::World::Name() const`

Get world display name.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::Name(std::string NameValue)`

Set world display name.

**Parameters**

- `NameValue`: New world name.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::CreateNode(std::string NameValue, Args &&... args)`

Create a node of reflected type `T`.

**Parameters**

- `NameValue`: Display/debug name assigned to the node.
- `args`: Additional constructor arguments. Must be omitted; passing any value causes the call to fail with `EErrorCode::InvalidArgument`.

**Returns:** Handle to the created node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::CreateNodeWithId(const Uuid &Id, std::string NameValue, Args &&... args)`

Create a node of reflected type `T` with an explicit UUID.

**Parameters**

- `Id`: Explicit stable identity to assign to the node.
- `NameValue`: Display/debug name assigned to the node.
- `args`: Additional constructor arguments. Must be omitted; passing any value causes the call to fail with `EErrorCode::InvalidArgument`.

**Returns:** Handle to the created node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `EWorldKind SnAPI::GameFramework::World::Kind() const override`

World role classification.

**Returns:** Active world kind.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldRunGameplay() const override`

Whether high-level gameplay orchestration should run for this world.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldTickInput() const override`

Whether input pumping should run during variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldTickUI() const override`

Whether UI context tick should run during variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldPumpNetworking() const override`

Whether networking queues/session pumps should run.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldSimulatePhysics() const override`

Whether physics simulation stepping should run.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldAllowPhysicsQueries() const override`

Whether physics query access should be considered valid.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldTickAudio() const override`

Whether audio subsystem update should run.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldRunNodeEndFrame() const override`

Whether node/component end-frame flush should run.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldBuildUiRenderPackets() const override`

Whether UI render packet generation/queueing should run.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::ShouldRenderFrame() const override`

Whether renderer end-frame submission should run.
</div>
<div class="snapi-api-card" markdown="1">
### `TObjectPool< BaseNode > & SnAPI::GameFramework::World::NodePool() override`

Access world-owned node pool storage.

**Returns:** Mutable node pool reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const TObjectPool< BaseNode > & SnAPI::GameFramework::World::NodePool() const override`

Access world-owned node pool storage (const).

**Returns:** Const node pool reference.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::ForEachNode(NodeVisitor Visitor, void *UserData) override`

Iterate all world-owned nodes.

**Parameters**

- `Visitor`: Callback invoked for each node.
- `UserData`: Opaque callback context pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::NodeHandleById(const Uuid &Id) const override`

Resolve node handle by UUID (slow path).

**Parameters**

- `Id`: Node UUID.

**Returns:** Node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::CreateNode(const TypeId &Type, std::string Name) override`

Create a node by reflected type.

**Parameters**

- `Type`: Reflected node type id.
- `Name`: 

**Returns:** Node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::CreateNodeWithId(const TypeId &Type, std::string Name, const Uuid &Id) override`

Create a node by reflected type with explicit UUID.

**Parameters**

- `Type`: Reflected node type id.
- `Name`: 
- `Id`: Explicit node UUID.

**Returns:** Node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::DestroyNode(const NodeHandle &Handle) override`

Destroy a node.

**Parameters**

- `Handle`: Node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::AttachChild(const NodeHandle &Parent, const NodeHandle &Child) override`

Attach child under parent.

**Parameters**

- `Parent`: Parent node handle.
- `Child`: Child node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::DetachChild(const NodeHandle &Child) override`

Detach child from parent.

**Parameters**

- `Child`: Child node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::World::BorrowedComponent(const NodeHandle &Owner, const TypeId &Type) override`

Borrow component instance by owner/type.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Component pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::World::BorrowedComponent(const NodeHandle &Owner, const TypeId &Type) const override`

Borrow component instance by owner/type (const).

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Component pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::RemoveComponentByType(const NodeHandle &Owner, const TypeId &Type) override`

Remove a component by owner/type.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void * > SnAPI::GameFramework::World::CreateComponent(const NodeHandle &Owner, const TypeId &Type) override`

Create a component by owner/type.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.

**Returns:** Raw component pointer or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void * > SnAPI::GameFramework::World::CreateComponentWithId(const NodeHandle &Owner, const TypeId &Type, const Uuid &Id) override`

Create a component by owner/type with explicit UUID.

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component reflected type id.
- `Id`: Explicit component UUID.

**Returns:** Raw component pointer or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::RequestNodeOnCreate(const NodeHandle &Handle) override`

Request node `OnCreate` execution for a world-owned node.

**Parameters**

- `Handle`: Target node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::AreNodeOnCreateCallbacksDeferred() const override`

Check whether node `OnCreate` invocations are currently deferred.

**Returns:** True when node create callbacks will be queued instead of invoked immediately.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::IsServer() const`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::IsClient() const`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::IsListenServer() const`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::SetWorldKind(EWorldKind Kind)`

Set the high-level world kind used by runtime/editor code paths.

**Parameters**

- `Kind`:
</div>
<div class="snapi-api-card" markdown="1">
### `const WorldExecutionProfile & SnAPI::GameFramework::World::ExecutionProfile() const`

Get the active execution profile that gates frame phases and subsystem work.

**Returns:** Current execution profile by const reference.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::SetExecutionProfile(const WorldExecutionProfile &Profile)`

Replace the active execution profile.

**Parameters**

- `Profile`: New execution profile to apply.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::DeferNodeOnCreateCallbacks(bool Deferred)`

Enable or disable deferred node `OnCreate` delivery.

**Parameters**

- `Deferred`: When `true`, future node `OnCreate` requests are queued instead of invoked immediately.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::FlushDeferredNodeOnCreate()`

Flush all queued node `OnCreate` callbacks.

**Returns:** Success or the first callback error encountered.
</div>
<div class="snapi-api-card" markdown="1">
### `TaskHandle SnAPI::GameFramework::World::EnqueueTask(WorkTask InTask, CompletionTask OnComplete={})`

Enqueue work on the world (game) thread.

**Parameters**

- `InTask`: Work callback executed on world-thread affinity.
- `OnComplete`: Optional completion callback marshaled to caller dispatcher.

**Returns:** Task handle for wait/cancel polling.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::EnqueueThreadTask(std::function< void()> InTask) override`

Enqueue a generic thread task for dispatcher marshalling.

**Parameters**

- `InTask`: Callback to execute on the world thread.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::ExecuteQueuedTasks()`

Execute all queued tasks on the world thread.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::Tick(float DeltaSeconds) override`

Per-frame tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::FixedTick(float DeltaSeconds) override`

Fixed-step tick.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::LateTick(float DeltaSeconds) override`

Late tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::EndFrame() override`

End-of-frame processing.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::Clear()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::FixedTickEnabled() const override`

Check whether fixed-step simulation is enabled for this frame.

**Returns:** True when fixed-step simulation is active.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::World::FixedTickDeltaSeconds() const override`

Get fixed-step delta used by runtime this frame.

**Returns:** Fixed-step interval in seconds (0 when disabled).
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::World::FixedTickInterpolationAlpha() const override`

Get render interpolation alpha between fixed samples.

**Returns:** Alpha in [0, 1].
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::SetFixedTickFrameState(bool Enabled, float FixedDeltaSeconds, float InterpolationAlpha)`

Update runtime fixed-step timing snapshot consumed by components/systems.

**Parameters**

- `Enabled`: True when fixed simulation is active.
- `FixedDeltaSeconds`: Active fixed-step interval in seconds.
- `InterpolationAlpha`: Current interpolation alpha between fixed samples.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::CreateLevel(std::string Name) override`

Create a level as a child node.

**Parameters**

- `Name`: 

**Returns:** Handle to the created level or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< Level > SnAPI::GameFramework::World::LevelRef(const NodeHandle &Handle) override`

Access a level by handle.

**Parameters**

- `Handle`: Level handle.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeNodeHandle > SnAPI::GameFramework::World::CreateRuntimeNode(std::string Name, const TypeId &Type) override`

Create a world-owned runtime node record in ECS storage.

**Parameters**

- `Name`: 
- `Type`: Runtime type id.

**Returns:** Runtime node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeNodeHandle > SnAPI::GameFramework::World::CreateRuntimeNodeWithId(const Uuid &Id, std::string Name, const TypeId &Type) override`

Create a world-owned runtime node record with explicit UUID.

**Parameters**

- `Id`: Explicit node UUID.
- `Name`: 
- `Type`: Runtime type id.

**Returns:** Runtime node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::DestroyRuntimeNode(RuntimeNodeHandle Handle) override`

Destroy a runtime node (recursive for descendants).

**Parameters**

- `Handle`: Runtime node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::AttachRuntimeChild(RuntimeNodeHandle Parent, RuntimeNodeHandle Child) override`

Attach a runtime child node to a parent.

**Parameters**

- `Parent`: Parent runtime node handle.
- `Child`: Child runtime node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::DetachRuntimeChild(RuntimeNodeHandle Child) override`

Detach a runtime child node from its parent.

**Parameters**

- `Child`: Child runtime node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeNodeHandle > SnAPI::GameFramework::World::RuntimeNodeById(const Uuid &Id) const override`

Resolve runtime node handle by UUID.

**Parameters**

- `Id`: Runtime node UUID.

**Returns:** Runtime node handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeNodeHandle SnAPI::GameFramework::World::RuntimeParent(RuntimeNodeHandle Child) const override`

Get runtime parent for a node.

**Parameters**

- `Child`: Child runtime node handle.

**Returns:** Parent runtime node handle (null when root or invalid).
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< RuntimeNodeHandle > SnAPI::GameFramework::World::RuntimeChildren(RuntimeNodeHandle Parent) const override`

Get runtime children for a node.

**Parameters**

- `Parent`: Parent runtime node handle.

**Returns:** Child runtime handles.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::ForEachRuntimeChild(RuntimeNodeHandle Parent, RuntimeChildVisitor Visitor, void *UserData) const override`

Iterate runtime children for a node without allocating snapshots.

**Parameters**

- `Parent`: Parent runtime node handle.
- `Visitor`: Callback invoked for each alive child.
- `UserData`: Opaque callback context pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< RuntimeNodeHandle > SnAPI::GameFramework::World::RuntimeRoots() const override`

Get runtime root nodes for the world.

**Returns:** Root runtime handles.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::World::AddRuntimeComponent(RuntimeNodeHandle Owner, const TypeId &Type) override`

Add a runtime component to a runtime node by reflected type.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** Runtime component handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::World::AddRuntimeComponentWithId(RuntimeNodeHandle Owner, const TypeId &Type, const Uuid &Id) override`

Add a runtime component with explicit UUID identity.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.
- `Id`: Explicit runtime component UUID.

**Returns:** Runtime component handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::World::RemoveRuntimeComponent(RuntimeNodeHandle Owner, const TypeId &Type) override`

Remove a runtime component from a runtime node by type.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::World::HasRuntimeComponent(RuntimeNodeHandle Owner, const TypeId &Type) const override`

Check if runtime node has a runtime component type attached.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** True when attached.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::World::RuntimeComponentByType(RuntimeNodeHandle Owner, const TypeId &Type) const override`

Get runtime component handle attached to runtime node by type.

**Parameters**

- `Owner`: Runtime owner node handle.
- `Type`: Runtime component type id.

**Returns:** Runtime component handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::World::ResolveRuntimeComponentRaw(RuntimeComponentHandle Handle, const TypeId &Type) override`

Resolve runtime component raw pointer from handle and type.

**Parameters**

- `Handle`: Runtime component handle.
- `Type`: Runtime component type id.

**Returns:** Mutable raw pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::World::ResolveRuntimeComponentRaw(RuntimeComponentHandle Handle, const TypeId &Type) const override`

Resolve runtime component raw pointer from handle and type (const).

**Parameters**

- `Handle`: Runtime component handle.
- `Type`: Runtime component type id.

**Returns:** Const raw pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< NodeHandle > SnAPI::GameFramework::World::Levels() const`

Get all level handles.

**Returns:** Vector of level handles.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::SetGameplayHost(GameplayHost *Host)`

Set gameplay host pointer associated with this world runtime.

**Parameters**

- `Host`:
</div>
<div class="snapi-api-card" markdown="1">
### `GameplayHost * SnAPI::GameFramework::World::GameplayHostPtr()`

Access gameplay host pointer associated with this world runtime.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameplayHost * SnAPI::GameFramework::World::GameplayHostPtr() const`

Access gameplay host pointer associated with this world runtime (const).
</div>
<div class="snapi-api-card" markdown="1">
### `JobSystem & SnAPI::GameFramework::World::Jobs()`

Access the job system for parallel internal tasks.

**Returns:** Reference to JobSystem.
</div>
<div class="snapi-api-card" markdown="1">
### `WorldEcsRuntime & SnAPI::GameFramework::World::EcsRuntime() override`

Access centralized world ECS runtime storage.
</div>
<div class="snapi-api-card" markdown="1">
### `const WorldEcsRuntime & SnAPI::GameFramework::World::EcsRuntime() const override`

Access centralized world ECS runtime storage (const).
</div>
<div class="snapi-api-card" markdown="1">
### `ScriptRuntimeService & SnAPI::GameFramework::World::Scripts() override`

Access world scripting runtime service.

**Returns:** Mutable scripting runtime service.
</div>
<div class="snapi-api-card" markdown="1">
### `const ScriptRuntimeService & SnAPI::GameFramework::World::Scripts() const override`

Access world scripting runtime service (const).

**Returns:** Const scripting runtime service.
</div>
