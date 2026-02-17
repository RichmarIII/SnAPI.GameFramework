#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Assert.h"

/**
 * @file GameThreading.h
 * @brief Cross-thread task handoff and thread-affinity validation primitives for GameFramework systems.
 * @remarks
 * Design goals:
 * - System internals are thread-owned and generally lock-free by ownership convention.
 * - Cross-thread mutation/interaction is routed through enqueue APIs.
 * - Real synchronization is limited to enqueue/wait state handoff.
 * - Thread-affinity validation is enabled in debug builds and compiled out in release.
 */

#if !defined(NDEBUG)
    /**
     * @brief Compile-time gate for thread-affinity validation code paths.
     * @remarks Enabled in debug builds, disabled in release builds.
     */
    #define SNAPI_GF_THREAD_AFFINITY_ENABLED 1
    /**
     * @brief Debug-only assertion helper used by thread-affinity checks.
     * @param condition Condition that must hold.
     * @param fmt std::format-compatible message.
     */
    #define SNAPI_GF_THREAD_AFFINITY_ASSERT(condition, fmt, ...) DEBUG_ASSERT(condition, fmt __VA_OPT__(,) __VA_ARGS__)
#else
    /**
     * @brief Compile-time gate for thread-affinity validation code paths.
     * @remarks Disabled in release builds to remove affinity-check runtime cost.
     */
    #define SNAPI_GF_THREAD_AFFINITY_ENABLED 0
    /**
     * @brief Release no-op for thread-affinity assertions.
     */
    #define SNAPI_GF_THREAD_AFFINITY_ASSERT(condition, fmt, ...) do { } while (0)
#endif

namespace SnAPI::GameFramework
{

/**
 * @brief Enforce lock-free 64-bit atomics for affinity-token checks.
 * @remarks
 * `GameMutex` uses `std::atomic<std::uint64_t>` for debug thread-ownership tokens.
 * This assertion guarantees those operations do not fall back to hidden locks on
 * supported targets.
 */
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "SnAPI.GameFramework requires lock-free std::atomic<std::uint64_t> for GameMutex affinity checks.");

/**
 * @brief Lifecycle state of an enqueued task.
 */
enum class ETaskStatus : std::uint8_t
{
    /** @brief Task is queued and not yet started. */
    Queued,
    /** @brief Task is currently executing. */
    Running,
    /** @brief Task finished successfully. */
    Completed,
    /** @brief Task execution started but finished with failure/exception. */
    Failed,
    /** @brief Task was canceled before execution started. */
    Canceled
};

class TaskHandle;

/**
 * @brief Dispatcher interface for thread-owned task queues.
 * @remarks
 * A dispatcher represents a thread affinity domain (for example world, physics,
 * renderer, audio, or networking). Callers enqueue closures to run on that owner
 * thread. Implementations are expected to provide the enqueue synchronization and
 * execute queued callbacks from that thread's update loop.
 */
class ITaskDispatcher
{
public:
    virtual ~ITaskDispatcher() = default;
    /**
     * @brief Enqueue callback onto dispatcher's owner thread.
     * @param Task Callback to execute on dispatcher thread.
     */
    virtual void EnqueueThreadTask(std::function<void()> Task) = 0;
};

/**
 * @brief RAII binding of the current thread to a dispatcher context.
 * @remarks
 * `TaskDispatcherScope` stores a thread-local pointer to the active dispatcher so
 * enqueue operations can capture where completion callbacks should be marshaled.
 * Scopes can be nested; previous bindings are restored on destruction.
 */
class TaskDispatcherScope final
{
public:
    /**
     * @brief Bind `Dispatcher` as current dispatcher for this thread.
     */
    explicit TaskDispatcherScope(ITaskDispatcher& Dispatcher)
        : m_previous(CurrentDispatcherStorage())
    {
        CurrentDispatcherStorage() = &Dispatcher;
    }

    /**
     * @brief Restore previous dispatcher binding for this thread.
     */
    ~TaskDispatcherScope()
    {
        CurrentDispatcherStorage() = m_previous;
    }

