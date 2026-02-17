#include <chrono>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "GameThreading.h"

using namespace SnAPI::GameFramework;
using namespace std::chrono_literals;

namespace
{

struct ThreadingTestOwner
{
    int ExecutedCount = 0;
    int Accumulator = 0;
    std::thread::id LastExecutionThread{};
};

class ThreadingTestSystem final
{
public:
    using QueueType = TSystemTaskQueue<ThreadingTestOwner>;

    ~ThreadingTestSystem()
    {
        Stop();
    }

    void Start()
    {
        if (m_thread.joinable())
        {
            return;
        }

        m_stopRequested.store(false, std::memory_order_release);

        std::promise<std::thread::id> StartedPromise;
        auto StartedFuture = StartedPromise.get_future();
        m_thread = std::thread([this, StartedPromise = std::move(StartedPromise)]() mutable {
            StartedPromise.set_value(std::this_thread::get_id());
            while (!m_stopRequested.load(std::memory_order_acquire))
            {
                Queue.ExecuteQueuedTasks(Owner, Mutex);
                std::this_thread::sleep_for(1ms);
            }
            Queue.ExecuteQueuedTasks(Owner, Mutex);
        });

        OwnerThreadId = StartedFuture.get();
    }

    void Stop()
    {
        if (!m_thread.joinable())
        {
            return;
        }

        m_stopRequested.store(true, std::memory_order_release);
        m_thread.join();
    }

    QueueType Queue{};
    GameMutex Mutex{};
    ThreadingTestOwner Owner{};
    std::thread::id OwnerThreadId{};

private:
    std::atomic<bool> m_stopRequested{false};
    std::thread m_thread{};
};

} // namespace

TEST_CASE("System task queue executes all enqueued work under multi-thread contention", "[threading]")
{
    ThreadingTestSystem System;
    System.Start();

    constexpr int kProducerCount = 8;
    constexpr int kTasksPerProducer = 200;
    constexpr int kExpectedTaskCount = kProducerCount * kTasksPerProducer;

    std::vector<TaskHandle> Handles;
    Handles.reserve(static_cast<std::size_t>(kExpectedTaskCount));
    std::mutex HandlesMutex{};

    std::vector<std::thread> Producers;
    Producers.reserve(kProducerCount);
    for (int ProducerIndex = 0; ProducerIndex < kProducerCount; ++ProducerIndex)
    {
        Producers.emplace_back([&]() {
            std::vector<TaskHandle> LocalHandles;
            LocalHandles.reserve(kTasksPerProducer);
            for (int TaskIndex = 0; TaskIndex < kTasksPerProducer; ++TaskIndex)
            {
                TaskHandle Handle = System.Queue.EnqueueTask([](ThreadingTestOwner& Owner) {
                    ++Owner.ExecutedCount;
                    Owner.Accumulator += 1;
                    Owner.LastExecutionThread = std::this_thread::get_id();
                });
                LocalHandles.push_back(Handle);
            }

            std::lock_guard<std::mutex> Lock(HandlesMutex);
            Handles.insert(Handles.end(), LocalHandles.begin(), LocalHandles.end());
        });
    }

    for (std::thread& Producer : Producers)
    {
        Producer.join();
    }

    REQUIRE(Handles.size() == static_cast<std::size_t>(kExpectedTaskCount));
    for (const TaskHandle& Handle : Handles)
    {
        REQUIRE(Handle.WaitFor(5s));
        REQUIRE(Handle.Status() == ETaskStatus::Completed);
    }

    System.Stop();

    REQUIRE(System.Owner.ExecutedCount == kExpectedTaskCount);
    REQUIRE(System.Owner.Accumulator == kExpectedTaskCount);
    REQUIRE(System.Owner.LastExecutionThread == System.OwnerThreadId);
}

