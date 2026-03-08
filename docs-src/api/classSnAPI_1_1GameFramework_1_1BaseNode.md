# SnAPI::GameFramework::BaseNode

Canonical base type for world-owned scene graph nodes.

`BaseNode` is the user-visible object that represents one graph element in a `World`. It carries the durable identity, hierarchy state, reflected type information, and the convenience API used to query or attach runtime components. Most gameplay-facing node types should derive from `BaseNode` rather than inventing a parallel ownership model.

Why this type exists:
- it gives gameplay code an address-stable object to reason about while the world owns storage
- it keeps hierarchy, identity, and reflected type metadata in one place
- it separates user-facing node semantics from the lower-level ECS/runtime records stored in `WorldEcsRuntime`

Ownership and lifetime:
- `IWorld` owns node lifetime and backing storage.
- `BaseNode` never owns its parent, children, world, or attached components directly.
- `Handle()` is the canonical public identity. Raw pointers obtained through handles or iteration are borrowed views.
- `OnCreate()` delivery may be deferred by the world during bootstrap until dependent subsystems are ready.

Threading model:
- Main-thread only for hierarchy mutation, component mutation, and most direct node access.
- Read-only access from other threads is not guaranteed safe unless external synchronization is provided.

Invariants:
- `TypeKey()` must match the concrete reflected node type.
- `Handle().Id` is the stable node identity used for serialization, replication, and registry lookup.
- `World()` is non-owning and may be null only when the node is detached from a live world.

Performance notes:
- Child lists and component masks are stored directly on the node for hot-path traversal.
- Use handles across frames; borrowed pointers should be treated as temporary frame-local views.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::BaseNode::kTypeName`

Stable type name used for reflection.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::m_self`

Stable runtime identity handle for this node.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::m_parent`

Parent identity; null indicates this node is a root in world hierarchy.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::BaseNode::m_children`

Ordered child identity list used for deterministic traversal.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<BaseNode*> SnAPI::GameFramework::BaseNode::m_childNodes`

Child pointer cache aligned with `m_children` to reduce handle resolves.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::BaseNode::m_name`

Human-readable/debug name (not required to be unique).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::m_active`

Local execution gate used by tree traversal.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::m_replicated`

Runtime replication gate for networking bridges.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::m_pendingDestroy`

True when this node has been scheduled for end-of-frame destruction.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::m_editorTransient`

True when this node is an editor-only transient helper and must not be persisted.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::BaseNode::m_componentTypes`

Attached component type ids for introspection and fast feature checks.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ComponentStorageView*> SnAPI::GameFramework::BaseNode::m_componentStorages`

Attached component storage cache aligned with m_componentTypes.
</div>
<div class="snapi-api-card" markdown="1">
### `RelevanceComponent* SnAPI::GameFramework::BaseNode::m_relevanceComponent`

Cached relevance component pointer for hot-path activation checks.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint64_t> SnAPI::GameFramework::BaseNode::m_componentMask`

Dense bitmask mirror of `m_componentTypes` for fast `Has<T>` checks.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseNode::m_maskVersion`

Last component-type-registry version this mask was synchronized against.
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld* SnAPI::GameFramework::BaseNode::m_world`

Non-owning pointer to world context for subsystem access and role queries.
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeNodeHandle SnAPI::GameFramework::BaseNode::m_runtimeNode`

Cached world-runtime handle for fast runtime hierarchy access.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::BaseNode::m_typeId`

Reflected type identity used by serialization/rpc/replication metadata lookups.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::BaseNode::BaseNode()`

Construct a node with default name.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::BaseNode::BaseNode(std::string InName)`

Construct a node with a custom name.

**Parameters**

- `InName`: Node name.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::BaseNode::~BaseNode()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::OnCreate()`

Node construction lifecycle hook.

Override in derived types to perform work that requires the node to already be registered with its world and fully assigned an identity. Worlds may defer this callback during bootstrap so render/UI-dependent nodes do not run before subsystems are ready.

