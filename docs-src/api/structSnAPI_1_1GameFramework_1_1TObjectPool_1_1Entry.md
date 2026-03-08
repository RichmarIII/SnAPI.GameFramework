# SnAPI::GameFramework::TObjectPool::Entry

Internal slot payload for one object entry.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::TObjectPool< T >::Entry::Id`

UUID key for this entry.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TObjectPool< T >::Entry::Generation`

Slot generation used for stale-handle rejection.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<T> SnAPI::GameFramework::TObjectPool< T >::Entry::m_uniqueObject`

Standard ownership path for pool-created objects.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<T> SnAPI::GameFramework::TObjectPool< T >::Entry::m_sharedObject`

Shared ownership path for externally-owned inserts.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::Entry::m_pendingDestroy`

True when scheduled for deletion.
</div>
