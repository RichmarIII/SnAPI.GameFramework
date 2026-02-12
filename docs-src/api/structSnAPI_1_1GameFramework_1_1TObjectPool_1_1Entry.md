# SnAPI::GameFramework::TObjectPool::Entry

Internal storage entry.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::TObjectPool< T >::Entry::Id`

UUID key for this entry.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<T> SnAPI::GameFramework::TObjectPool< T >::Entry::m_object`

Stored object pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TObjectPool< T >::Entry::m_pendingDestroy`

True when scheduled for deletion.
</div>