    TaskDispatcherScope(const TaskDispatcherScope&) = delete;
    TaskDispatcherScope& operator=(const TaskDispatcherScope&) = delete;

    /**
     * @brief Get currently bound dispatcher for this thread.
     * @return Dispatcher pointer or nullptr when no scope is active.
     */
    static ITaskDispatcher* Current()
    {
        return CurrentDispatcherStorage();
    }

private:
    static ITaskDispatcher*& CurrentDispatcherStorage()
    {
        static thread_local ITaskDispatcher* s_currentDispatcher = nullptr;
        return s_currentDispatcher;
    }

    ITaskDispatcher* m_previous = nullptr;
};

namespace detail
{

/**
 * @brief Internal shared task state backing `TaskHandle`.
 * @remarks
 * This object owns terminal-state signaling and wait semantics. It intentionally
 * uses a real mutex + condition_variable because it coordinates between producer
 * and consumer threads for `Wait` and status transitions.
 */
class TaskState final
{
public:
    /**
     * @brief Read current task status.
     */
    ETaskStatus Status() const
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        return m_status;
    }

    /**
     * @brief Attempt to cancel task while still queued.
     * @return True when cancellation succeeded, false if task already started/finished.
     */
    bool CancelIfQueued()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_status != ETaskStatus::Queued)
        {
            return false;
        }
        m_status = ETaskStatus::Canceled;
        m_cv.notify_all();
        return true;
    }

    /**
     * @brief Transition task from queued to running.
     * @return True when transition succeeds, false when task is not queued.
     */
    bool TryStart()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_status != ETaskStatus::Queued)
        {
            return false;
        }
        m_status = ETaskStatus::Running;
        return true;
    }

    /**
     * @brief Mark running task as terminal and wake waiters.
     * @param StatusValue Terminal status to set.
     */
    void Finish(ETaskStatus StatusValue)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_status == ETaskStatus::Running)
        {
            m_status = StatusValue;
        }
        m_cv.notify_all();
    }

    /**
     * @brief Wait indefinitely until task reaches a terminal state.
     * @return Always true.
     */
    bool Wait()
    {
        std::unique_lock<std::mutex> Lock(m_mutex);
        m_cv.wait(Lock, [this]() { return IsTerminal(m_status); });
        return true;
    }

    /**
     * @brief Wait for terminal state up to a timeout.
     * @tparam Rep Duration representation type.
     * @tparam Period Duration period type.
     * @param Timeout Max wait duration.
     * @return True if terminal state reached before timeout, false otherwise.
     */
    template<typename Rep, typename Period>
    bool WaitFor(const std::chrono::duration<Rep, Period>& Timeout)
    {
        std::unique_lock<std::mutex> Lock(m_mutex);
        return m_cv.wait_for(Lock, Timeout, [this]() { return IsTerminal(m_status); });
    }

    /**
     * @brief Check whether a status is terminal.
     * @param StatusValue Status to test.
     * @return True for completed/failed/canceled.
     */
    static bool IsTerminal(const ETaskStatus StatusValue)
    {
        return StatusValue == ETaskStatus::Completed
            || StatusValue == ETaskStatus::Failed
            || StatusValue == ETaskStatus::Canceled;
    }

private:
    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    ETaskStatus m_status = ETaskStatus::Queued;
};

} // namespace detail

/**
 * @brief Copyable handle for observing/control of an enqueued task.
 * @remarks
 * `TaskHandle` is a small shared-state wrapper that allows callers to:
 * - poll current status,
 * - cancel queued work before execution starts,
 * - wait for terminal state (completed/failed/canceled).
 *
 * Handles are safe to copy and pass across threads.
 */
class TaskHandle final
{
public:
    /** @brief Construct an invalid handle (no task state). */
    TaskHandle() = default;

    /**
     * @brief Construct a handle from shared task state.
     * @param State Shared internal state.
     */
    explicit TaskHandle(std::shared_ptr<detail::TaskState> State)
        : m_state(std::move(State))
    {
    }

    /**
     * @brief Check whether handle references a real task.
     */
    bool IsValid() const
    {
        return static_cast<bool>(m_state);
    }

