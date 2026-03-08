# SnAPI::GameFramework::TComponentStorage

Dense one-component-per-node storage for a specific component type.

This storage is the bridge between object-like component lifetime and data-oriented ticking. It maintains:
- a deferred-destroy object pool for the component instances
- an owner UUID map for slow-path lookup
- a sparse runtime-owner map for fast-path lookup
- a dense linear array for cache-friendly iteration

Core semantics:
- A node may own at most one `T`.
- `Add*()` immediately inserts into the dense set and object registry.
- `Remove()` detaches the component from the owner immediately, but physical destruction and `OnDestroy()` are deferred until `EndFrame()`.
- Dense order is unstable; removals use swap-pop compaction.

Threading:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::TComponentStorage::ComponentEntry
- **Type:** SnAPI::GameFramework::TComponentStorage::PendingDestroyEntry

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::TComponentStorage< T >::kInvalidDenseIndex`
</div>

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
### `std::unordered_map<Uuid, std::size_t, UuidHash> SnAPI::GameFramework::TComponentStorage< T >::m_ownerToDense`

Owner-node UUID -> dense-entry index.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::size_t> SnAPI::GameFramework::TComponentStorage< T >::m_sparseOwnerToDense`

Runtime owner slot index -> dense-entry index (sparse-set style fast path).
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint32_t> SnAPI::GameFramework::TComponentStorage< T >::m_sparseOwnerGeneration`

Generation mirror for sparse owner slots to reject stale handles.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ComponentEntry> SnAPI::GameFramework::TComponentStorage< T >::m_dense`

Dense component entries for cache-friendly linear traversal.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<PendingDestroyEntry> SnAPI::GameFramework::TComponentStorage< T >::m_pendingDestroy`

Components scheduled for end-of-frame destroy flush.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TComponentStorage< T >::TypeKey() const override`

Get the component type id.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::Add(const NodeHandle &Owner)`

Add a default-constructed component with a generated UUID.

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::Add(const NodeHandle &Owner, Args &&... args)`

Add a component constructed from caller-provided arguments.

**Parameters**

- `Owner`: Owner node handle.
- `args`: Constructor arguments.

**Returns:** Borrowed reference to the attached component, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::AddWithId(const NodeHandle &Owner, const Uuid &Id, Args &&... args)`

Add a component under an explicit UUID.

Semantics:
- Fails when the owner already has a `T`.
- Sets owner/id/runtime identity/type key fields on the component.
- Registers the component in `ObjectRegistry`.
- Invokes `OnCreate()` immediately unless component `OnCreate` is currently suppressed by `ScopedComponentOnCreateSuppression`.

This is the entry point used by deserialization and replication paths that need identity continuity rather than a fresh UUID.

**Parameters**

- `Owner`: Owner node handle.
- `Id`: Component UUID.
- `args`: Constructor arguments.

**Returns:** Borrowed reference to the attached component, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< T > SnAPI::GameFramework::TComponentStorage< T >::Component(const NodeHandle &Owner)`

Resolve the component currently attached to an owner node.

**Parameters**

- `Owner`: Owner node handle.

**Returns:** Borrowed reference to the component, or `NotFound` when the owner does not currently have this component.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TComponentStorage< T >::Has(const NodeHandle &Owner) const override`

Check if a node has this component.

**Parameters**

- `Owner`: Node handle.

**Returns:** True if present.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::Remove(const NodeHandle &Owner) override`

Detach a component from its owner and schedule it for deferred destruction.

Semantics:
- Owner lookup tables are updated immediately.
- Dense storage is compacted immediately with swap-pop.
- The component instance remains alive until `EndFrame()`.
- `OnDestroy()` and `ObjectRegistry` unregistration happen during `EndFrame()`.

**Parameters**

- `Owner`: Node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::TickComponent(const NodeHandle &Owner, float DeltaSeconds) override`

Tick the component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::FixedTickComponent(const NodeHandle &Owner, float DeltaSeconds) override`

