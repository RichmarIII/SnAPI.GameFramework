# SnAPI::GameFramework::GameMutex

Debug-time thread-affinity guard with a mutex-compatible API surface.

`GameMutex` deliberately does not provide mutual exclusion. It exists to validate that a thread-owned object is only touched by its owning thread during development.

Behavior summary:
- Debug: first access binds an owner token, later cross-thread access asserts.
- Release: all operations are compiled to no-ops.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::atomic<std::uint64_t> SnAPI::GameFramework::GameMutex::m_ownerThreadToken`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::GameMutex::GameMutex()=default`

Construct affinity guard.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::GameMutex::GameMutex(const GameMutex &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `GameMutex & SnAPI::GameFramework::GameMutex::operator=(const GameMutex &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::GameMutex::GameMutex(GameMutex &&Other) noexcept`

Move-construct affinity state.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `GameMutex & SnAPI::GameFramework::GameMutex::operator=(GameMutex &&Other) noexcept`

Move-assign affinity state.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameMutex::lock()`

Validate/bind thread ownership.

Release behavior:
- No-op.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameMutex::try_lock()`

Validate/bind thread ownership (try-lock form).

**Returns:** Always true.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameMutex::unlock() noexcept`

Unlock no-op.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameMutex::RebindCurrentThread()`

Rebind ownership to current thread.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::GameMutex::ResetBinding()`

Reset ownership so next thread can bind.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static std::uint64_t SnAPI::GameFramework::GameMutex::ThreadToken()`
</div>
