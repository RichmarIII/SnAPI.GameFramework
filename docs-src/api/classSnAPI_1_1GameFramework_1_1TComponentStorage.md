# SnAPI::GameFramework::TComponentStorage

Typed component storage for a specific component type.

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TComponentStorage< T >::m_typeId`

Reflected type id for this storage specialization.
</div>
<div class="snapi-api-card" markdown="1">
### `TObjectPool<T> SnAPI::GameFramework::TComponentStorage< T >::m_pool`

Underlying component object pool with deferred destroy support.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<NodeHandle, Uuid, HandleHash> SnAPI::GameFramework::TComponentStorage< T >::m_index`

Owner-node handle -> component UUID map.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<Uuid> SnAPI::GameFramework::TComponentStorage< T >::m_pendingDestroy`

Component ids scheduled for end-of-frame destroy flush.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TComponentStorage< T >::TypeKey() const override`

Get the component type id.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::Add(NodeHandle Owner)`

Add a component with a generated UUID.

**Parameters**

- `Owner`: Owner node handle.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::Add(NodeHandle Owner, Args &&... args)`

Add a component with constructor arguments.

**Parameters**

- `Owner`: Owner node handle.
- `args`: Constructor arguments.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::AddWithId(NodeHandle Owner, const Uuid &Id, Args &&... args)`

Add a component with an explicit UUID.

**Parameters**

- `Owner`: Owner node handle.
- `Id`: Component UUID.
- `args`: Constructor arguments.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::Component(NodeHandle Owner)`

Get a component by owner.

**Parameters**

- `Owner`: Owner node handle.

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TComponentStorage< T >::Has(NodeHandle Owner) const override`

Check if a node has this component.

**Parameters**

- `Owner`: Node handle.

**Returns:** True if present.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::Remove(NodeHandle Owner) override`

Remove a component from a node.

**Parameters**

- `Owner`: Node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::TickComponent(NodeHandle Owner, float DeltaSeconds) override`

Tick the component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::FixedTickComponent(NodeHandle Owner, float DeltaSeconds) override`

Fixed-step tick the component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::LateTickComponent(NodeHandle Owner, float DeltaSeconds) override`

Late tick the component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::TComponentStorage< T >::Borrowed(NodeHandle Owner) override`

Borrow the component instance (mutable).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::TComponentStorage< T >::Borrowed(NodeHandle Owner) const override`

Borrow the component instance (const).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::EndFrame() override`

Process pending destruction at end-of-frame.

**Notes**

- Ordering is deterministic by pending queue insertion order.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::Clear() override`

Clear all components immediately.

**Notes**

- Immediate path bypasses deferred destroy semantics.
</div>