    /**
     * @brief Get current task status.
     * @return Current status; invalid handles report `Completed`.
     */
    ETaskStatus Status() const
    {
        if (!m_state)
        {
            return ETaskStatus::Completed;
        }
        return m_state->Status();
    }

    /**
     * @brief Cancel queued task.
     * @return True only when task was still queued and is now canceled.
     * @remarks
     * Cancellation is best-effort and pre-start only. Once task transitions to
     * running (or any terminal state), cancellation fails.
     */
    bool Cancel() const
    {
        if (!m_state)
        {
            return false;
        }
        return m_state->CancelIfQueued();
    }

    /**
     * @brief Wait indefinitely for task completion/cancel/failure.
     * @return True when wait completed (or handle is invalid).
     */
    bool Wait() const
    {
        if (!m_state)
        {
            return true;
        }
        return m_state->Wait();
    }

    /**
     * @brief Wait up to timeout for task completion/cancel/failure.
     * @tparam Rep Duration representation type.
     * @tparam Period Duration period type.
     * @param Timeout Maximum duration to block.
     * @return True if task reached a terminal state before timeout.
     */
    template<typename Rep, typename Period>
    bool WaitFor(const std::chrono::duration<Rep, Period>& Timeout) const
    {
        if (!m_state)
        {
            return true;
        }
        return m_state->WaitFor(Timeout);
    }

private:
    std::shared_ptr<detail::TaskState> m_state{};
};

/**
 * @brief Debug-time thread-affinity guard with mutex-compatible API.
 * @remarks
 * `GameMutex` intentionally does not provide mutual exclusion. It exists to
 * validate that a thread-owned system/object is only touched by its owning
 * thread during development.
 *
 * Behavior summary:
 * - Debug: verifies/binds thread ownership and asserts on cross-thread access.
 * - Release: all operations compile to no-op.
 *
 * This enables lock-free system internals by ownership while retaining runtime
 * misuse detection in development builds.
 */
class GameMutex final
{
public:
    /**
     * @brief Construct affinity guard.
     * @remarks
     * This is not a mutual-exclusion mutex. In debug builds it validates thread
     * ownership; in release builds all operations compile to no-op.
     */
    GameMutex() = default;
    GameMutex(const GameMutex&) = delete;
    GameMutex& operator=(const GameMutex&) = delete;

    /** @brief Move-construct affinity state. */
    GameMutex(GameMutex&& Other) noexcept
    {
#if SNAPI_GF_THREAD_AFFINITY_ENABLED
        const std::uint64_t OtherToken = Other.m_ownerThreadToken.load(std::memory_order_relaxed);
        m_ownerThreadToken.store(OtherToken, std::memory_order_relaxed);
        Other.m_ownerThreadToken.store(0, std::memory_order_relaxed);
#else
        (void)Other;
#endif
    }

    /** @brief Move-assign affinity state. */
    GameMutex& operator=(GameMutex&& Other) noexcept
    {
        if (this == &Other)
        {
            return *this;
        }
#if SNAPI_GF_THREAD_AFFINITY_ENABLED
        const std::uint64_t OtherToken = Other.m_ownerThreadToken.load(std::memory_order_relaxed);
        m_ownerThreadToken.store(OtherToken, std::memory_order_relaxed);
        Other.m_ownerThreadToken.store(0, std::memory_order_relaxed);
#else
        (void)Other;
#endif
        return *this;
    }

    /**
     * @brief Validate/bind thread ownership.
     * @remarks
     * Debug behavior:
     * - First caller binds ownership token.
     * - Same-thread calls are fast-path no-op checks.
     * - Cross-thread calls assert.
     *
     * Release behavior:
     * - No-op.
     */
    void lock()
    {
#if SNAPI_GF_THREAD_AFFINITY_ENABLED
        const std::uint64_t CurrentThreadToken = ThreadToken();
        std::uint64_t OwnerThreadToken = m_ownerThreadToken.load(std::memory_order_relaxed);
        if (OwnerThreadToken == CurrentThreadToken)
        {
            return;
        }

        if (OwnerThreadToken == 0)
        {
            std::uint64_t Expected = 0;
            if (m_ownerThreadToken.compare_exchange_strong(Expected, CurrentThreadToken, std::memory_order_relaxed))
            {
                return;
            }
            OwnerThreadToken = Expected;
        }

        SNAPI_GF_THREAD_AFFINITY_ASSERT(OwnerThreadToken == CurrentThreadToken,
                                        "Thread-affinity violation: owner={}, current={}",
                                        OwnerThreadToken,
                                        CurrentThreadToken);
#endif
    }

