# SnAPI::GameFramework::ObjectRegistry::RuntimeIdentity

Refreshed runtime identity returned after a UUID fallback successfully finds a live object.

Callers that fall back from runtime-key lookup should copy this data back into the same handle instance so future resolutions can return to the fast path.

## Public Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ObjectRegistry::RuntimeIdentity::RuntimePoolToken`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ObjectRegistry::RuntimeIdentity::RuntimeIndex`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::ObjectRegistry::RuntimeIdentity::RuntimeGeneration`
</div>
