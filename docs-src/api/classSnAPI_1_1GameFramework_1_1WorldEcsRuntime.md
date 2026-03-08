# SnAPI::GameFramework::WorldEcsRuntime

World-owned orchestration layer for dense runtime nodes and typed runtime component storages.

`WorldEcsRuntime` is the top-level container that ties together:
- `WorldNodeRuntime` for runtime node identity and hierarchy
- lazily created `TDenseRuntimeStorage<T>` instances for concrete runtime types
- one-component-per-type attachments from runtime nodes to runtime components
- globally ordered tick dispatch by compile-time priority

Core semantics:
- Typed `Storage<T>()` creation is lazy and also registers tick dispatch for `T` when it exposes any runtime tick phase.
- Tick order is ascending `RuntimeTickPriority<T>()`; ties keep storage creation order.
- The typed `AddComponent<T>()` path can create storage on demand.
- The dynamic `AddComponent(TypeId)` path only works for types whose storage model has already been created.
- `FlushPendingOnCreate()` iterates an `unordered_map`, so inter-type flush order is intentionally unspecified.

Threading:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::WorldEcsRuntime::IErasedStorage
- **Type:** SnAPI::GameFramework::WorldEcsRuntime::NodeComponentLink
- **Type:** SnAPI::GameFramework::WorldEcsRuntime::NodeComponentAttachment
- **Type:** SnAPI::GameFramework::WorldEcsRuntime::TickEntry
- **Type:** SnAPI::GameFramework::WorldEcsRuntime::IStorageModel
- **Type:** SnAPI::GameFramework::WorldEcsRuntime::TStorageModel

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, std::unique_ptr<IStorageModel>, UuidHash> SnAPI::GameFramework::WorldEcsRuntime::m_storages`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<uint32_t, IStorageModel*> SnAPI::GameFramework::WorldEcsRuntime::m_storageByToken`
</div>
<div class="snapi-api-card" markdown="1">
### `WorldNodeRuntime SnAPI::GameFramework::WorldEcsRuntime::m_nodeRuntime`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeComponentAttachment> SnAPI::GameFramework::WorldEcsRuntime::m_nodeComponentsBySlot`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<RuntimeNodeHandle> SnAPI::GameFramework::WorldEcsRuntime::m_componentDestroyScratch`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TickEntry> SnAPI::GameFramework::WorldEcsRuntime::m_tickEntries`
</div>
<div class="snapi-api-card" markdown="1">
### `uint64_t SnAPI::GameFramework::WorldEcsRuntime::m_nextTickSequence`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `TDenseRuntimeStorage< TObject > & SnAPI::GameFramework::WorldEcsRuntime::Storage()`

Get or lazily create the typed storage for `TObject`.

The first call also:
- acquires a unique storage token
- creates the erased storage model
- registers tick dispatch when the type exposes any runtime tick phase

**Returns:** Reference to the owned typed storage.
</div>
<div class="snapi-api-card" markdown="1">
### `TDenseRuntimeStorage< TObject > * SnAPI::GameFramework::WorldEcsRuntime::FindStorage()`

Find an existing typed storage without creating one.

**Returns:** Pointer to the storage, or `nullptr` when no storage has been created yet.
</div>
<div class="snapi-api-card" markdown="1">
### `const TDenseRuntimeStorage< TObject > * SnAPI::GameFramework::WorldEcsRuntime::FindStorage() const`

Const overload of `FindStorage<TObject>()`.
</div>
<div class="snapi-api-card" markdown="1">
### `IErasedStorage * SnAPI::GameFramework::WorldEcsRuntime::FindErased(const TypeId &Type)`

Find an existing erased storage by reflected type id.

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `const IErasedStorage * SnAPI::GameFramework::WorldEcsRuntime::FindErased(const TypeId &Type) const`

Const overload of `FindErased(const TypeId&)`.

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `WorldNodeRuntime & SnAPI::GameFramework::WorldEcsRuntime::Nodes()`

Access the world-owned runtime node hierarchy.
</div>
<div class="snapi-api-card" markdown="1">
### `const WorldNodeRuntime & SnAPI::GameFramework::WorldEcsRuntime::Nodes() const`