TEST_CASE("TaskHandle cancel before execution suppresses work and reports canceled", "[threading]")
{
    TSystemTaskQueue<ThreadingTestOwner> Queue;
    ThreadingTestOwner Owner{};
    GameMutex Mutex{};

    bool Ran = false;
    bool CompletionCalled = false;
    ETaskStatus CompletionStatus = ETaskStatus::Queued;

    TaskHandle Handle{};
    {
        TaskDispatcherScope DispatcherScope(Queue);
        Handle = Queue.EnqueueTask(
            [&](ThreadingTestOwner&) {
                Ran = true;
            },
            [&](const TaskHandle& CompletedHandle) {
                CompletionCalled = true;
                CompletionStatus = CompletedHandle.Status();
            });
    }

    REQUIRE(Handle.Cancel());
    REQUIRE(Handle.Status() == ETaskStatus::Canceled);

    Queue.ExecuteQueuedTasks(Owner, Mutex);
    Queue.ExecuteQueuedTasks(Owner, Mutex);

    REQUIRE_FALSE(Ran);
    REQUIRE(Handle.WaitFor(100ms));
    REQUIRE(Handle.Status() == ETaskStatus::Canceled);
    REQUIRE(CompletionCalled);
    REQUIRE(CompletionStatus == ETaskStatus::Canceled);
}

TEST_CASE("TaskHandle cancel fails once work has started", "[threading]")
{
    ThreadingTestSystem System;
    System.Start();

    std::promise<void> StartedPromise;
    auto StartedFuture = StartedPromise.get_future();

    std::promise<void> ReleasePromise;
    std::shared_future<void> ReleaseFuture = ReleasePromise.get_future().share();

    TaskHandle Handle = System.Queue.EnqueueTask([&](ThreadingTestOwner& Owner) {
        StartedPromise.set_value();
        ReleaseFuture.wait();
        ++Owner.ExecutedCount;
    });

    REQUIRE(StartedFuture.wait_for(2s) == std::future_status::ready);
    REQUIRE_FALSE(Handle.Cancel());

    ReleasePromise.set_value();
    REQUIRE(Handle.WaitFor(2s));
    REQUIRE(Handle.Status() == ETaskStatus::Completed);

    System.Stop();
    REQUIRE(System.Owner.ExecutedCount == 1);
}

TEST_CASE("TaskHandle wait timeout and later completion work correctly", "[threading]")
{
    TSystemTaskQueue<ThreadingTestOwner> Queue;
    ThreadingTestOwner Owner{};
    GameMutex Mutex{};

    TaskHandle Handle = Queue.EnqueueTask([](ThreadingTestOwner& InOwner) {
        ++InOwner.ExecutedCount;
    });

    REQUIRE_FALSE(Handle.WaitFor(20ms));
    Queue.ExecuteQueuedTasks(Owner, Mutex);
    REQUIRE(Handle.WaitFor(200ms));
    REQUIRE(Handle.Status() == ETaskStatus::Completed);
    REQUIRE(Owner.ExecutedCount == 1);
}

TEST_CASE("Task exceptions transition handle to failed status", "[threading]")
{
    TSystemTaskQueue<ThreadingTestOwner> Queue;
    ThreadingTestOwner Owner{};
    GameMutex Mutex{};

    TaskHandle Handle = Queue.EnqueueTask([](ThreadingTestOwner&) {
        throw std::runtime_error("threading test failure");
    });

    Queue.ExecuteQueuedTasks(Owner, Mutex);

    REQUIRE(Handle.WaitFor(200ms));
    REQUIRE(Handle.Status() == ETaskStatus::Failed);
}

TEST_CASE("Completion callback marshals to caller dispatcher thread", "[threading]")
{
    ThreadingTestSystem CallerSystem;
    ThreadingTestSystem WorkerSystem;
    CallerSystem.Start();
    WorkerSystem.Start();

    std::promise<TaskHandle> HandlePromise;
    auto HandleFuture = HandlePromise.get_future();

    std::promise<std::thread::id> WorkThreadPromise;
    auto WorkThreadFuture = WorkThreadPromise.get_future();

    std::promise<std::thread::id> CompletionThreadPromise;
    auto CompletionThreadFuture = CompletionThreadPromise.get_future();

    CallerSystem.Queue.EnqueueThreadTask([&]() {
        TaskHandle Handle = WorkerSystem.Queue.EnqueueTask(
            [&](ThreadingTestOwner& Owner) {
                ++Owner.ExecutedCount;
                WorkThreadPromise.set_value(std::this_thread::get_id());
            },
            [&](const TaskHandle&) {
                CompletionThreadPromise.set_value(std::this_thread::get_id());
            });
        HandlePromise.set_value(Handle);
    });

    REQUIRE(HandleFuture.wait_for(2s) == std::future_status::ready);
    TaskHandle Handle = HandleFuture.get();
    REQUIRE(Handle.WaitFor(2s));
    REQUIRE(Handle.Status() == ETaskStatus::Completed);

    REQUIRE(WorkThreadFuture.wait_for(2s) == std::future_status::ready);
    REQUIRE(CompletionThreadFuture.wait_for(2s) == std::future_status::ready);
    const std::thread::id WorkThreadId = WorkThreadFuture.get();
    const std::thread::id CompletionThreadId = CompletionThreadFuture.get();

    WorkerSystem.Stop();
    CallerSystem.Stop();

    REQUIRE(WorkThreadId == WorkerSystem.OwnerThreadId);
    REQUIRE(CompletionThreadId == CallerSystem.OwnerThreadId);
    REQUIRE(CompletionThreadId != WorkerSystem.OwnerThreadId);
}

