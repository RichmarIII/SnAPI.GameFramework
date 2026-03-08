# SnAPI::GameFramework::TSystemTaskQueue

Generic task queue for a thread-owned system.

Threading semantics:
- `EnqueueTask()` and `EnqueueThreadTask()` are cross-thread safe and use a real mutex only for insertion.
- `ExecuteQueuedTasks()` must be called from the owner thread's update loop.
- Completion callbacks are marshaled back to the enqueuer's dispatcher when a `TaskDispatcherScope` was active at enqueue time.
- In debug builds, supplying a completion callback without a bound dispatcher asserts.
- When no dispatcher is captured and execution is still allowed, completion runs inline on the owner thread.

Execution semantics:
- Execution order is FIFO within the drained snapshot for one `ExecuteQueuedTasks()` call.
- New tasks enqueued during execution are deferred until the next drain.
- Cancellation only succeeds while the task is still queued.

## Contents

- **Type:** SnAPI::GameFramework::TSystemTaskQueue::PendingTask

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TSystemTaskQueue< TOwner >::WorkTask = std::function<void(TOwner&)>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TSystemTaskQueue< TOwner >::CompletionTask = std::function<void(const TaskHandle&)>`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::TSystemTaskQueue< TOwner >::m_enqueueMutex`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<PendingTask> SnAPI::GameFramework::TSystemTaskQueue< TOwner >::m_pending`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TSystemTaskQueue< TOwner >::TSystemTaskQueue()=default`

Construct an empty queue.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TSystemTaskQueue< TOwner >::TSystemTaskQueue(const TSystemTaskQueue &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `TSystemTaskQueue & SnAPI::GameFramework::TSystemTaskQueue< TOwner >::operator=(const TSystemTaskQueue &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TSystemTaskQueue< TOwner >::TSystemTaskQueue(TSystemTaskQueue &&Other) noexcept`

Move-construct queue contents with enqueue mutex protection.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `TSystemTaskQueue & SnAPI::GameFramework::TSystemTaskQueue< TOwner >::operator=(TSystemTaskQueue &&Other) noexcept`

Move-assign queue contents with enqueue mutex protection.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `TaskHandle SnAPI::GameFramework::TSystemTaskQueue< TOwner >::EnqueueTask(WorkTask Work, CompletionTask OnComplete={})`

Enqueue owner-thread work item.

**Parameters**

- `Work`: Work callback executed on owner thread.
- `OnComplete`: Optional completion callback.

**Returns:** Handle for wait/cancel/status operations.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TSystemTaskQueue< TOwner >::EnqueueThreadTask(std::function< void()> Task) override`

Enqueue raw callback directly onto owner thread queue.

**Parameters**

- `Task`: Callback to execute on owner thread.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TSystemTaskQueue< TOwner >::ExecuteQueuedTasks(TOwner &Owner, GameMutex &AffinityMutex)`

Execute and drain all queued work for owner thread.

**Parameters**

- `Owner`: Owning system instance passed into work callbacks.
- `AffinityMutex`: Owner thread-affinity validator.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TSystemTaskQueue< TOwner >::DispatchCompletion(CompletionTask OnComplete, ITaskDispatcher *CallerDispatcher, TaskHandle Handle)`

Dispatch completion callback to caller dispatcher when available.

**Parameters**

- `OnComplete`: Completion callback.
- `CallerDispatcher`: Dispatcher captured at enqueue time.
- `Handle`: Task handle with final state.
</div>
