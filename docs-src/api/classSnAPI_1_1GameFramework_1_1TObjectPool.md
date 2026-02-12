# SnAPI::GameFramework::TObjectPool

Thread-safe object pool keyed by UUID handles.

## Contents

- **Type:** SnAPI::GameFramework::TObjectPool::Entry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TObjectPool< T >::Handle = THandle<T>`

Handle type for objects in this pool.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::TObjectPool< T >::m_mutex`

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

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TObjectPool< T >::TObjectPool()=default`

Construct an empty pool.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::Create(Args &&... args)`

Create a new object with a generated UUID.

**Parameters**

- `args`: Constructor arguments for U.

**Returns:** Handle to the created object or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::CreateWithId(const Uuid &Id, Args &&... args)`

Create a new object with an explicit UUID.

**Parameters**

- `Id`: UUID to assign to the object.
- `args`: Constructor arguments for U.

**Returns:** Handle to the created object or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::CreateFromShared(std::shared_ptr< T > Object)`

Insert an existing shared object with a generated UUID.

**Parameters**

- `Object`: Shared pointer to insert.

**Returns:** Handle to the inserted object or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::TObjectPool< T >::CreateFromSharedWithId(std::shared_ptr< T > Object, const Uuid &Id)`

Insert an existing shared object with an explicit UUID.

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
### `T * SnAPI::GameFramework::TObjectPool< T >::Borrowed(const Handle &HandleRef)`

Resolve a handle to a borrowed pointer.

**Parameters**

- `HandleRef`: Handle to resolve.

**Returns:** Pointer to object or nullptr if not found/pending destroy.

**Notes**

- Borrowed pointers must not be cached.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::TObjectPool< T >::Borrowed(const Uuid &Id)`

Resolve a UUID to a borrowed pointer.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Pointer to object or nullptr if not found/pending destroy.

**Notes**

- Borrowed pointers must not be cached.
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

Mark an object for end-of-frame destruction by handle.

**Parameters**

- `HandleRef`: Handle to destroy.

**Returns:** Success or error if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::TObjectPool< T >::DestroyLater(const Uuid &Id)`

Mark an object for end-of-frame destruction by UUID.

**Parameters**

- `Id`: UUID to destroy.

**Returns:** Success or error if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::EndFrame()`

Destroy all objects that were marked for deletion.

**Notes**

- Should be called at end of frame to keep handles stable.
- Destroyed UUID keys are removed from index and may be reused on future creates.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::Clear()`

Remove all objects immediately.

**Notes**

- Use cautiously; invalidates all handles immediately.
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

Iterate over all live (non-pending) objects (const).

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEachAll(const Fn &Func) const`

Iterate over all objects including pending destroy (const).

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEach(const Fn &Func)`

Iterate over all live (non-pending) objects (mutable).

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TObjectPool< T >::ForEachAll(const Fn &Func)`

Iterate over all objects including pending destroy (mutable).

**Parameters**

- `Func`: Callback invoked with (Handle, Object).
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TObjectPool< T >::AllocateSlot()`

Allocate a storage slot, reusing free slots if possible.

**Returns:** Index into m_entries.
</div>
