#include "ConduitPerfNative.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "GameFramework.hpp"

namespace SnAPI::GameFramework::Benchmarks
{

#if defined(_MSC_VER)
#define SNAPI_BENCH_NOINLINE __declspec(noinline)
#define SNAPI_BENCH_NOIPA
#elif defined(__GNUC__) && !defined(__clang__)
#define SNAPI_BENCH_NOINLINE __attribute__((noinline))
#define SNAPI_BENCH_NOIPA __attribute__((noipa))
#else
#define SNAPI_BENCH_NOINLINE __attribute__((noinline))
#define SNAPI_BENCH_NOIPA
#endif

namespace
{

template<typename T>
inline void DoNotOptimize(T& Value)
{
#if defined(__clang__)
    asm volatile("" : "+r,m"(Value) : : "memory");
#elif defined(__GNUC__)
    asm volatile("" : "+m,r"(Value) : : "memory");
#else
    (void)Value;
#endif
}

inline void ClobberMemory()
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : : "memory");
#endif
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA ConduitPerfHarness* ResolveHandleDirect(const HandleResolverState& State,
                                                                               const ConduitPerfHandle Handle)
{
    if (Handle.Id < 0)
    {
        return nullptr;
    }

    const std::size_t Index = static_cast<std::size_t>(Handle.Id);
    if (Index >= State.Targets.size())
    {
        return nullptr;
    }

    return State.Targets[Index];
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA void StepRealisticDirect(ConduitPerfHarness& Harness,
                                                                const int Delta,
                                                                const int Limit)
{
    const int Current = Harness.Health;
    const int Next = Current + Delta;
    Harness.Health = Next;
    if (Next < Limit)
    {
        Harness.AddHealth(Delta);
    }
}

} // namespace

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA void ConduitPerfHarness::AddHealth(const int Delta)
{
    Health += Delta;
}

void EnsureConduitPerfHarnessRegistered()
{
    RegisterBuiltinTypes();

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPerfHarness>()))
    {
        auto RegisterResult = TTypeBuilder<ConduitPerfHarness>(ConduitPerfHarness::kTypeName)
            .Field("Health", &ConduitPerfHarness::Health)
            .Method("AddHealth", &ConduitPerfHarness::AddHealth)
            .Constructor<>()
            .Register();
        if (!RegisterResult)
        {
            throw std::runtime_error(RegisterResult.error().Message);
        }
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPerfHandle>()))
    {
        auto RegisterResult = TTypeBuilder<ConduitPerfHandle>(ConduitPerfHandle::kTypeName)
            .Field("Id", &ConduitPerfHandle::Id)
            .Constructor<>()
            .Register();
        if (!RegisterResult)
        {
            throw std::runtime_error(RegisterResult.error().Message);
        }
    }
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA std::int64_t RunDirectMethodLoop(ConduitPerfHarness& Harness,
                                                                        const int* const Deltas,
                                                                        const std::size_t DeltaCount,
                                                                        const std::uint64_t Iterations)
{
    if (!Deltas || DeltaCount == 0)
    {
        return -1;
    }

    std::int64_t Checksum = 0;
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        const int Delta = Deltas[Index % DeltaCount];
        DoNotOptimize(Harness.Health);
        Harness.AddHealth(Delta);
        DoNotOptimize(Harness.Health);
        Checksum += Harness.Health;
        DoNotOptimize(Checksum);
    }
    ClobberMemory();
    return Checksum;
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA std::int64_t RunDirectFieldReadLoop(ConduitPerfHarness& Harness,
                                                                           const int* const Values,
                                                                           const std::size_t ValueCount,
                                                                           const std::uint64_t Iterations)
{
    if (!Values || ValueCount == 0)
    {
        return -1;
    }

    std::int64_t Accumulator = 0;
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        Harness.Health = Values[Index % ValueCount];
        ClobberMemory();
        const int Output = Harness.Health;
        Accumulator += Output;
        DoNotOptimize(Accumulator);
    }
    ClobberMemory();
    return Accumulator;
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA std::int64_t RunDirectFieldWriteLoop(ConduitPerfHarness& Harness,
                                                                            const int* const Values,
                                                                            const std::size_t ValueCount,
                                                                            const std::uint64_t Iterations)
{
    if (!Values || ValueCount == 0)
    {
        return -1;
    }

    std::int64_t Checksum = 0;
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        const int Value = Values[Index % ValueCount];
        Harness.Health = Value;
        ClobberMemory();
        DoNotOptimize(Harness.Health);
        Checksum += Harness.Health;
        DoNotOptimize(Checksum);
    }
    return Checksum;
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA std::int64_t RunDirectHandleMethodLoop(const HandleResolverState& State,
                                                                              const ConduitPerfHandle Handle,
                                                                              const int* const Deltas,
                                                                              const std::size_t DeltaCount,
                                                                              const std::uint64_t Iterations)
{
    if (!Deltas || DeltaCount == 0)
    {
        return -1;
    }

    std::int64_t Checksum = 0;
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        ConduitPerfHarness* Target = ResolveHandleDirect(State, Handle);
        if (!Target)
        {
            return -1;
        }
        const int Delta = Deltas[Index % DeltaCount];
        DoNotOptimize(Target);
        Target->AddHealth(Delta);
        DoNotOptimize(Target->Health);
        Checksum += Target->Health;
        DoNotOptimize(Checksum);
    }
    return Checksum;
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA std::int64_t RunDirectRealisticLoop(ConduitPerfHarness& Harness,
                                                                           const int* const Deltas,
                                                                           const std::size_t DeltaCount,
                                                                           const int Limit,
                                                                           const std::uint64_t Iterations)
{
    if (!Deltas || DeltaCount == 0)
    {
        return -1;
    }

    std::int64_t Checksum = 0;
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        const int Delta = Deltas[Index % DeltaCount];
        StepRealisticDirect(Harness, Delta, Limit);
        DoNotOptimize(Harness.Health);
        Checksum += Harness.Health;
        DoNotOptimize(Checksum);
    }
    ClobberMemory();
    return Checksum;
}

SNAPI_BENCH_NOINLINE SNAPI_BENCH_NOIPA std::int64_t RunDirectManyInstanceRealisticLoop(ConduitPerfHarness* const Harnesses,
                                                                                       const std::size_t Count,
                                                                                       const int* const Deltas,
                                                                                       const std::size_t DeltaCount,
                                                                                       const int Limit,
                                                                                       const std::uint64_t Passes)
{
    if (!Harnesses || !Deltas || DeltaCount == 0)
    {
        return -1;
    }

    std::int64_t Checksum = 0;
    for (std::uint64_t Pass = 0; Pass < Passes; ++Pass)
    {
        for (std::size_t Index = 0; Index < Count; ++Index)
        {
            const int Delta = Deltas[(Pass * Count + Index) % DeltaCount];
            StepRealisticDirect(Harnesses[Index], Delta, Limit);
            DoNotOptimize(Harnesses[Index].Health);
            Checksum += Harnesses[Index].Health;
            DoNotOptimize(Checksum);
        }
    }
    ClobberMemory();
    return Checksum;
}

} // namespace SnAPI::GameFramework::Benchmarks
