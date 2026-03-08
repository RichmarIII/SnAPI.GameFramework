# SnAPI::GameFramework::Level

Level node facade that forwards graph operations into the owning world.

`Level` is a regular `BaseNode`-derived object used as a convenient grouping root for gameplay content. In the ECS-only architecture it does not own separate storage; instead, it forwards creation, destruction, attachment, and component operations into the `IWorld` it is bound to. This gives users a level-centric authoring API without splitting ownership away from the world.

Core semantics:
- a level is just another node as far as world ownership is concerned
- child nodes created through a bound level are attached under that level when possible
- all storage, identity, and destruction rules still come from the world

Ownership and lifetime:
- The world owns the level and every node/component reachable through it.
- References returned by level APIs are borrowed views into world-owned objects.

Threading model:
- Main-thread only for graph mutation.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::Level::kTypeName`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::Level()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::Level(std::string Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::Level(const Level &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `Level & SnAPI::GameFramework::Level::operator=(const Level &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::Level(Level &&) noexcept=default`
</div>
<div class="snapi-api-card" markdown="1">
### `Level & SnAPI::GameFramework::Level::operator=(Level &&) noexcept=default`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::~Level()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::World(IWorld *InWorld)`

Bind this level to an owning world.

**Parameters**

- `InWorld`: Non-owning world pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::Level::CreateNode(std::string Name, Args &&... args)`

Create a child node of reflected type `T`.

**Parameters**

- `Name`: 
- `args`: Additional constructor arguments. Must be omitted; passing any value causes the call to fail with `EErrorCode::InvalidArgument`.

**Returns:** Handle to the created node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::Level::CreateNodeWithId(const Uuid &Id, std::string Name, Args &&... args)`

Create a child node of reflected type `T` with an explicit UUID.

**Parameters**

- `Id`: 
- `Name`: 
- `args`: Additional constructor arguments. Must be omitted; passing any value causes the call to fail with `EErrorCode::InvalidArgument`.

**Returns:** Handle to the created node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::Level::CreateNode(const TypeId &Type, std::string Name)`

Create a child node by reflected type.

**Parameters**

- `Type`: Reflected node type id.
- `Name`: 

**Returns:** Handle to the created node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::Level::CreateNode(const TypeId &Type, std::string Name, const Uuid &Id)`

Create a child node by reflected type with an explicit UUID.

**Parameters**

- `Type`: Reflected node type id.
- `Name`: 
- `Id`: 

**Returns:** Handle to the created node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::Level::DestroyNode(const NodeHandle &Handle)`

Destroy a node through the owning world.

**Parameters**

- `Handle`: 

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::Level::AttachChild(const NodeHandle &Parent, const NodeHandle &Child)`

Attach a child node under a parent node.

**Parameters**

- `Parent`: 
- `Child`: Child node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::Level::DetachChild(const NodeHandle &Child)`

Detach a child node from its current parent.

**Parameters**

- `Child`: Child node handle.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::Tick(float DeltaSeconds)`

Variable-step level hook.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::FixedTick(float DeltaSeconds)`

Fixed-step level hook.

**Parameters**

- `DeltaSeconds`: Fixed delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::LateTick(float DeltaSeconds)`

Late-step level hook.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::EndFrame()`

End-of-frame level hook.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::Clear()`

Clear level-owned state.
</div>
<div class="snapi-api-card" markdown="1">
### `TObjectPool< BaseNode > & SnAPI::GameFramework::Level::NodePool()`

Access the owning world's node pool.

**Returns:** Mutable node pool reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const TObjectPool< BaseNode > & SnAPI::GameFramework::Level::NodePool() const`

Access the owning world's node pool.

**Returns:** Const node pool reference.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::Level::NodeHandleByIdSlow(const Uuid &Id) const`

Resolve a node by UUID through the owning world.

**Parameters**

- `Id`: 

**Returns:** Handle to the resolved node or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Level::RemoveComponentByType(const NodeHandle &Owner, const TypeId &Type)`

Remove a component by reflected type from a node.

**Parameters**

- `Owner`: Owning node handle.
- `Type`: Reflected component type id.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::Level::AddComponent(const NodeHandle &Owner, Args &&... args)`

Attach a runtime-compatible component to a node.

**Parameters**

- `Owner`: Owning node handle.
- `args`: Constructor arguments forwarded to `BaseNode::Add<T>()`.

**Returns:** Borrowed reference wrapper to the attached component or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::Level::AddComponentWithId(const NodeHandle &Owner, const Uuid &Id, Args &&... args)`

Attach a runtime-compatible component with an explicit UUID.

**Parameters**

- `Owner`: Owning node handle.
- `Id`: 
- `args`: Constructor arguments forwarded to the runtime storage path.

**Returns:** Borrowed reference wrapper to the attached component or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::Level::Component(const NodeHandle &Owner)`

Resolve a typed component attached to a node.

**Parameters**

- `Owner`: Owning node handle.

**Returns:** Borrowed reference wrapper to the component or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Level::HasComponent(const NodeHandle &Owner) const`

Check whether a node currently has a component of type `T`.

**Parameters**

- `Owner`: Owning node handle.

**Returns:** `true` when the component is attached.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::RemoveComponent(const NodeHandle &Owner)`

Remove a typed component from a node if it exists.

**Parameters**

- `Owner`: Owning node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::Level::BorrowedComponent(const NodeHandle &Owner, const TypeId &Type)`

Borrow a raw component pointer by reflected type.

**Parameters**

- `Owner`: Owning node handle.
- `Type`: Reflected component type id.

**Returns:** Non-owning raw component pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::Level::BorrowedComponent(const NodeHandle &Owner, const TypeId &Type) const`

Borrow a raw component pointer by reflected type.

**Parameters**

- `Owner`: Owning node handle.
- `Type`: Reflected component type id.

**Returns:** Non-owning raw component pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld * SnAPI::GameFramework::BaseNode::World() const`

Get the owning world for this node.

**Returns:** Pointer to the world interface or nullptr if unowned.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static TObjectPool< BaseNode > & SnAPI::GameFramework::Level::NullNodePool()`
</div>
