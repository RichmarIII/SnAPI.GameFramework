# SnAPI::GameFramework::IComponentStorage

Type-erased interface for component storage.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IComponentStorage::~IComponentStorage()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TypeId SnAPI::GameFramework::IComponentStorage::TypeKey() const =0`

Get the component type id stored by this storage.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::IComponentStorage::Has(NodeHandle Owner) const =0`

Check if a node has this component.

**Parameters**

- `Owner`: Node handle.

**Returns:** True if the component exists.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponentStorage::Remove(NodeHandle Owner)=0`

Remove a component from a node.

**Parameters**

- `Owner`: Node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponentStorage::TickComponent(NodeHandle Owner, float DeltaSeconds)=0`

Tick a component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponentStorage::FixedTickComponent(NodeHandle Owner, float DeltaSeconds)=0`

Fixed-step tick a component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponentStorage::LateTickComponent(NodeHandle Owner, float DeltaSeconds)=0`

Late tick a component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void * SnAPI::GameFramework::IComponentStorage::Borrowed(NodeHandle Owner)=0`

Borrow a component instance (mutable).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.

**Notes**

- Borrowed pointers must not be cached.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const void * SnAPI::GameFramework::IComponentStorage::Borrowed(NodeHandle Owner) const =0`

Borrow a component instance (const).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponentStorage::EndFrame()=0`

Process pending destruction at end-of-frame.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponentStorage::Clear()=0`

Clear all components immediately.
</div>