    /**
     * @brief Validate/bind thread ownership (try-lock form).
     * @return Always true.
     * @remarks Provided for lock-guard compatibility.
     */
    bool try_lock()
    {
        lock();
        return true;
    }

    /**
     * @brief Unlock no-op.
     * @remarks Present for lock-guard compatibility.
     */
    void unlock() noexcept
    {
    }

    /**
     * @brief Rebind ownership to current thread.
     * @remarks No-op in release builds.
     */
    void RebindCurrentThread()
    {
#if SNAPI_GF_THREAD_AFFINITY_ENABLED
        m_ownerThreadToken.store(ThreadToken(), std::memory_order_relaxed);
#endif
    }

    /**
     * @brief Reset ownership so next thread can bind.
     * @remarks No-op in release builds.
     */
    void ResetBinding()
    {
#if SNAPI_GF_THREAD_AFFINITY_ENABLED
        m_ownerThreadToken.store(0, std::memory_order_relaxed);
#endif
    }

private:
#if SNAPI_GF_THREAD_AFFINITY_ENABLED
    static std::uint64_t ThreadToken()
    {
        std::uint64_t Token = static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        if (Token == 0)
        {
            Token = 1;
        }
        return Token;
    }

    std::atomic<std::uint64_t> m_ownerThreadToken{0};
#endif
};

/**
 * @brief Lock-guard alias for `GameMutex` affinity validation.
 * @remarks
 * Preserves familiar RAII call-sites (`GameLockGuard Lock(m_mutex);`) while
 * using debug-only ownership checks and release no-op behavior.
 */
using GameLockGuard = std::lock_guard<GameMutex>;

/**
 * @brief Generic enqueue-only task queue for a thread-owned system.
 * @tparam TOwner Owning system type executed by this queue.
 * @remarks
 * Threading semantics:
 * - `EnqueueTask` and `EnqueueThreadTask` are cross-thread safe and use a real
 *   mutex only for queue insertion.
 * - `ExecuteQueuedTasks` must be called from owner-thread update loop.
 * - Owner-thread affinity is optionally validated via `GameMutex`.
 * - Completion callbacks are marshaled to the caller's dispatcher (captured from
 *   `TaskDispatcherScope::Current()` at enqueue time).
 *
 * Task semantics:
 * - `Cancel()` succeeds only before task starts.
 * - Canceled tasks are not executed.
 * - Completion callback receives `TaskHandle` with final status.
 */
