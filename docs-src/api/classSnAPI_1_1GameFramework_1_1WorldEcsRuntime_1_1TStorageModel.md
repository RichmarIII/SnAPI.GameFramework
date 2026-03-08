# SnAPI::GameFramework::WorldEcsRuntime::TStorageModel

## Public Members

<div class="snapi-api-card" markdown="1">
### `TDenseRuntimeStorage<TObject> SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::TypedStorage`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::TStorageModel(const uint32_t StorageToken)`

**Parameters**

- `StorageToken`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::~TStorageModel() override`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::Type() const override`

Get the reflected type stored by this erased storage.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::Size() const override`

Get the current live object count.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::StorageToken() const override`
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::ResolveRaw(const Uuid &Id) override`

Resolve an object by UUID to a borrowed mutable pointer.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::ResolveRaw(const Uuid &Id) const override`

Resolve an object by UUID to a borrowed const pointer.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::DestroyById(IWorld &WorldRef, const Uuid &Id) override`

Destroy an object by UUID.

**Parameters**

- `WorldRef`: 
- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::CreateDefault(IWorld &WorldRef, const Uuid *ExplicitId) override`

**Parameters**

- `WorldRef`: 
- `ExplicitId`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::FlushPendingOnCreate(IWorld &WorldRef) override`

**Parameters**

- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::DestroyByRuntimeHandle(IWorld &WorldRef, const RuntimeComponentHandle Handle) override`

**Parameters**

- `WorldRef`: 
- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::ResolveRawByRuntimeHandle(const RuntimeComponentHandle Handle) override`

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::ResolveRawByRuntimeHandle(const RuntimeComponentHandle Handle) const override`

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::TStorageModel< TObject >::Clear(IWorld &WorldRef) override`

**Parameters**

- `WorldRef`:
</div>
