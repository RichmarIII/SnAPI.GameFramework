# SnAPI::GameFramework::TDenseRuntimeStorage

Dense, generation-safe storage for one runtime object type.

`TDenseRuntimeStorage` is the hot-path container behind the ECS refactor. Objects are stored contiguously in `m_denseObjects`, while stable identity is tracked through a slot table plus generation-safe handles.

Core semantics:
- Dense order is unstable and may change on destroy via swap-pop compaction.
- UUID identity is unique within the storage.
- `OnCreate` may run immediately or be deferred via `PendingOnCreate`.
- `OnDestroy` runs synchronously during destroy/clear, not at a later frame boundary.

Ownership and lifetime:
- The storage owns all contained `TObject` instances by value.
- Resolved pointers are borrowed and invalidated by any destroy or clear that moves or erases the underlying dense array.

Threading:
- Main-thread only.

Performance:
- Handle resolution is O(1).
- UUID fallback resolution is O(1) average through `m_idToSlot`.
- Tick phases iterate linearly over contiguous storage.

## Contents

- **Type:** SnAPI::GameFramework::TDenseRuntimeStorage::SlotMeta

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Handle = TDenseRuntimeHandle<TObject>`
</div>

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasOnCreatePhase`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasOnDestroyPhase`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasPreTickPhase`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasTickPhase`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasFixedTickPhase`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasLateTickPhase`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::kHasPostTickPhase`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::m_storageToken`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TObject> SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::m_denseObjects`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint32_t> SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::m_denseSlotIndices`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<SlotMeta> SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::m_slots`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint32_t> SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::m_freeSlotIndices`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<Uuid, uint32_t, UuidHash> SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::m_idToSlot`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::TDenseRuntimeStorage(const uint32_t StorageToken=1)`

Construct a storage bound to a specific storage token.

**Parameters**

- `StorageToken`:
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::StorageToken() const noexcept`

Get the stable token that identifies this storage instance in handles.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Size() const noexcept`

Get the current number of live runtime objects in dense storage.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Empty() const noexcept`

Return `true` when the storage contains no live objects.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Create(IWorld &WorldRef, TArgs &&... Args)`

Create a new runtime object with a generated UUID.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `Args`: Constructor arguments for `TObject`.

**Returns:** Handle to the created object, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::CreateWithId(IWorld &WorldRef, const Uuid &Id, TArgs &&... Args)`

Create a new runtime object under an explicit UUID.

Semantics:
- UUID collisions fail and do not overwrite an existing object.
- If `OnCreate` is suppressed on the current thread, the object is marked `PendingOnCreate` and must be flushed later.
- On construction failure, slot allocation is rolled back.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `Id`: Stable identity for the new object.
- `Args`: Constructor arguments for `TObject`.

**Returns:** Handle to the created object, or an error when the UUID is invalid, duplicated, or construction fails.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Destroy(IWorld &WorldRef, const Handle &InHandle)`

Destroy a runtime object by handle.

Destruction is immediate. Dense order may change because the last dense object is swapped into the removed slot.

**Parameters**

- `WorldRef`: Owning world passed through to `OnDestroy` when present.
- `InHandle`: Handle to destroy.

**Returns:** `true` when the handle resolved and the object was destroyed.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::DestroySlow(IWorld &WorldRef, const Uuid &Id)`

Destroy a runtime object by UUID fallback lookup.

**Parameters**

- `WorldRef`: Owning world passed through to `OnDestroy`.
- `Id`: UUID to destroy.

**Returns:** `true` when the UUID resolved to a live object.
</div>
<div class="snapi-api-card" markdown="1">
### `TObject * SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Resolve(const Handle &InHandle)`

Resolve a handle to a borrowed mutable object pointer.

**Parameters**

- `InHandle`: Handle to resolve.

**Returns:** Borrowed pointer to the live object, or `nullptr` if the handle is stale.
</div>
<div class="snapi-api-card" markdown="1">
### `const TObject * SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Resolve(const Handle &InHandle) const`

Const overload of `Resolve(const Handle&)`.

**Parameters**

- `InHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `TObject * SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::ResolveSlowById(const Uuid &Id)`

Resolve a UUID to a borrowed mutable object pointer.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Borrowed pointer to the live object, or `nullptr` when missing.

**Notes**

- This is the slow path compared with handle resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `const TObject * SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::ResolveSlowById(const Uuid &Id) const`

Const overload of `ResolveSlowById(const Uuid&)`.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::HandleById(const Uuid &Id) const`

Rebuild a current handle from a UUID.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Fresh handle for the live object, or an error when not found.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::PreTick(IWorld &WorldRef, const float DeltaSeconds)`

Execute the storage's `PreTick` phase across all live objects.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `DeltaSeconds`: Variable-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Tick(IWorld &WorldRef, const float DeltaSeconds)`

Execute the storage's `Tick` phase across all live objects.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `DeltaSeconds`: Variable-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::FixedTick(IWorld &WorldRef, const float DeltaSeconds)`

Execute the storage's `FixedTick` phase across all live objects.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `DeltaSeconds`: Fixed-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::LateTick(IWorld &WorldRef, const float DeltaSeconds)`

Execute the storage's `LateTick` phase across all live objects.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `DeltaSeconds`: Variable-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::PostTick(IWorld &WorldRef, const float DeltaSeconds)`

Execute the storage's `PostTick` phase across all live objects.

**Parameters**

- `WorldRef`: Owning world passed through to lifecycle hooks.
- `DeltaSeconds`: Variable-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::FlushPendingOnCreate(IWorld &WorldRef)`

Invoke any deferred `OnCreate` hooks that were suppressed during creation.

Ordering is slot-table order, which usually matches creation order but is not documented as a stable cross-version contract.

**Parameters**

- `WorldRef`: Owning world passed through to `OnCreate`.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::Clear(IWorld &WorldRef)`

Destroy all live objects immediately and reset the storage to empty.

This invalidates every outstanding handle and borrowed pointer.

**Parameters**

- `WorldRef`: Owning world passed through to `OnDestroy`.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokeOnCreate(TObject &Object, IWorld &WorldRef)`

**Parameters**

- `Object`: 
- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokeOnDestroy(TObject &Object, IWorld &WorldRef)`

**Parameters**

- `Object`: 
- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokePreTick(TObject &Object, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `Object`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokeTick(TObject &Object, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `Object`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokeFixedTick(TObject &Object, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `Object`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokeLateTick(TObject &Object, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `Object`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::InvokePostTick(TObject &Object, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `Object`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `Handle SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::MakeHandle(const uint32_t SlotIndex) const`

**Parameters**

- `SlotIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::ResolveSlot(const Handle &InHandle, uint32_t &OutSlotIndex) const`

**Parameters**

- `InHandle`: 
- `OutSlotIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::AcquireSlot(const Uuid &Id)`

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::RollbackCreate(const uint32_t SlotIndex)`

**Parameters**

- `SlotIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TDenseRuntimeStorage< TObject >::DestroyBySlot(IWorld &WorldRef, const uint32_t SlotIndex)`

**Parameters**

- `WorldRef`: 
- `SlotIndex`:
</div>
