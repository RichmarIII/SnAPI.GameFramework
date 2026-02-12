# SnAPI::GameFramework::INode

Abstract runtime contract for graph nodes.

Semantics:
- A node is identity-first (`Handle` / `Id`) and hierarchy-aware (`Parent` / `Children`).
- Runtime ownership is external: `NodeGraph` controls insertion/removal and lifecycle.
- `World` association is optional for detached/prefab graphs, but world-backed behavior (networking/audio subsystems, authoritative role queries, tick-tree participation) depends on a valid `World()` pointer.

Implementers:
- `BaseNode` is the canonical implementation and should be preferred.
- Implementing `INode` directly is valid but requires preserving all invariants described on each accessor/mutator below.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::INode::~INode()=default`

Virtual destructor for interface.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Tick(float DeltaSeconds)`

Per-frame update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::FixedTick(float DeltaSeconds)`

Fixed-step update hook.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::LateTick(float DeltaSeconds)`

Late update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const std::string & SnAPI::GameFramework::INode::Name() const =0`

Get the display name of the node.

**Returns:** Node name.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Name(std::string Name)=0`

Set the display name of the node.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::INode::Handle() const =0`

Get the handle for this node.

**Returns:** Node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Handle(NodeHandle Handle)=0`

Set the node handle.

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const Uuid & SnAPI::GameFramework::INode::Id() const =0`

Get the node UUID.

**Returns:** UUID value.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Id(Uuid Id)=0`

Set the node UUID.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const TypeId & SnAPI::GameFramework::INode::TypeKey() const =0`

Get the reflected type id for this node.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::TypeKey(const TypeId &Id)=0`

Set the reflected type id for this node.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeHandle SnAPI::GameFramework::INode::Parent() const =0`

Get the parent node handle.

**Returns:** Parent handle or null handle if root.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Parent(NodeHandle Parent)=0`

Set the parent node handle.

**Parameters**

- `Parent`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const std::vector< NodeHandle > & SnAPI::GameFramework::INode::Children() const =0`

Get the list of child handles.

**Returns:** Vector of child handles.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::AddChild(NodeHandle Child)=0`

Add a child handle to the node.

**Parameters**

- `Child`: Child handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::RemoveChild(NodeHandle Child)=0`

Remove a child handle from the node.

**Parameters**

- `Child`: Child handle to remove.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::INode::Active() const =0`

Check if the node is active.

**Returns:** True if active.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Active(bool Active)=0`

Set the active state for the node.

**Parameters**

- `Active`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::INode::Replicated() const =0`

Check if the node is replicated over the network.

**Returns:** True if replicated.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::Replicated(bool Replicated)=0`

Set whether the node is replicated over the network.

**Parameters**

- `Replicated`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::INode::IsServer() const =0`

Check whether this node is executing with server authority.

**Returns:** True when server-authoritative.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::INode::IsClient() const =0`

Check whether this node is executing in a client context.

**Returns:** True when client-side.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::INode::IsListenServer() const =0`

Check whether this node is executing as a listen-server.

**Returns:** True when both server and client role are active.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::vector< TypeId > & SnAPI::GameFramework::INode::ComponentTypes()=0`

Access the list of component type ids.

**Returns:** Mutable reference to the type id list.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const std::vector< TypeId > & SnAPI::GameFramework::INode::ComponentTypes() const =0`

Access the list of component type ids (const).

**Returns:** Const reference to the type id list.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::vector< uint64_t > & SnAPI::GameFramework::INode::ComponentMask()=0`

Access the component bitmask storage.

**Returns:** Mutable reference to the component mask.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const std::vector< uint64_t > & SnAPI::GameFramework::INode::ComponentMask() const =0`

Access the component bitmask storage (const).

**Returns:** Const reference to the component mask.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual uint32_t SnAPI::GameFramework::INode::MaskVersion() const =0`

Get the component mask version.

**Returns:** Version id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::MaskVersion(uint32_t Version)=0`

Set the component mask version.

**Parameters**

- `Version`: New version id.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual NodeGraph * SnAPI::GameFramework::INode::OwnerGraph() const =0`

Get the owning graph.

**Returns:** Pointer to owner graph or nullptr if unowned.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::OwnerGraph(NodeGraph *Graph)=0`

Set the owning graph.

**Parameters**

- `Graph`: Owner graph pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual IWorld * SnAPI::GameFramework::INode::World() const =0`

Get the owning world for this node.

**Returns:** Pointer to the world interface or nullptr if unowned.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::World(IWorld *InWorld)=0`

Set the owning world for this node.

**Parameters**

- `InWorld`: World interface pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::TickTree(float DeltaSeconds)=0`

Tick this node and its subtree.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::FixedTickTree(float DeltaSeconds)=0`

Fixed-step tick for this node and its subtree.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::INode::LateTickTree(float DeltaSeconds)=0`

Late tick for this node and its subtree.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
