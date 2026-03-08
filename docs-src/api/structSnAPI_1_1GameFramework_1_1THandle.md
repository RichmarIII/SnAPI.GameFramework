# SnAPI::GameFramework::THandle

Strongly typed, non-owning identity token for framework objects.

Forward declaration of the strong typed-handle wrapper used throughout GameFramework.

`THandle<T>` is the public identity boundary for world-owned objects such as nodes and components. A handle stores the stable UUID that survives serialization, replication, and deferred-destroy windows, plus optional runtime slot metadata used as a fast-path for hot resolution through `ObjectRegistry`.

Why this exists:
- raw pointers are fast but unsafe to persist across frames, loads, or destroy queues
- UUIDs are stable but expensive to hash/resolve repeatedly in hot paths
- `THandle` combines both: a stable external identity plus an internal cached runtime key

Core semantics:
- Handles never own the target object.
- Equality compares stable UUID identity, not pointer identity.
- A non-null handle may still fail to resolve if the object has been destroyed or is not loaded.
- Successful `Borrowed()` resolution may refresh the cached runtime key on the handle instance.

Ownership and lifetime:
- The caller owns only the handle value, never the resolved object.
- Borrowed pointers returned from `Borrowed()` are transient views and must not be cached.
- The handle may outlive the target object; resolution then returns `nullptr`.

Threading:
- Copying and comparing handles is thread-safe.
- Calling `Borrowed()` on the same handle instance from multiple threads is not thread-safe, because the runtime cache fields are updated lazily.
- External synchronization is required if one handle instance is shared across threads.

Performance:
- Fast path is O(1) when runtime key fields are valid.
- Slow UUID fallback requires registry lookup and is more expensive; avoid it in hot loops.

`THandle<T>` is forward-declared here so headers can accept or store handle types without pulling in the full handle implementation and all of its dependencies.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::THandle< T >::kInvalidRuntimePoolToken`

Sentinel runtime pool token representing "no runtime key".
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::THandle< T >::kInvalidRuntimeIndex`

Sentinel runtime slot index representing "no runtime key".
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::THandle< T >::Id`

Stable UUID of the referenced object; this is the canonical identity used for equality and persistence.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::THandle< T >::RuntimePoolToken`

Optional cached pool token used to bypass UUID lookup during hot resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::THandle< T >::RuntimeIndex`

Optional cached slot index paired with `RuntimePoolToken` for fast lookup.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::THandle< T >::RuntimeGeneration`

Cached generation used to reject stale slot reuse after object destruction.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::THandle()=default`

Construct a null handle.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::THandle(Uuid InId)`

Construct a handle from a UUID.

**Parameters**

- `InId`: UUID of the target object.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::THandle(Uuid InId, uint32_t InRuntimePoolToken, uint32_t InRuntimeIndex, uint32_t InRuntimeGeneration)`

Construct a handle from UUID plus runtime slot identity.

**Parameters**

- `InId`: UUID of the target object.
- `InRuntimePoolToken`: Pool token.
- `InRuntimeIndex`: Pool slot index.
- `InRuntimeGeneration`: Pool slot generation.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::IsNull() const noexcept`

Check if the handle is null.

**Returns:** True when the UUID is nil.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::operator bool() const noexcept`

Boolean conversion for validity checks.

**Returns:** True when the handle is not null.

**Notes**

- This does not guarantee the object is loaded.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::HasRuntimeKey() const noexcept`

Check whether runtime slot identity is present.

**Returns:** True when `RuntimeIndex` contains a valid slot id.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::operator==(const THandle &Other) const noexcept`

Equality comparison.

**Parameters**

- `Other`: Another handle.

**Returns:** True when UUIDs match.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::operator!=(const THandle &Other) const noexcept`

Inequality comparison.

**Parameters**

- `Other`: Another handle.

**Returns:** True when UUIDs differ.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::THandle< T >::Borrowed() const`

Resolve to a non-owning pointer using cached runtime identity when possible.

**Returns:** Non-owning pointer to the object, or `nullptr` if the object is not currently registered.

**Notes**

- The returned pointer must not be stored across frames or destroy boundaries.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::THandle< T >::Borrowed()`

Resolve to a non-owning pointer using cached runtime identity when possible.

**Returns:** Non-owning pointer to the object, or `nullptr` if the object is not currently registered.

**Notes**

- The returned pointer must not be stored across frames or destroy boundaries.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::THandle< T >::BorrowedSlowByUuid() const`

Resolve by UUID using registry hash lookup (slow path).

**Returns:** Pointer to object or nullptr if missing/type mismatch.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::THandle< T >::BorrowedSlowByUuid()`

Resolve by UUID using registry hash lookup (slow path).

**Returns:** Pointer to object or nullptr if missing/type mismatch.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::IsValid() const`

Check whether the handle resolves to a live object through the fast path.

**Returns:** `true` when the object is currently registered and reachable through `Borrowed()`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::IsValidSlowByUuid() const`

Validate by UUID using registry hash lookup (slow path).

**Returns:** True when object resolves by UUID.
</div>
