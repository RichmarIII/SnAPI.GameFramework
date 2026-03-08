# SnAPI::GameFramework::TDenseRuntimeHandle

Generation-safe handle used by dense ECS runtime storages.

`TDenseRuntimeHandle` is the runtime-only equivalent of an engine handle:
- `Id` provides stable identity across serialization-like boundaries
- `StorageToken` identifies the owning dense storage instance
- `Index` addresses the current slot inside that storage
- `Generation` rejects stale handles after slot reuse

Ownership and lifetime:
- The handle is a value type and owns no object memory.
- A handle remains valid only while the target slot is alive and its generation matches.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::kInvalidStorageToken`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::kInvalidIndex`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::Id`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::StorageToken`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::Index`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::Generation`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::IsNull() const noexcept`

Return `true` when the handle carries no UUID identity.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::HasRuntimeKey() const noexcept`

Return `true` when the handle contains a storage token plus slot index.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::operator bool() const noexcept`

Boolean test for non-null handle identity.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDenseRuntimeHandle< TObject >::operator==(const TDenseRuntimeHandle &) const noexcept=default`
</div>
