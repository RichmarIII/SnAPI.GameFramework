# SnAPI::GameFramework::JobSystem

Minimal job-system facade for internal data-parallel work.

This is currently a serial compatibility layer rather than a real scheduler. Its main purpose is to give callers a stable API boundary that can later be backed by a true worker pool without rewriting call sites.

Current semantics:
- `ParallelFor()` executes synchronously on the calling thread.
- `WorkerCount` is configuration state only and does not affect execution yet.

## Private Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::JobSystem::m_workerCount`

Desired worker count (not yet used).
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::JobSystem::WorkerCount(uint32_t Count)`

Set the desired worker-count hint.

The current implementation stores the value only for future use.

**Parameters**

- `Count`: Number of worker threads.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::JobSystem::WorkerCount() const`

Get the configured worker-count hint.

**Returns:** Worker-count hint value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::JobSystem::ParallelFor(size_t Count, const std::function< void(size_t)> &Fn) const`

Execute a parallel-for style workload.

Current implementation:
- deterministic serial execution
- on the calling thread
- with one callback per index in `[0, Count)`

**Parameters**

- `Count`: Number of iterations.
- `Fn`: Function invoked per index.
</div>
