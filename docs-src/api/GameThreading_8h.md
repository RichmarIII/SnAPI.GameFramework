# File `GameThreading.h`

Cross-thread task handoff and thread-affinity validation primitives for engine systems.

Design goals:
- Most systems remain thread-owned rather than generally mutex-protected.
- Cross-thread mutation is routed through enqueue APIs.
- Real blocking synchronization is limited to enqueue and task wait state.
- Affinity validation is aggressive in debug builds and free in release.

## Contents

- **Namespace:** SnAPI
- **Namespace:** SnAPI::GameFramework
- **Namespace:** SnAPI::GameFramework::detail
- **Type:** SnAPI::GameFramework::ITaskDispatcher
- **Type:** SnAPI::GameFramework::TaskDispatcherScope
- **Type:** SnAPI::GameFramework::detail::TaskState
- **Type:** SnAPI::GameFramework::TaskHandle
- **Type:** SnAPI::GameFramework::GameMutex
- **Type:** SnAPI::GameFramework::TSystemTaskQueue
- **Type:** SnAPI::GameFramework::TSystemTaskQueue::PendingTask

## Macros

<div class="snapi-api-card" markdown="1">
### `SNAPI_GF_THREAD_AFFINITY_ENABLED`

Compile-time gate for thread-affinity validation code paths.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GF_THREAD_AFFINITY_ASSERT`

Debug-only assertion helper used by thread-affinity checks.

**Parameters**

- `condition`: Condition that must hold.
- `fmt`: std::format-compatible message.
- `...`:
</div>
