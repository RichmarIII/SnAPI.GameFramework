# SnAPI::GameFramework::ScopedComponentOnCreateSuppression

RAII helper that defers runtime/component `OnCreate` hooks for the current thread.

Nested scopes are supported. Destruction restores the previous suppression state rather than unconditionally setting the flag to `false`.

Typical usage:
- create nodes/components/runtime records during bootstrap or deserialization
- initialize dependent systems
- call the relevant `FlushPendingOnCreate()` API once the environment is ready

Threading:
- Affects only the current thread because the underlying flag is thread-local.

## Private Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::ScopedComponentOnCreateSuppression::m_previousState`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScopedComponentOnCreateSuppression::ScopedComponentOnCreateSuppression()`

Enable `OnCreate` suppression for the current thread, preserving the previous state.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScopedComponentOnCreateSuppression::~ScopedComponentOnCreateSuppression()`

Restore the previous suppression state for the current thread.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::ScopedComponentOnCreateSuppression::ScopedComponentOnCreateSuppression(const ScopedComponentOnCreateSuppression &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `ScopedComponentOnCreateSuppression & SnAPI::GameFramework::ScopedComponentOnCreateSuppression::operator=(const ScopedComponentOnCreateSuppression &)=delete`
</div>
