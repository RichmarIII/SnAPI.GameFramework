# SnAPI::GameFramework::detail::TaskState

Internal shared task state backing `TaskHandle`.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::detail::TaskState::m_mutex`
</div>
<div class="snapi-api-card" markdown="1">
### `std::condition_variable SnAPI::GameFramework::detail::TaskState::m_cv`
</div>
<div class="snapi-api-card" markdown="1">
### `ETaskStatus SnAPI::GameFramework::detail::TaskState::m_status`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `ETaskStatus SnAPI::GameFramework::detail::TaskState::Status() const`

Read current task status.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::detail::TaskState::CancelIfQueued()`

Attempt to cancel task while still queued.

**Returns:** True when cancellation succeeded, false if task already started/finished.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::detail::TaskState::TryStart()`

Transition task from queued to running.

**Returns:** True when transition succeeds, false when task is not queued.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::detail::TaskState::Finish(ETaskStatus StatusValue)`

Mark running task as terminal and wake waiters.

**Parameters**

- `StatusValue`: Terminal status to set.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::detail::TaskState::Wait()`

Wait indefinitely until task reaches a terminal state.

**Returns:** Always true.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::detail::TaskState::WaitFor(const std::chrono::duration< Rep, Period > &Timeout)`

Wait for terminal state up to a timeout.

**Parameters**

- `Timeout`: Max wait duration.

**Returns:** True if terminal state reached before timeout, false otherwise.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::detail::TaskState::IsTerminal(const ETaskStatus StatusValue)`

Check whether a status is terminal.

**Parameters**

- `StatusValue`: Status to test.

**Returns:** True for completed/failed/canceled.
</div>
