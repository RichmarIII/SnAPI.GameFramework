# SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage

Minimal cold-path interface for type-erased runtime storages.

This interface exists for reflection, serialization, and dynamic component APIs. Hot-path ticking continues to operate through typed storage pointers.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage::~IErasedStorage()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TypeId SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage::Type() const =0`

Get the reflected type stored by this erased storage.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::size_t SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage::Size() const =0`

Get the current live object count.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void * SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage::ResolveRaw(const Uuid &Id)=0`

Resolve an object by UUID to a borrowed mutable pointer.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const void * SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage::ResolveRaw(const Uuid &Id) const =0`

Resolve an object by UUID to a borrowed const pointer.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `virtual bool SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage::DestroyById(IWorld &WorldRef, const Uuid &Id)=0`

Destroy an object by UUID.

**Parameters**

- `WorldRef`: 
- `Id`:
</div>
