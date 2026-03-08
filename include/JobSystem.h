#pragma once

#include <cstdint>
#include <functional>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Minimal job-system facade for internal data-parallel work.
 *
 * This is currently a serial compatibility layer rather than a real scheduler. Its main purpose is to
 * give callers a stable API boundary that can later be backed by a true worker pool without rewriting
 * call sites.
 *
 * Current semantics:
 * - `ParallelFor()` executes synchronously on the calling thread.
 * - `WorkerCount` is configuration state only and does not affect execution yet.
 */
class JobSystem
{
public:
    /**
     * @brief Set the desired worker-count hint.
     * @param Count Number of worker threads.
     *
     * The current implementation stores the value only for future use.
     */
    void WorkerCount(uint32_t Count)
    {
        m_workerCount = Count;
    }

    /**
     * @brief Get the configured worker-count hint.
     * @return Worker-count hint value.
     */
    uint32_t WorkerCount() const
    {
        return m_workerCount;
    }

    /**
     * @brief Execute a parallel-for style workload.
     * @param Count Number of iterations.
     * @param Fn Function invoked per index.
     *
     * Current implementation:
     * - deterministic serial execution
     * - on the calling thread
     * - with one callback per index in `[0, Count)`
     */
    void ParallelFor(size_t Count, const std::function<void(size_t)>& Fn) const
    {
        for (size_t Index = 0; Index < Count; ++Index)
        {
            Fn(Index);
        }
    }

private:
    uint32_t m_workerCount = 0; /**< @brief Desired worker count (not yet used). */
};

} // namespace SnAPI::GameFramework