Const access to the world-owned runtime node hierarchy.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TDenseRuntimeHandle< TObject > > SnAPI::GameFramework::WorldEcsRuntime::AddComponent(IWorld &WorldRef, const RuntimeNodeHandle Owner, TArgs &&... Args)`

Create and attach a typed runtime component to a runtime node.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `Owner`: Runtime node that will own the component.
- `Args`: Constructor arguments for `TObject`.

**Returns:** Typed runtime-component handle, or an error when the node is invalid or already owns that component type.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TDenseRuntimeHandle< TObject > > SnAPI::GameFramework::WorldEcsRuntime::AddComponentWithId(IWorld &WorldRef, const RuntimeNodeHandle Owner, const Uuid &Id, TArgs &&... Args)`

Create and attach a typed runtime component under an explicit UUID.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `Owner`: Runtime node that will own the component.
- `Id`: Stable component identity.
- `Args`: Constructor arguments for `TObject`.

**Returns:** Typed runtime-component handle, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `TObject * SnAPI::GameFramework::WorldEcsRuntime::Component(const RuntimeNodeHandle Owner)`

Resolve a typed runtime component attached to a node.

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TObject * SnAPI::GameFramework::WorldEcsRuntime::Component(const RuntimeNodeHandle Owner) const`

Const overload of `Component<TObject>(...)`.

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldEcsRuntime::RemoveComponent(IWorld &WorldRef, const RuntimeNodeHandle Owner)`

Remove a typed runtime component from a node.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `Owner`: Runtime node that owns the component.

**Returns:** `true` when the underlying typed storage destroyed the component.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::WorldEcsRuntime::AddComponent(IWorld &WorldRef, const RuntimeNodeHandle Owner, const TypeId &Type)`

Dynamically create and attach a runtime component by reflected type id.

Unlike the typed overload, this path does not create a storage model implicitly. The target storage must already exist, usually because `Storage<T>()` was created earlier for that runtime type.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `Owner`: Runtime node that will own the component.
- `Type`: Reflected component type.

**Returns:** Generic runtime-component handle, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::WorldEcsRuntime::AddComponentWithId(IWorld &WorldRef, const RuntimeNodeHandle Owner, const TypeId &Type, const Uuid &Id)`

Dynamic overload of `AddComponent(...)` with explicit component UUID.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `Owner`: Runtime node that will own the component.
- `Type`: Reflected component type.
- `Id`: Explicit component UUID. A nil UUID requests auto-generation.

**Returns:** Generic runtime-component handle, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::WorldEcsRuntime::RemoveComponent(IWorld &WorldRef, const RuntimeNodeHandle Owner, const TypeId &Type)`

Remove a dynamically addressed runtime component from a node.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `Owner`: Runtime node that owns the component.
- `Type`: Reflected component type.

**Returns:** Success when the backing storage destroy path succeeded, otherwise an error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldEcsRuntime::HasComponent(const RuntimeNodeHandle Owner, const TypeId &Type) const`

Return `true` when a runtime node currently owns a component of the given reflected type.

**Parameters**

- `Owner`: 
- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< RuntimeComponentHandle > SnAPI::GameFramework::WorldEcsRuntime::ComponentHandle(const RuntimeNodeHandle Owner, const TypeId &Type) const`

Fetch the generic runtime-component handle attached to a node for a specific reflected type.

**Parameters**

- `Owner`: Runtime node owner.
- `Type`: Reflected component type.

**Returns:** Generic runtime-component handle, or an error when the attachment is absent.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::WorldEcsRuntime::ResolveComponentRaw(const RuntimeComponentHandle Handle, const TypeId &Type)`

Resolve a generic runtime-component handle to a raw mutable pointer.

**Parameters**

- `Handle`: Generic runtime-component handle.
- `Type`: Reflected component type expected by the caller.

**Returns:** Borrowed pointer to the live component, or `nullptr` when the handle/type pair does not resolve.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::WorldEcsRuntime::ResolveComponentRaw(const RuntimeComponentHandle Handle, const TypeId &Type) const`

Const overload of `ResolveComponentRaw(...)`.

**Parameters**

- `Handle`: 
- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::WorldEcsRuntime::DestroyRuntimeNode(IWorld &WorldRef, const RuntimeNodeHandle RootHandle)`

