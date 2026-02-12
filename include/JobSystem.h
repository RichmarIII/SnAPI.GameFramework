#pragma once

#include <cstdint>
#include <functional>

namespace SnAPI::GameFramework
{

/**
 * @brief Minimal job system facade for internal parallelism.
 * @remarks Currently single-threaded; ParallelFor executes serially.
 * @note Designed as a stable integration seam for a future true task scheduler.
 */
class JobSystem
{
public:
    /**
     * @brief Set the desired worker count.
     * @param Count Number of worker threads.
     * @remarks Currently stored for future use.
     */
    void WorkerCount(uint32_t Count)
    {
        m_workerCount = Count;
    }

    /**
     * @brief Get the configured worker count.
     * @return Worker count value.
     */
    uint32_t WorkerCount() const
    {
        return m_workerCount;
    }

    /**
     * @brief Execute a parallel-for workload.
     * @param Count Number of iterations.
     * @param Fn Function invoked per index.
     * @remarks Current implementation is deterministic serial execution on calling thread.
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
