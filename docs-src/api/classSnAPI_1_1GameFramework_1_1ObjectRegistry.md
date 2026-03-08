# SnAPI::GameFramework::ObjectRegistry

Process-wide bridge from UUID/runtime-handle identity to live object pointers.

The registry exists to support two complementary lookup modes:
- fast-path resolution through `(runtime pool token, runtime index, generation)`
- fallback resolution through stable UUID identity

Higher-level systems such as node pools, component storage, and ECS runtime storages register objects here so `THandle` can cheaply recover a live pointer without each subsystem reinventing the same indirection table.

Ownership and lifetime:
- The registry never owns the objects it points at.
- Stored pointers are borrowed and become invalid as soon as the owning system unregisters the UUID.
- Pool tokens are registry-owned identities that outlive the pool instance and are intentionally never reused.

Threading:
- Not generally thread-safe.
- Internal `GameMutex` use performs affinity validation only; it is not a real cross-thread lock.
- Register, unregister, and resolve on the owning thread or under external synchronization.

Error semantics:
- Missing entries resolve to `nullptr`.
- Type mismatches also resolve to `nullptr`; no exception or cast failure is thrown.

## Contents

- **Type:** SnAPI::GameFramework::ObjectRegistry::RuntimeIdentity
- **Type:** SnAPI::GameFramework::ObjectRegistry::Entry
- **Type:** SnAPI::GameFramework::ObjectRegistry::RuntimeSlot

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ObjectRegistry::kInvalidRuntimePoolToken`

Runtime pool token sentinel meaning "no runtime pool".
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ObjectRegistry::kInvalidRuntimeIndex`

Runtime slot index sentinel meaning "no runtime slot".
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::ObjectRegistry::m_mutex`

Protects registry state.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<Uuid, Entry, UuidHash> SnAPI::GameFramework::ObjectRegistry::m_entries`

UUID -> entry map (fallback path).
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::vector<RuntimeSlot> > SnAPI::GameFramework::ObjectRegistry::m_runtimeSlotsByPool`

Runtime pool token -> runtime slots (fast path).
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<Uuid, uint64_t, UuidHash> SnAPI::GameFramework::ObjectRegistry::m_fastPathFallbackCounts`

Per-object fast-path miss counters for fallback diagnostics.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static ObjectRegistry & SnAPI::GameFramework::ObjectRegistry::Instance()`

Access the process-wide singleton registry.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ObjectRegistry::AcquireRuntimePoolToken()`

Acquire a fresh runtime-pool token for one handle-producing storage instance.

Tokens are monotonically assigned and never reused. That prevents a stale handle from accidentally resolving into a different pool that later reused the same slot index and generation.

**Returns:** Token used to bind runtime index lookups for a pool instance.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::ReleaseRuntimePoolToken(uint32_t PoolToken)`

Clear all runtime-slot bindings currently associated with a pool token.

Release does not recycle the token number. It only clears the fast-path runtime slots so old handles stop resolving by runtime key.

**Parameters**

- `PoolToken`: Token to release.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterNode(const Uuid &Id, BaseNode *Node)`

Register a node for UUID-based lookup only.

If an entry already exists for `Id`, the old registration is replaced.

**Parameters**

- `Id`: UUID of the node.
- `Node`: Borrowed node pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterNode(const Uuid &Id, BaseNode *Node, uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration)`

Register a node with both UUID fallback identity and runtime-key fast-path identity.

**Parameters**

- `Id`: UUID of the node.
- `Node`: Borrowed node pointer.
- `RuntimePoolToken`: Runtime pool token from owning pool.
- `RuntimeIndex`: Runtime slot index in owning pool.
- `RuntimeGeneration`: Runtime slot generation in owning pool.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterComponent(const Uuid &Id, BaseComponent *Component)`

Register a component for UUID-based lookup only.

**Parameters**

- `Id`: 
- `Component`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterComponent(const Uuid &Id, BaseComponent *Component, uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration)`

Register a component with both UUID fallback identity and runtime-key fast-path identity.

**Parameters**

- `Id`: UUID of the component.
- `Component`: Borrowed component pointer.
- `RuntimePoolToken`: Runtime pool token from owning pool.
- `RuntimeIndex`: Runtime slot index in owning pool.
- `RuntimeGeneration`: Runtime slot generation in owning pool.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::Register(const Uuid &Id, T *Object)`