template<typename TOwner>
class TSystemTaskQueue final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(TOwner&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;

    /** @brief Construct an empty queue. */
    TSystemTaskQueue() = default;
    TSystemTaskQueue(const TSystemTaskQueue&) = delete;
    TSystemTaskQueue& operator=(const TSystemTaskQueue&) = delete;

    /** @brief Move-construct queue contents with enqueue mutex protection. */
    TSystemTaskQueue(TSystemTaskQueue&& Other) noexcept
    {
        std::lock_guard<std::mutex> Lock(Other.m_enqueueMutex);
        m_pending = std::move(Other.m_pending);
    }

    /** @brief Move-assign queue contents with enqueue mutex protection. */
    TSystemTaskQueue& operator=(TSystemTaskQueue&& Other) noexcept
    {
        if (this == &Other)
        {
            return *this;
        }
        std::scoped_lock Lock(m_enqueueMutex, Other.m_enqueueMutex);
        m_pending = std::move(Other.m_pending);
        return *this;
    }

    /**
     * @brief Enqueue owner-thread work item.
     * @param Work Work callback executed on owner thread.
     * @param OnComplete Optional completion callback.
     * @return Handle for wait/cancel/status operations.
     * @remarks
     * Completion callback runs on the enqueuer's dispatcher thread when available.
     * If no dispatcher is bound, completion runs inline at execution point.
     */
    TaskHandle EnqueueTask(WorkTask Work, CompletionTask OnComplete = {})
    {
        auto State = std::make_shared<detail::TaskState>();
        PendingTask Pending{};
        Pending.State = State;
        Pending.Work = std::move(Work);
        Pending.OnComplete = std::move(OnComplete);
        Pending.CallerDispatcher = TaskDispatcherScope::Current();
        DEBUG_ASSERT(!Pending.OnComplete || Pending.CallerDispatcher != nullptr,
                     "EnqueueTask completion callback requires a bound TaskDispatcherScope on caller thread");

        std::lock_guard<std::mutex> Lock(m_enqueueMutex);
        m_pending.push_back(std::move(Pending));
        return TaskHandle(State);
    }

    /**
     * @brief Enqueue raw callback directly onto owner thread queue.
     * @param Task Callback to execute on owner thread.
     * @remarks This path does not produce a `TaskHandle`.
     */
    void EnqueueThreadTask(std::function<void()> Task) override
    {
        PendingTask Pending{};
        Pending.ThreadTask = std::move(Task);
        std::lock_guard<std::mutex> Lock(m_enqueueMutex);
        m_pending.push_back(std::move(Pending));
    }

    /**
     * @brief Execute and drain all queued work for owner thread.
     * @param Owner Owning system instance passed into work callbacks.
     * @param AffinityMutex Owner thread-affinity validator.
     * @remarks
     * Execution order is FIFO based on queue snapshot order. New enqueues that
     * arrive while executing are deferred to the next call.
     */
    void ExecuteQueuedTasks(TOwner& Owner, GameMutex& AffinityMutex)
    {
        GameLockGuard AffinityLock(AffinityMutex);
        TaskDispatcherScope DispatcherScope(*this);

        std::vector<PendingTask> LocalPending;
        {
            std::lock_guard<std::mutex> Lock(m_enqueueMutex);
            LocalPending.swap(m_pending);
        }

        for (PendingTask& Pending : LocalPending)
        {
            if (Pending.ThreadTask)
            {
                Pending.ThreadTask();
                continue;
            }

            if (!Pending.State)
            {
                continue;
            }

            TaskHandle Handle(Pending.State);
            if (!Pending.State->TryStart())
            {
                if (detail::TaskState::IsTerminal(Pending.State->Status()))
                {
                    DispatchCompletion(std::move(Pending.OnComplete), Pending.CallerDispatcher, Handle);
                }
                continue;
            }

            ETaskStatus ResultStatus = ETaskStatus::Completed;
            try
            {
                if (Pending.Work)
                {
                    Pending.Work(Owner);
                }
            }
            catch (...)
            {
                ResultStatus = ETaskStatus::Failed;
            }

            Pending.State->Finish(ResultStatus);
            DispatchCompletion(std::move(Pending.OnComplete), Pending.CallerDispatcher, Handle);
        }
    }

private:
    /**
     * @brief Dispatch completion callback to caller dispatcher when available.
     * @param OnComplete Completion callback.
     * @param CallerDispatcher Dispatcher captured at enqueue time.
     * @param Handle Task handle with final state.
     */
    static void DispatchCompletion(CompletionTask OnComplete, ITaskDispatcher* CallerDispatcher, TaskHandle Handle)
    {
        if (!OnComplete)
        {
            return;
        }

        auto CompletionThunk = [Callback = std::move(OnComplete), Handle]() mutable {
            Callback(Handle);
        };

        if (CallerDispatcher)
        {
            CallerDispatcher->EnqueueThreadTask(std::move(CompletionThunk));
        }
        else
        {
            CompletionThunk();
        }
    }

    struct PendingTask
    {
        std::shared_ptr<detail::TaskState> State{};
        WorkTask Work{};
        CompletionTask OnComplete{};
        ITaskDispatcher* CallerDispatcher = nullptr;
        std::function<void()> ThreadTask{};
    };

    std::mutex m_enqueueMutex{};
    std::vector<PendingTask> m_pending{};
};

} // namespace SnAPI::GameFramework
