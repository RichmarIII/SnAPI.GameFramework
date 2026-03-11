#pragma once

#include <cstddef>
#include <cstdint>

#include "ConduitPerfShared.h"

namespace SnAPI::GameFramework::Benchmarks
{

void EnsureConduitPerfHarnessRegistered();

std::int64_t RunDirectMethodLoop(ConduitPerfHarness& Harness,
                                 const int* Deltas,
                                 std::size_t DeltaCount,
                                 std::uint64_t Iterations);
std::int64_t RunDirectFieldReadLoop(ConduitPerfHarness& Harness,
                                    const int* Values,
                                    std::size_t ValueCount,
                                    std::uint64_t Iterations);
std::int64_t RunDirectFieldWriteLoop(ConduitPerfHarness& Harness,
                                     const int* Values,
                                     std::size_t ValueCount,
                                     std::uint64_t Iterations);
std::int64_t RunDirectHandleMethodLoop(const HandleResolverState& State,
                                       ConduitPerfHandle Handle,
                                       const int* Deltas,
                                       std::size_t DeltaCount,
                                       std::uint64_t Iterations);
std::int64_t RunDirectRealisticLoop(ConduitPerfHarness& Harness,
                                    const int* Deltas,
                                    std::size_t DeltaCount,
                                    int Limit,
                                    std::uint64_t Iterations);
std::int64_t RunDirectManyInstanceRealisticLoop(ConduitPerfHarness* Harnesses,
                                                std::size_t Count,
                                                const int* Deltas,
                                                std::size_t DeltaCount,
                                                int Limit,
                                                std::uint64_t Passes);

} // namespace SnAPI::GameFramework::Benchmarks