Fixed-step tick the component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::LateTickComponent(const NodeHandle &Owner, float DeltaSeconds) override`

Late tick the component for a node.

**Parameters**

- `Owner`: Node handle.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::TickAll(NodeActivePredicate NodeIsActive, void *UserData, float DeltaSeconds) override`

Tick all active components in dense storage order.

The owner node is also gated through `NodeIsActive` when that callback is provided. Owner-node pointers are lazily cached per dense entry.

**Parameters**

- `NodeIsActive`: Owner-node activity predicate.
- `UserData`: Opaque predicate context.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::FixedTickAll(NodeActivePredicate NodeIsActive, void *UserData, float DeltaSeconds) override`

Fixed-step tick all components in dense storage order.

**Parameters**

- `NodeIsActive`: Owner-node activity predicate.
- `UserData`: Opaque predicate context.
- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::LateTickAll(NodeActivePredicate NodeIsActive, void *UserData, float DeltaSeconds) override`

Late tick all components in dense storage order.

**Parameters**

- `NodeIsActive`: Owner-node activity predicate.
- `UserData`: Opaque predicate context.
- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::TComponentStorage< T >::Borrowed(const NodeHandle &Owner) override`

Borrow the attached component instance.

**Parameters**

- `Owner`: Node handle.

**Returns:** Non-owning component pointer, or `nullptr` when the owner has no `T`.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::TComponentStorage< T >::Borrowed(const NodeHandle &Owner) const override`

Borrow the component instance (const).

**Parameters**

- `Owner`: Node handle.

**Returns:** Pointer to component or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::EndFrame() override`

Finalize all removals that were deferred earlier in the frame.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::Clear() override`

Destroy every stored component immediately and reset the storage to empty.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::TComponentStorage< T >::DenseSize() const`

Get the current dense entry count.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::TComponentStorage< T >::DenseOwner(std::size_t Index) const`

Read the owner handle stored at a dense index.

**Parameters**

- `Index`: Dense index.

**Returns:** Copy of the stored owner handle, or a null handle when out of range.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::TComponentStorage< T >::DenseComponent(std::size_t Index)`

Borrow the component pointer stored at a dense index.

**Parameters**

- `Index`: Dense index.

**Returns:** Non-owning component pointer or `nullptr` when out of range.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TComponentStorage< T >::ResolveDenseIndex(const NodeHandle &Owner, std::size_t &OutIndex) const`

**Parameters**

- `Owner`: 
- `OutIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TComponentStorage< T >::ResolveDenseIndex(const NodeHandle &Owner, std::size_t &OutIndex)`

**Parameters**

- `Owner`: 
- `OutIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TComponentStorage< T >::TryResolveDenseIndexFromSparse(const NodeHandle &Owner, std::size_t &OutIndex) const`

**Parameters**

- `Owner`: 
- `OutIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TComponentStorage< T >::TryResolveDenseIndexFromOwnerId(const NodeHandle &Owner, std::size_t &OutIndex) const`

**Parameters**

- `Owner`: 
- `OutIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::RehydrateOwnerRuntimeIdentity(const NodeHandle &LookupOwner, std::size_t DenseIndex)`

**Parameters**

- `LookupOwner`: 
- `DenseIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::SetSparseOwnerIndex(const NodeHandle &Owner, std::size_t DenseIndex)`

**Parameters**

- `Owner`: 
- `DenseIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TComponentStorage< T >::ClearSparseOwnerIndex(const NodeHandle &Owner)`

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::TComponentStorage< T >::ResolveComponent(ComponentEntry &Entry)`

**Parameters**

- `Entry`:
</div>
<div class="snapi-api-card" markdown="1">
### `const T * SnAPI::GameFramework::TComponentStorage< T >::ResolveComponent(const ComponentEntry &Entry) const`

**Parameters**

- `Entry`:
</div>
