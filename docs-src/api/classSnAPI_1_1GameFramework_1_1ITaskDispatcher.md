# SnAPI::GameFramework::ITaskDispatcher

Dispatcher interface representing one thread-affinity domain.

Examples include world, renderer, physics, networking, audio, and UI threads.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::ITaskDispatcher::~ITaskDispatcher()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ITaskDispatcher::EnqueueThreadTask(std::function< void()> Task)=0`

Enqueue a callback onto the dispatcher's owner thread.

**Parameters**

- `Task`: Callback to execute on dispatcher thread.
</div>