Threading:
- Main-thread only.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::OnDestroy()`

Node destruction lifecycle hook.

Called before the node is finally removed from world-owned storage. Use this to release world-facing runtime state that should be torn down while the world and its subsystems are still valid.

Threading:
- Main-thread only.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::PreTick(float DeltaSeconds)`

Early variable-step update hook executed before `Tick`.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Tick(float DeltaSeconds)`

Primary variable-step update hook.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::FixedTick(float DeltaSeconds)`

Fixed-step update hook used for deterministic simulation.

**Parameters**

- `DeltaSeconds`: Fixed simulation step in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::LateTick(float DeltaSeconds)`

Late variable-step hook executed after `Tick`.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::PostTick(float DeltaSeconds)`

Post-update hook executed after the regular variable-step phases.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::EndFrame()`

End-of-frame hook executed during `World::EndFrame` when enabled by the world execution profile.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::BaseNode::Name() const`

Get the node name.

**Returns:** Name string.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Name(std::string Name)`

Set the node name.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::Handle() const`

Get the node handle.

**Returns:** NodeHandle for this node.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Handle(const NodeHandle &Handle)`

Set the node handle.

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `const Uuid & SnAPI::GameFramework::BaseNode::Id() const`

Get the node UUID.

**Returns:** UUID value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Id(Uuid Id)`

Set the node UUID.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::BaseNode::TypeKey() const`

Get the reflected type id for this node.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::TypeKey(const TypeId &Id)`

Set the reflected type id for this node.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::Parent() const`

Get the parent node handle.

**Returns:** Parent handle or null handle if root.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Parent(const NodeHandle &Parent)`

Set the parent node handle.

**Parameters**

- `Parent`:
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< NodeHandle > & SnAPI::GameFramework::BaseNode::Children() const`

Get the list of child handles.

**Returns:** Vector of child handles.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::AddChild(const NodeHandle &Child)`

Add a child handle to the node.

**Parameters**

- `Child`: Child handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::AddChildResolved(const NodeHandle &Child, BaseNode *ChildNode)`

Add a child with a resolved pointer cache entry.

**Parameters**

- `Child`: Child handle.
- `ChildNode`: Resolved child node pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::RemoveChild(const NodeHandle &Child)`

Remove a child handle from the node.

**Parameters**

- `Child`: Child handle to remove.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::Active() const`

Check if the node is active.

**Returns:** True if active.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Active(bool Active)`

Set the active state for the node.

**Parameters**

- `Active`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::Replicated() const`

Check if the node is replicated over the network.

**Returns:** True if replicated.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Replicated(bool Replicated)`

Set whether the node is replicated over the network.

**Parameters**

- `Replicated`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::PendingDestroy() const`

Check whether this node is queued for deferred destruction.

**Returns:** True when destruction has been scheduled but not yet flushed.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::PendingDestroy(bool Pending)`

Mark whether this node is queued for deferred destruction.

**Parameters**

- `Pending`: New pending-destroy state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::EditorTransient() const`

Check whether this node is editor-transient and should be excluded from persistence.

**Returns:** True when the node is flagged transient for editor preview/runtime-only use.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::EditorTransient(const bool Transient)`

Mark this node as editor-transient.

**Parameters**

- `Transient`: New transient state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::IsServer() const`

True when this node executes with server authority.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::IsClient() const`

True when this node executes in client context.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::IsListenServer() const`

True when this node executes as listen-server.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::OnPossess(const NodeHandle &PlayerHandle)`

Possession callback invoked when a LocalPlayer begins possessing this node.

**Parameters**

- `PlayerHandle`: Handle of the possessing LocalPlayer.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::OnUnpossess(const NodeHandle &PlayerHandle)`

Possession callback invoked when a LocalPlayer stops possessing this node.

**Parameters**

- `PlayerHandle`: Handle of the unpossessing LocalPlayer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::CallRPC(std::string_view MethodName, std::span< const Variant > Args={})`

Dispatch a reflected RPC method for this node.

**Parameters**

- `MethodName`: Reflected method name.
- `Args`: Variant-packed arguments.

