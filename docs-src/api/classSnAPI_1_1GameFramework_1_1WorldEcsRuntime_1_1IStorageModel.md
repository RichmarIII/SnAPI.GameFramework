# SnAPI::GameFramework::WorldEcsRuntime::IStorageModel

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual uint32_t SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::StorageToken() const =0`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< RuntimeComponentHandle > SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::CreateDefault(IWorld &WorldRef, const Uuid *ExplicitId)=0`

**Parameters**

- `WorldRef`: 
- `ExplicitId`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::FlushPendingOnCreate(IWorld &WorldRef)=0`

**Parameters**

- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::DestroyByRuntimeHandle(IWorld &WorldRef, RuntimeComponentHandle Handle)=0`

**Parameters**

- `WorldRef`: 
- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void * SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::ResolveRawByRuntimeHandle(RuntimeComponentHandle Handle)=0`

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const void * SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::ResolveRawByRuntimeHandle(RuntimeComponentHandle Handle) const =0`

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::WorldEcsRuntime::IStorageModel::Clear(IWorld &WorldRef)=0`

**Parameters**

- `WorldRef`:
</div>