Destroy a runtime node subtree and all runtime components attached within that subtree.

Components are destroyed child-first before the node hierarchy itself is removed.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `RootHandle`: Root of the subtree to destroy.

**Returns:** Success or an error when the root handle is invalid.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::Tick(IWorld &WorldRef, const float DeltaSeconds)`

Execute variable-step runtime phases across all registered storages.

Per-storage execution order is ascending runtime tick priority, then storage creation order for ties. Within a storage, phases execute as `PreTick`, `Tick`, `PostTick`.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `DeltaSeconds`: Variable-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::FixedTick(IWorld &WorldRef, const float DeltaSeconds)`

Execute fixed-step runtime phases across all registered storages.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `DeltaSeconds`: Fixed-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::LateTick(IWorld &WorldRef, const float DeltaSeconds)`

Execute late runtime phases across all registered storages.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
- `DeltaSeconds`: Variable-step delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::FlushPendingOnCreate(IWorld &WorldRef)`

Flush deferred `OnCreate` hooks across all storages.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::Clear(IWorld &WorldRef)`

Destroy all runtime components and nodes and reset the ECS runtime to empty.

**Parameters**

- `WorldRef`: Owning world passed to lifecycle hooks.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `NodeComponentAttachment * SnAPI::GameFramework::WorldEcsRuntime::EnsureNodeAttachment(const RuntimeNodeHandle Owner)`

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `const NodeComponentAttachment * SnAPI::GameFramework::WorldEcsRuntime::FindNodeAttachment(const RuntimeNodeHandle Owner) const`

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeComponentAttachment * SnAPI::GameFramework::WorldEcsRuntime::FindNodeAttachment(const RuntimeNodeHandle Owner)`

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::ClearNodeAttachment(const RuntimeNodeHandle Owner)`

**Parameters**

- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::RemoveAllComponentsOnNode(IWorld &WorldRef, const RuntimeNodeHandle Owner)`

**Parameters**

- `WorldRef`: 
- `Owner`:
</div>
<div class="snapi-api-card" markdown="1">
### `IStorageModel * SnAPI::GameFramework::WorldEcsRuntime::FindStorageModel(const TypeId &Type)`

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `const IStorageModel * SnAPI::GameFramework::WorldEcsRuntime::FindStorageModel(const TypeId &Type) const`

**Parameters**

- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `IStorageModel * SnAPI::GameFramework::WorldEcsRuntime::FindStorageModelByToken(const uint32_t StorageToken)`

**Parameters**

- `StorageToken`:
</div>
<div class="snapi-api-card" markdown="1">
### `const IStorageModel * SnAPI::GameFramework::WorldEcsRuntime::FindStorageModelByToken(const uint32_t StorageToken) const`

**Parameters**

- `StorageToken`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldEcsRuntime::RegisterTickEntry(TDenseRuntimeStorage< TObject > *Storage)`

**Parameters**

- `Storage`:
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::WorldEcsRuntime::AcquireStorageToken()`
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static std::optional< std::size_t > SnAPI::GameFramework::WorldEcsRuntime::FindNodeComponentIndex(const NodeComponentAttachment &Attachment, const TypeId &Type)`

**Parameters**

- `Attachment`: 
- `Type`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldEcsRuntime::RemoveNodeComponentAt(NodeComponentAttachment &Attachment, const std::size_t Index)`

**Parameters**

- `Attachment`: 
- `Index`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldEcsRuntime::DispatchPreTick(void *StoragePtr, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `StoragePtr`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldEcsRuntime::DispatchTick(void *StoragePtr, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `StoragePtr`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldEcsRuntime::DispatchFixedTick(void *StoragePtr, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `StoragePtr`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldEcsRuntime::DispatchLateTick(void *StoragePtr, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `StoragePtr`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldEcsRuntime::DispatchPostTick(void *StoragePtr, IWorld &WorldRef, const float DeltaSeconds)`

**Parameters**

- `StoragePtr`: 
- `WorldRef`: 
- `DeltaSeconds`:
</div>