Register an arbitrary non-node, non-component object.

For `EObjectKind::Other`, resolution uses exact `typeid(T)` equality. Registering a derived object under `TBase` does not make `Resolve<TDerived>()` succeed.

**Parameters**

- `Id`: UUID of the object.
- `Object`: Borrowed pointer to the object.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::Unregister(const Uuid &Id)`

Remove an object's UUID and runtime-slot bindings from the registry.

**Parameters**

- `Id`: UUID to remove.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::Resolve(const Uuid &Id) const`

Resolve an object through the UUID fallback map only.

**Parameters**

- `Id`: UUID to resolve.

**Returns:** Borrowed pointer to the object, or `nullptr` when missing or type-mismatched.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::ResolveFast(const Uuid &Id, uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration) const`

Resolve an object by runtime key and silently fall back to UUID lookup when needed.

This overload discards any refreshed runtime identity. Callers that want to rehydrate the handle cache should use `ResolveFastOrFallback()` instead.

**Parameters**

- `Id`: UUID for safety/fallback.
- `RuntimePoolToken`: Runtime pool token.
- `RuntimeIndex`: Runtime slot index.
- `RuntimeGeneration`: Runtime slot generation.

**Returns:** Borrowed pointer to the object, or `nullptr` when missing or type-mismatched.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::ResolveFastOrFallback(const Uuid &Id, uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration, RuntimeIdentity *OutIdentity) const`

Resolve an object by runtime key, then fall back to UUID lookup and report a refreshed runtime identity.

Semantics:
- First tries the runtime slot table.
- If that misses, falls back to UUID lookup.
- When the fallback succeeds, emits a rate-limited warning to `stderr`.
- When `OutIdentity` is non-null and the entry has a runtime identity, the refreshed identity is written back for handle rehydration.

**Parameters**

- `Id`: UUID for fallback path.
- `RuntimePoolToken`: Runtime pool token.
- `RuntimeIndex`: Runtime slot index.
- `RuntimeGeneration`: Runtime slot generation.
- `OutIdentity`: Optional refreshed runtime identity when resolved.

**Returns:** Borrowed pointer to the object, or `nullptr` when missing or type-mismatched.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ObjectRegistry::IsValid(const Uuid &Id) const`

Check whether a UUID resolves to a live object of type `T`.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ObjectRegistry::IsValidFast(const Uuid &Id, uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration) const`

Check whether runtime-key lookup resolves to a live object of type `T`.

**Parameters**

- `Id`: 
- `RuntimePoolToken`: 
- `RuntimeIndex`: 
- `RuntimeGeneration`:
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::ObjectRegistry::HasRuntimeKey(uint32_t PoolToken, uint32_t Index)`

**Parameters**

- `PoolToken`: 
- `Index`:
</div>
<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::ObjectRegistry::HasRuntimeKey(const Entry &EntryRef)`

**Parameters**

- `EntryRef`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::ResolveFromEntryLocked(const Entry &EntryRef) const`

**Parameters**

- `EntryRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::ResolveFromRuntimeSlotLocked(const RuntimeSlot &Slot) const`

**Parameters**

- `Slot`:
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::ObjectRegistry::ResolveByIdLocked(const Uuid &Id) const`

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::EnsureRuntimeSlotLocked(uint32_t PoolToken, uint32_t RuntimeIndex)`

**Parameters**

- `PoolToken`: 
- `RuntimeIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::BindRuntimeSlotLocked(const Entry &EntryRef)`

**Parameters**

- `EntryRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::ClearRuntimeSlotLocked(const Entry &EntryRef)`

**Parameters**

- `EntryRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::ObjectRegistry::RegisterInternal(const Uuid &Id, EObjectKind Kind, BaseNode *Node, BaseComponent *Component, void *Other, std::type_index Type, uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration)`

**Parameters**

- `Id`: 
- `Kind`: 
- `Node`: 
- `Component`: 
- `Other`: 
- `Type`: 
- `RuntimePoolToken`: 
- `RuntimeIndex`: 
- `RuntimeGeneration`:
</div>
