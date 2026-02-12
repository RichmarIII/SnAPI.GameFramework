# SnAPI::GameFramework::NodeGraph

Hierarchical runtime container for nodes/components.

Core semantics:
- Node/component identity is UUID-handle based.
- Object lifetime is deferred-destruction by default (`DestroyNode` + `EndFrame`).
- Component storage is type-partitioned (`TypeId -> IComponentStorage`) and created lazily.
- Graph tick methods evaluate relevance and then drive deterministic tree traversal.

This class is the primary runtime backbone for world/level/prefab style object trees.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::NodeGraph::kTypeName`

Stable type name for reflection.
</div>

## Friends

<div class="snapi-api-card" markdown="1">
### `friend class BaseNode`
</div>
<div class="snapi-api-card" markdown="1">
### `friend class ComponentSerializationRegistry`
</div>
<div class="snapi-api-card" markdown="1">
### `friend class NodeGraphSerializer`
</div>
<div class="snapi-api-card" markdown="1">
### `friend class NetReplicationBridge`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<TObjectPool<BaseNode> > SnAPI::GameFramework::NodeGraph::m_nodePool`

Owning node pool providing stable addresses and deferred destroy semantics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, std::unique_ptr<IComponentStorage>, UuidHash> SnAPI::GameFramework::NodeGraph::m_storages`

Type-partitioned component storages created lazily on demand.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::NodeGraph::m_rootNodes`

Root traversal entry points (nodes without parent in this graph).
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::NodeGraph::m_pendingDestroy`

Node handles queued for end-of-frame destruction.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::NodeGraph::m_relevanceCursor`

Cursor for incremental relevance sweeps when budgeted evaluation is enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::NodeGraph::m_relevanceBudget`

Per-tick relevance evaluation cap; 0 means evaluate all nodes.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::NodeGraph::NodeGraph(const NodeGraph &)=delete`

Non-copyable.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph & SnAPI::GameFramework::NodeGraph::operator=(const NodeGraph &)=delete`

Non-copyable.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::NodeGraph::NodeGraph(NodeGraph &&Other) noexcept`

Move construct a graph.

**Parameters**

- `Other`: Graph to move from.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph & SnAPI::GameFramework::NodeGraph::operator=(NodeGraph &&Other) noexcept`

Move assign a graph.

**Parameters**

- `Other`: Graph to move from.

**Returns:** Reference to this graph.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::NodeGraph::NodeGraph()`

Construct an empty graph with default name.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::NodeGraph::NodeGraph(std::string Name)`

Construct an empty graph with a name.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::NodeGraph::~NodeGraph() override`

Destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::NodeGraph::CreateNode(std::string Name, Args &&... args)`

Create a node of type T with a generated UUID.

**Parameters**

- `Name`: 
- `args`: Constructor arguments for T.

**Returns:** Handle to the created node or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::NodeGraph::CreateNodeWithId(const Uuid &Id, std::string Name, Args &&... args)`

Create a node of type T with an explicit UUID.

**Parameters**

- `Id`: 
- `Name`: 
- `args`: Constructor arguments for T.

**Returns:** Handle to the created node or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::NodeGraph::CreateNode(const TypeId &Type, std::string Name)`

Create a node by reflected TypeId with a generated UUID.

**Parameters**

- `Type`: Reflected type id.
- `Name`: 

**Returns:** Handle to the created node or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::NodeGraph::CreateNode(const TypeId &Type, std::string Name, const Uuid &Id)`

Create a node by reflected TypeId with an explicit UUID.

**Parameters**

- `Type`: Reflected type id.
- `Name`: 
- `Id`: 

**Returns:** Handle to the created node or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::NodeGraph::DestroyNode(NodeHandle Handle)`

Destroy a node at end-of-frame.

**Parameters**

- `Handle`: 

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::NodeGraph::AttachChild(NodeHandle Parent, NodeHandle Child)`

Attach a child node to a parent.

**Parameters**

- `Parent`: 
- `Child`: Child handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::NodeGraph::DetachChild(NodeHandle Child)`

Detach a child node from its parent.

**Parameters**

- `Child`: Child handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::Tick(float DeltaSeconds) override`

Tick the graph (relevance + node tree).

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::FixedTick(float DeltaSeconds) override`

Fixed-step tick for the graph.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::LateTick(float DeltaSeconds) override`

