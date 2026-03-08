# SnAPI::GameFramework::TObjectPool

Generation-safe UUID object pool that keeps object addresses stable while entries are alive.

`TObjectPool` is the low-level backing store used for handle-addressable engine objects whose lifetime is frame-oriented rather than instant-destroy:
- each live object has a stable UUID
- handles also carry a runtime slot key for fast resolution
- slots are generation-checked so stale handles do not alias reused entries
- destruction is normally deferred to `EndFrame()`

Ownership and lifetime:
- `Create*()` stores pool-owned objects through `std::unique_ptr`.
- `CreateFromShared*()` stores an externally shared object through `std::shared_ptr`.
- Borrowed pointers remain valid until the object is scheduled for destroy and the pool reaches `EndFrame()`, or until `Clear()` destroys everything immediately.

Threading:
- Not generally thread-safe.
- Internal `GameMutex` use validates thread affinity in debug builds but does not provide cross-thread mutual exclusion.
- Mutate and query the pool from one owner thread or provide external synchronization.

Performance:
- Runtime-handle lookups are O(1) direct slot checks.
- UUID lookups are O(1) average hash-map probes.
- Slot reuse avoids vector growth where possible.

## Contents

- **Type:** SnAPI::GameFramework::TObjectPool::Entry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TObjectPool< T >::Handle = THandle<T>`

Handle type for objects in this pool.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::TObjectPool< T >::m_mutex`

Protects pool state.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<Entry> SnAPI::GameFramework::TObjectPool< T >::m_entries`

Dense storage for entries.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<Uuid, size_t, UuidHash> SnAPI::GameFramework::TObjectPool< T >::m_index`

UUID -> entry index.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<size_t> SnAPI::GameFramework::TObjectPool< T >::m_freeList`

Reusable entry indices.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<size_t> SnAPI::GameFramework::TObjectPool< T >::m_pendingDestroy`

Indices scheduled for deletion.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TObjectPool< T >::m_runtimePoolToken`

Runtime token used for direct handle resolution.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TObjectPool< T >::TObjectPool()`

