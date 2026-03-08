# SnAPI::GameFramework::ComponentStorageView

Type-erased storage interface for one component type.

`ComponentStorageView` is the cold-path abstraction used when world/level code needs to work with "a component storage" without statically knowing `T`. Concrete hot-path iteration still happens in `TComponentStorage<T>`.

Ownership and lifetime:
- The storage owns component instances.
- Borrowed pointers returned from `Borrowed()` remain valid only until that component is removed, the storage reaches `EndFrame()`, or `Clear()` is called.

Threading:
- Main-thread only unless an outer system guarantees exclusive access.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentStorageView::NodeActivePredicate = bool(*)(void* UserData, const BaseNode& Node)`

Node-activity callback signature used by storage-driven ticking.

**Returns:** True when the node should execute component tick hooks.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::ComponentStorageView::~ComponentStorageView()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TypeId SnAPI::GameFramework::ComponentStorageView::TypeKey() const =0`

Get the component type id stored by this storage.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::ComponentStorageView::Has(const NodeHandle &Owner) const =0`

Check if a node has this component.

**Parameters**

- `Owner`: Node handle.

**Returns:** True if the component exists.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::Remove(const NodeHandle &Owner)=0`

Remove a component from a node.

**Parameters**

- `Owner`: Node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::TickComponent(const NodeHandle &Owner, float DeltaSeconds)=0`

Tick a component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::FixedTickComponent(const NodeHandle &Owner, float DeltaSeconds)=0`

Fixed-step tick a component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::LateTickComponent(const NodeHandle &Owner, float DeltaSeconds)=0`

Late tick a component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::TickAll(NodeActivePredicate NodeIsActive, void *UserData, float DeltaSeconds)=0`

Tick all stored components in dense storage order.

**Parameters**

- `NodeIsActive`: Callback used to gate owner-node activity/relevance.
- `UserData`: Opaque callback context.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::FixedTickAll(NodeActivePredicate NodeIsActive, void *UserData, float DeltaSeconds)=0`

Fixed-step tick all stored components in dense storage order.

**Parameters**

- `NodeIsActive`: Callback used to gate owner-node activity/relevance.
- `UserData`: Opaque callback context.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::LateTickAll(NodeActivePredicate NodeIsActive, void *UserData, float DeltaSeconds)=0`

Late tick all stored components in dense storage order.

**Parameters**

- `NodeIsActive`: Callback used to gate owner-node activity/relevance.
- `UserData`: Opaque callback context.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void * SnAPI::GameFramework::ComponentStorageView::Borrowed(const NodeHandle &Owner)=0`

Borrow a component instance (mutable).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.

**Notes**

- Borrowed pointers must not be cached.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const void * SnAPI::GameFramework::ComponentStorageView::Borrowed(const NodeHandle &Owner) const =0`

Borrow a component instance (const).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::EndFrame()=0`

Process pending destruction at end-of-frame.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ComponentStorageView::Clear()=0`

Clear all components immediately.
</div>
