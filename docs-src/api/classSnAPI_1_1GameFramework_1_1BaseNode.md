# SnAPI::GameFramework::BaseNode

Default node implementation for the scene graph.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::BaseNode::kTypeName`

Stable type name used for reflection.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::m_self`

Handle for this node.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::m_parent`

Parent handle (null if root).
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::BaseNode::m_children`

Child handles.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::BaseNode::m_name`

Display name.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::m_active`

Active state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::m_replicated`

Replication flag.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::BaseNode::m_componentTypes`

Component type ids present.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint64_t> SnAPI::GameFramework::BaseNode::m_componentMask`

Bitmask for component queries.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseNode::m_maskVersion`

Mask version for registry changes.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph* SnAPI::GameFramework::BaseNode::m_ownerGraph`

Owning graph (non-owning).
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld* SnAPI::GameFramework::BaseNode::m_world`

Owning world (non-owning).
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::BaseNode::m_typeId`

Reflected type id.
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
### `const std::string & SnAPI::GameFramework::BaseNode::Name() const override`

Get the node name.

**Returns:** Name string.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Name(std::string Name) override`

Set the node name.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::Handle() const override`

Get the node handle.

**Returns:** NodeHandle for this node.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Handle(NodeHandle Handle) override`

Set the node handle.

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `const Uuid & SnAPI::GameFramework::BaseNode::Id() const override`

Get the node UUID.

**Returns:** UUID value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Id(Uuid Id) override`

Set the node UUID.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::BaseNode::TypeKey() const override`

Get the reflected type id for this node.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::TypeKey(const TypeId &Id) override`

Set the reflected type id for this node.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseNode::Parent() const override`

Get the parent node handle.

**Returns:** Parent handle or null handle if root.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Parent(NodeHandle Parent) override`

Set the parent node handle.

**Parameters**

- `Parent`:
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< NodeHandle > & SnAPI::GameFramework::BaseNode::Children() const override`

Get the list of child handles.

**Returns:** Vector of child handles.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::AddChild(NodeHandle Child) override`

Add a child handle to the node.

**Parameters**

- `Child`: Child handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::RemoveChild(NodeHandle Child) override`

Remove a child handle from the node.

**Parameters**

- `Child`: Child handle to remove.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::Active() const override`

Check if the node is active.

**Returns:** True if active.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Active(bool Active) override`

Set the active state for the node.

**Parameters**

- `Active`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseNode::Replicated() const override`

Check if the node is replicated over the network.

**Returns:** True if replicated.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::Replicated(bool Replicated) override`

Set whether the node is replicated over the network.

**Parameters**

- `Replicated`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< TypeId > & SnAPI::GameFramework::BaseNode::ComponentTypes() override`

Access the list of component type ids.

**Returns:** Mutable reference to the type id list.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< TypeId > & SnAPI::GameFramework::BaseNode::ComponentTypes() const override`

Access the list of component type ids (const).

**Returns:** Const reference to the type id list.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< uint64_t > & SnAPI::GameFramework::BaseNode::ComponentMask() override`

Access the component bitmask storage.

**Returns:** Mutable reference to the component mask.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< uint64_t > & SnAPI::GameFramework::BaseNode::ComponentMask() const override`

Access the component bitmask storage (const).

**Returns:** Const reference to the component mask.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseNode::MaskVersion() const override`

Get the component mask version.

**Returns:** Version id.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::MaskVersion(uint32_t Version) override`

Set the component mask version.

**Parameters**

- `Version`: New version id.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph * SnAPI::GameFramework::BaseNode::OwnerGraph() const override`

Get the owning graph.

**Returns:** Pointer to owner graph or nullptr if unowned.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::OwnerGraph(NodeGraph *Graph) override`

Set the owning graph.

**Parameters**

- `Graph`: Owner graph pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld * SnAPI::GameFramework::BaseNode::World() const override`

Get the owning world for this node.

**Returns:** Pointer to the world interface or nullptr if unowned.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::World(IWorld *InWorld) override`

Set the owning world for this node.

**Parameters**

- `InWorld`: World interface pointer.
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
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::TickTree(float DeltaSeconds) override`

Tick this node and its subtree.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::FixedTickTree(float DeltaSeconds) override`

Fixed-step tick for this node and its subtree.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseNode::LateTickTree(float DeltaSeconds) override`

Late tick for this node and its subtree.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