Late tick for the graph.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::EndFrame()`

Process end-of-frame destruction for nodes/components.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::Clear()`

Remove all nodes/components immediately.
</div>
<div class="snapi-api-card" markdown="1">
### `TObjectPool< BaseNode > & SnAPI::GameFramework::NodeGraph::NodePool()`

Access the node pool (mutable).

**Returns:** Reference to the pool.
</div>
<div class="snapi-api-card" markdown="1">
### `const TObjectPool< BaseNode > & SnAPI::GameFramework::NodeGraph::NodePool() const`

Access the node pool (const).

**Returns:** Const reference to the pool.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::NodeGraph::RelevanceBudget() const`

Get the relevance evaluation budget.

**Returns:** Max number of nodes evaluated per tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::RelevanceBudget(size_t Budget)`

Set the relevance evaluation budget.

**Parameters**

- `Budget`: Max number of nodes evaluated per tick (0 = unlimited).
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::NodeGraph::AddComponent(NodeHandle Owner, Args &&... args)`

Add a component of type T to a node.

**Parameters**

- `Owner`: Owner node handle.
- `args`: Constructor arguments.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::NodeGraph::AddComponentWithId(NodeHandle Owner, const Uuid &Id, Args &&... args)`

Add a component of type T to a node with explicit UUID.

**Parameters**

- `Owner`: Owner node handle.
- `Id`: 
- `args`: Constructor arguments.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::NodeGraph::Component(NodeHandle Owner)`

Get a component of type T from a node.

**Parameters**

- `Owner`: Owner node handle.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodeGraph::HasComponent(NodeHandle Owner) const`

Check if a node has a component of type T.

**Parameters**

- `Owner`: Owner node handle.

**Returns:** True if the component exists.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::RemoveComponent(NodeHandle Owner)`

Remove a component of type T from a node.

**Parameters**

- `Owner`: Owner node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::TickComponents(NodeHandle Owner, float DeltaSeconds)`

Tick all components for a node.

**Parameters**

- `Owner`: Owner node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::FixedTickComponents(NodeHandle Owner, float DeltaSeconds)`

Fixed-step tick all components for a node.

**Parameters**

- `Owner`: Owner node handle.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::LateTickComponents(NodeHandle Owner, float DeltaSeconds)`

Late tick all components for a node.

**Parameters**

- `Owner`: Owner node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::EvaluateRelevance()`

Evaluate relevance policies to enable/disable nodes.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::NodeGraph::IsNodeActive(NodeHandle Handle)`

Check whether a node is active for ticking.

**Parameters**

- `Handle`: 

**Returns:** True if node is active and relevant.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::RegisterComponentOnNode(BaseNode &Node, const TypeId &Type)`

Register a component type on a node's type list/mask.

**Parameters**

- `Node`: Node to update.
- `Type`: Component type id.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::UnregisterComponentOnNode(BaseNode &Node, const TypeId &Type)`

Unregister a component type from a node's type list/mask.

**Parameters**

- `Node`: Node to update.
- `Type`: Component type id.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::NodeGraph::RebindOwnerGraph()`

Rebind owner graph pointers after move.
</div>
<div class="snapi-api-card" markdown="1">
### `TComponentStorage< T > & SnAPI::GameFramework::NodeGraph::StorageFor()`

Get or create a component storage for type T.

**Returns:** Reference to the storage.
</div>
<div class="snapi-api-card" markdown="1">
### `IComponentStorage * SnAPI::GameFramework::NodeGraph::Storage(const TypeId &Type)`

Get a component storage by type id (mutable).

**Parameters**

- `Type`: Component type id.

**Returns:** Pointer to storage or nullptr if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `const IComponentStorage * SnAPI::GameFramework::NodeGraph::Storage(const TypeId &Type) const`

Get a component storage by type id (const).

**Parameters**

- `Type`: Component type id.

**Returns:** Pointer to storage or nullptr if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::NodeGraph::BorrowedComponent(NodeHandle Owner, const TypeId &Type)`

Borrow a component instance by owner/type (mutable).

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component type id.

**Returns:** Pointer to component or nullptr if missing.

**Notes**

- Borrowed pointers must not be cached.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::NodeGraph::BorrowedComponent(NodeHandle Owner, const TypeId &Type) const`

Borrow a component instance by owner/type (const).

**Parameters**

- `Owner`: Owner node handle.
- `Type`: Component type id.

**Returns:** Pointer to component or nullptr if missing.
</div>
