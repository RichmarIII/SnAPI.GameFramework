# SnAPI::GameFramework::TaskDispatcherScope

RAII binding of the current thread to a dispatcher context.

`TaskDispatcherScope` stores a thread-local pointer to the active dispatcher so enqueue operations can capture where completion callbacks should later be marshaled.

Scopes can be nested; the previous binding is restored on destruction.

## Private Members

<div class="snapi-api-card" markdown="1">
### `ITaskDispatcher* SnAPI::GameFramework::TaskDispatcherScope::m_previous`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TaskDispatcherScope::TaskDispatcherScope(ITaskDispatcher &Dispatcher)`

Bind `Dispatcher` as current dispatcher for this thread.

**Parameters**

- `Dispatcher`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TaskDispatcherScope::~TaskDispatcherScope()`

Restore previous dispatcher binding for this thread.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TaskDispatcherScope::TaskDispatcherScope(const TaskDispatcherScope &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `TaskDispatcherScope & SnAPI::GameFramework::TaskDispatcherScope::operator=(const TaskDispatcherScope &)=delete`
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static ITaskDispatcher * SnAPI::GameFramework::TaskDispatcherScope::Current()`

Get currently bound dispatcher for this thread.

**Returns:** Dispatcher pointer or nullptr when no scope is active.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static ITaskDispatcher *& SnAPI::GameFramework::TaskDispatcherScope::CurrentDispatcherStorage()`
</div>
