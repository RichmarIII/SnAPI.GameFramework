#pragma once

#include <cstdint>
#include <functional>

namespace SnAPI::GameFramework
{

class JobSystem
{
public:
    void WorkerCount(uint32_t Count)
    {
        m_workerCount = Count;
    }

    uint32_t WorkerCount() const
    {
        return m_workerCount;
    }

    void ParallelFor(size_t Count, const std::function<void(size_t)>& Fn) const
    {
        for (size_t Index = 0; Index < Count; ++Index)
        {
            Fn(Index);
        }
    }

private:
    uint32_t m_workerCount = 0;
};

} // namespace SnAPI::GameFramework