**Returns:** True when dispatch succeeded (local invoke or queued network call).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::CallRPC(std::string_view MethodName, std::initializer_list< Variant > Args)`

Initializer-list convenience overload for `CallRPC`.

**Parameters**

- `MethodName`: 
- `Args`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< TypeId > & SnAPI::GameFramework::BaseNode::ComponentTypes()`

Access the list of component type ids.

**Returns:** Mutable reference to the type id list.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< TypeId > & SnAPI::GameFramework::BaseNode::ComponentTypes() const`

Access the list of component type ids (const).

**Returns:** Const reference to the type id list.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< ComponentStorageView * > & SnAPI::GameFramework::BaseNode::ComponentStorages()`

Access attached component storages for this node.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< ComponentStorageView * > & SnAPI::GameFramework::BaseNode::ComponentStorages() const`

Access attached component storages for this node (const).
</div>
<div class="snapi-api-card" markdown="1">
### `RelevanceComponent * SnAPI::GameFramework::BaseNode::RelevanceState()`

Get cached relevance component pointer for this node.

**Returns:** Relevance component pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `const RelevanceComponent * SnAPI::GameFramework::BaseNode::RelevanceState() const`

Get cached relevance component pointer for this node (const).

**Returns:** Relevance component pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::RelevanceState(RelevanceComponent *Relevance)`

Set cached relevance component pointer for this node.

**Parameters**

- `Relevance`: Relevance component pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< uint64_t > & SnAPI::GameFramework::BaseNode::ComponentMask()`

Access the component bitmask storage.

**Returns:** Mutable reference to the component mask.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< uint64_t > & SnAPI::GameFramework::BaseNode::ComponentMask() const`

Access the component bitmask storage (const).

**Returns:** Const reference to the component mask.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseNode::MaskVersion() const`

Get the component mask version.

**Returns:** Version id.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::MaskVersion(uint32_t Version)`

Set the component mask version.

**Parameters**

- `Version`: New version id.
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld * SnAPI::GameFramework::BaseNode::World() const`

Get the owning world for this node.

**Returns:** Pointer to the world interface or nullptr if unowned.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::World(IWorld *InWorld)`

Set the owning world for this node.

**Parameters**

- `InWorld`: World interface pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeNodeHandle SnAPI::GameFramework::BaseNode::RuntimeNode() const`

Get cached world-runtime node handle for this node.

**Returns:** Runtime node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::RuntimeNode(const RuntimeNodeHandle Handle)`

Set cached world-runtime node handle for this node.

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TDenseRuntimeHandle< T > > SnAPI::GameFramework::BaseNode::AddRuntimeComponent(Args &&... args)`

Add a world-owned runtime ECS component to this node.

**Parameters**

- `args`: Constructor arguments for the runtime component.

**Returns:** Runtime typed handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TDenseRuntimeHandle< T > > SnAPI::GameFramework::BaseNode::AddRuntimeComponentWithId(const Uuid &Id, Args &&... args)`

Add a world-owned runtime ECS component with explicit UUID.

**Parameters**

- `Id`: 
- `args`: Constructor arguments for the runtime component.

**Returns:** Runtime typed handle or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::BaseNode::RuntimeComponent()`

Borrow a world-owned runtime ECS component attached to this node.

**Returns:** Mutable reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::HasRuntimeComponent() const`

Check whether this node has a world-owned runtime ECS component type.

**Returns:** True when attached.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::BaseNode::RemoveRuntimeComponent()`

Remove a world-owned runtime ECS component type from this node.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::BaseNode::Add(Args &&... args)`

Add a component of type T to this node.

**Parameters**

- `args`: Constructor arguments.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::BaseNode::Component()`

Get a component of type T from this node.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::Has() const`

Check if a component of type T exists on this node.

**Returns:** True if present.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Remove()`

Remove a component of type T from this node.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `RuntimeNodeHandle SnAPI::GameFramework::BaseNode::ResolveRuntimeNodeHandle() const`
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeNodeHandle SnAPI::GameFramework::BaseNode::ResolveRuntimeNodeHandleAndCache()`
</div>