Construct an empty pool and acquire a unique runtime-pool token for fast handle resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TObjectPool< T >::~TObjectPool()`

Destroy the pool and release its runtime-pool token.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TObjectPool< T >::TObjectPool(const TObjectPool &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `TObjectPool & SnAPI::GameFramework::TObjectPool< T >::operator=(const TObjectPool &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TObjectPool< T >::TObjectPool(TObjectPool &&)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `TObjectPool & SnAPI::GameFramework::TObjectPool< T >::operator=(TObjectPool &&)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::Create(Args &&... args)`

Construct and insert a new pool-owned object with a generated UUID.

The returned handle usually contains both UUID and runtime-slot identity. If the pool grows beyond the 32-bit runtime-index range, creation fails instead of silently producing an unusable fast-path handle.

**Parameters**

- `args`: Constructor arguments for U.

**Returns:** A generation-safe handle to the new object, or an error when the object cannot be created.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::CreateWithId(const Uuid &Id, Args &&... args)`

Construct and insert a new pool-owned object under an explicit UUID.

**Parameters**

- `Id`: UUID to assign to the object.
- `args`: Constructor arguments for U.

**Returns:** Handle to the created object, or an error when insertion fails.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::CreateFromShared(std::shared_ptr< T > Object)`

Insert an already-allocated shared object under a generated UUID.

Ownership:
- The pool shares ownership with the caller by storing the same `shared_ptr`.
- The object address stays stable for the lifetime of that shared object.

**Parameters**

- `Object`: Shared pointer to insert.

**Returns:** Handle to the inserted object or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::CreateFromSharedWithId(std::shared_ptr< T > Object, const Uuid &Id)`

Insert an already-allocated shared object under an explicit UUID.

**Parameters**

- `Object`: Shared pointer to insert.
- `Id`: UUID to assign to the object.

**Returns:** Handle to the inserted object or error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::IsValid(const Handle &HandleRef) const`

Check if a handle resolves to a live object.

**Parameters**

- `HandleRef`: Handle to validate.

**Returns:** True if object exists and is not pending destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::IsValid(const Uuid &Id) const`

Check if a UUID resolves to a live object.

**Parameters**

- `Id`: UUID to validate.

**Returns:** True if object exists and is not pending destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::HandleByIdSlow(const Uuid &Id) const`

Rebuild a fast runtime handle from a UUID lookup.

Use this when code persisted only the UUID identity and now needs a current, generation-checked runtime handle again. This is intentionally the slow path and performs a hash-map lookup.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Runtime-key handle or error if missing/pending destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::TObjectPool< T >::Borrowed(const Handle &HandleRef)`

Resolve a runtime handle to a borrowed mutable pointer.

Borrowing does not extend lifetime. The returned pointer becomes invalid when the entry is physically removed by `EndFrame()` or when `Clear()` is called.

**Parameters**

- `HandleRef`: Handle to resolve.

**Returns:** Pointer to object or nullptr if not found/pending destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::TObjectPool< T >::Borrowed(const Uuid &Id)`

Resolve a UUID to a borrowed mutable pointer.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Pointer to object or nullptr if not found/pending destroy.

**Notes**

- This is slower than handle-based lookup because it hashes the UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `const T * SnAPI::GameFramework::TObjectPool< T >::Borrowed(const Handle &HandleRef) const`

Resolve a handle to a borrowed pointer (const).

**Parameters**

- `HandleRef`: Handle to resolve.

**Returns:** Pointer to object or nullptr if not found/pending destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `const T * SnAPI::GameFramework::TObjectPool< T >::Borrowed(const Uuid &Id) const`

Resolve a UUID to a borrowed pointer (const).

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Pointer to object or nullptr if not found/pending destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::TObjectPool< T >::DestroyLater(const Handle &HandleRef)`

Mark an object for deferred destruction by runtime handle.

Semantics:
- The object remains borrowable until `EndFrame()`.
- Repeated calls are idempotent.
- `IsValid()` returns `false` immediately once an entry is pending destroy.

**Parameters**

- `HandleRef`: Handle to destroy.

**Returns:** Success or error if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::TObjectPool< T >::DestroyLater(const Uuid &Id)`

Mark an object for deferred destruction by UUID.

**Parameters**

- `Id`: UUID to destroy.

**Returns:** Success or error if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::EndFrame()`

Finalize all deferred destroys and recycle the freed slots.

`EndFrame()` is the point where pending entries actually disappear:
- UUID lookup entries are removed
- ownership pointers are released
- slot ids are cleared
- slot indices return to the free list

After this call, all borrowed pointers to destroyed objects are invalid.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::Clear()`

Destroy all entries immediately and reset the pool to empty.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::IsPendingDestroy(const Handle &HandleRef) const`

Check if a handle is pending destruction.

**Parameters**

- `HandleRef`: Handle to check.

**Returns:** True if the object is marked for deletion.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::IsPendingDestroy(const Uuid &Id) const`

Check if a UUID is pending destruction.

**Parameters**

- `Id`: UUID to check.

**Returns:** True if the object is marked for deletion.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEach(const Fn &Func) const`

Visit all currently live, non-pending objects in slot order.

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEachAll(const Fn &Func) const`

Visit all objects including entries already marked for destruction.

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEach(const Fn &Func)`

Mutable overload of `ForEach` for live, non-pending objects.

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEachAll(const Fn &Func)`

Mutable overload of `ForEachAll`.

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static T * SnAPI::GameFramework::TObjectPool< T >::ObjectPtr(Entry &EntryRef)`

**Parameters**

- `EntryRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `static T * SnAPI::GameFramework::TObjectPool< T >::ObjectPtr(const Entry &EntryRef)`

**Parameters**

- `EntryRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `static uint32_t SnAPI::GameFramework::TObjectPool< T >::NextGeneration(uint32_t Previous)`

Increment slot generation while reserving zero as invalid.

**Parameters**

- `Previous`: Previous generation value.

**Returns:** Next non-zero generation.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< uint32_t > SnAPI::GameFramework::TObjectPool< T >::RuntimeIndexFromSlot(size_t Index) const`

Return a runtime slot index usable by `THandle`.

**Parameters**

- `Index`: Slot index in `m_entries`.

**Returns:** Runtime index value or error when pool exceeds handle index range.
</div>
<div class="snapi-api-card" markdown="1">
### `Handle SnAPI::GameFramework::TObjectPool< T >::MakeHandle(size_t Index, const Entry &EntryRef) const`

Build a handle for a live entry.

**Parameters**

- `Index`: Slot index.
- `EntryRef`: Entry payload.

**Returns:** Handle containing UUID plus runtime key when index fits.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::ResolveIndexLocked(const Handle &HandleRef, size_t &OutIndex) const`

Resolve a handle to an entry index using only the runtime-key fast path.

This function deliberately does not fall back to UUID lookup. Callers that only have a UUID must first use `HandleByIdSlow()`.

**Parameters**

- `HandleRef`: Handle to resolve.
- `OutIndex`: Resolved index on success.

**Returns:** True when the handle matches the current slot generation and UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TObjectPool< T >::AllocateSlot()`

Allocate a storage slot, reusing free slots if possible.

**Returns:** Index into m_entries.
</div>