TEST_CASE("Canceled-task completion callback marshals to caller dispatcher thread", "[threading]")
{
    ThreadingTestSystem CallerSystem;
    CallerSystem.Start();

    TSystemTaskQueue<ThreadingTestOwner> WorkerQueue;
    ThreadingTestOwner WorkerOwner{};
    GameMutex WorkerMutex{};

    std::promise<TaskHandle> HandlePromise;
    auto HandleFuture = HandlePromise.get_future();

    std::promise<std::thread::id> CompletionThreadPromise;
    auto CompletionThreadFuture = CompletionThreadPromise.get_future();

    std::promise<ETaskStatus> CompletionStatusPromise;
    auto CompletionStatusFuture = CompletionStatusPromise.get_future();

    CallerSystem.Queue.EnqueueThreadTask([&]() {
        TaskHandle Handle = WorkerQueue.EnqueueTask(
            [&](ThreadingTestOwner& Owner) {
                ++Owner.ExecutedCount;
            },
            [&](const TaskHandle& CompletedHandle) {
                CompletionThreadPromise.set_value(std::this_thread::get_id());
                CompletionStatusPromise.set_value(CompletedHandle.Status());
            });
        HandlePromise.set_value(Handle);
    });

    REQUIRE(HandleFuture.wait_for(2s) == std::future_status::ready);
    TaskHandle Handle = HandleFuture.get();
    REQUIRE(Handle.Cancel());

    WorkerQueue.ExecuteQueuedTasks(WorkerOwner, WorkerMutex);

    REQUIRE(CompletionThreadFuture.wait_for(2s) == std::future_status::ready);
    REQUIRE(CompletionStatusFuture.wait_for(2s) == std::future_status::ready);
    const std::thread::id CompletionThreadId = CompletionThreadFuture.get();
    const ETaskStatus CompletionStatus = CompletionStatusFuture.get();

    CallerSystem.Stop();

    REQUIRE(CompletionThreadId == CallerSystem.OwnerThreadId);
    REQUIRE(CompletionStatus == ETaskStatus::Canceled);
    REQUIRE(WorkerOwner.ExecutedCount == 0);
}

TEST_CASE("EnqueueThreadTask executes on owner thread and nested enqueue is deferred", "[threading]")
{
    TSystemTaskQueue<ThreadingTestOwner> Queue;
    ThreadingTestOwner Owner{};
    GameMutex Mutex{};

    bool FirstRan = false;
    bool SecondRan = false;
    std::thread::id FirstThread{};
    std::thread::id SecondThread{};

    Queue.EnqueueThreadTask([&]() {
        FirstRan = true;
        FirstThread = std::this_thread::get_id();
        Queue.EnqueueThreadTask([&]() {
            SecondRan = true;
            SecondThread = std::this_thread::get_id();
        });
    });

    Queue.ExecuteQueuedTasks(Owner, Mutex);
    REQUIRE(FirstRan);
    REQUIRE_FALSE(SecondRan);

    Queue.ExecuteQueuedTasks(Owner, Mutex);
    REQUIRE(SecondRan);
    REQUIRE(FirstThread == std::this_thread::get_id());
    REQUIRE(SecondThread == std::this_thread::get_id());
}
