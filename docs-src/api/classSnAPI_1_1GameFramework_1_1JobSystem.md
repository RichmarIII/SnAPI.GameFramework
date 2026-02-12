# SnAPI::GameFramework::JobSystem

Minimal job system facade for internal parallelism.

## Private Members

<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::JobSystem::m_workerCount`

Desired worker count (not yet used).
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::JobSystem::WorkerCount(uint32_t Count)`

Set the desired worker count.

**Parameters**

- `Count`: Number of worker threads.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::JobSystem::WorkerCount() const`

Get the configured worker count.

**Returns:** Worker count value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::JobSystem::ParallelFor(size_t Count, const std::function< void(size_t)> &Fn) const`

Execute a parallel-for workload.

**Parameters**

- `Count`: Number of iterations.
- `Fn`: Function invoked per index.
</div>
