# SnAPI::GameFramework::TaskHandle

Copyable handle for observing and canceling enqueued task work.

`TaskHandle` is a small shared-state wrapper that allows callers to:
- poll current status
- cancel queued work before execution starts
- wait for a terminal state

Invalid-handle semantics:
- invalid handles report `Completed`
- waiting on an invalid handle succeeds immediately
- canceling an invalid handle fails

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<detail::TaskState> SnAPI::GameFramework::TaskHandle::m_state`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TaskHandle::TaskHandle()=default`

Construct an invalid handle (no task state).
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TaskHandle::TaskHandle(std::shared_ptr< detail::TaskState > State)`

Construct a handle from shared task state.

**Parameters**

- `State`: Shared internal state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TaskHandle::IsValid() const`

Check whether handle references a real task.
</div>
<div class="snapi-api-card" markdown="1">
### `ETaskStatus SnAPI::GameFramework::TaskHandle::Status() const`

Get current task status.

**Returns:** Current status; invalid handles report `Completed`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TaskHandle::Cancel() const`

Cancel queued task.

**Returns:** True only when task was still queued and is now canceled.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TaskHandle::Wait() const`

Wait indefinitely for task completion/cancel/failure.

**Returns:** True when wait completed (or handle is invalid).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TaskHandle::WaitFor(const std::chrono::duration< Rep, Period > &Timeout) const`

Wait up to timeout for task completion/cancel/failure.

**Parameters**

- `Timeout`: Maximum duration to block.

**Returns:** True if task reached a terminal state before timeout.
</div>
