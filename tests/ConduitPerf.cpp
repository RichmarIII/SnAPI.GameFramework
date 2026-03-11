#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "GameFramework.hpp"
#include "ConduitPerfNative.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Conduit;
using namespace SnAPI::GameFramework::Benchmarks;

namespace
{

#ifndef SNAPI_BENCH_BUILD_LABEL
#define SNAPI_BENCH_BUILD_LABEL "Unknown"
#endif

struct BenchmarkStats
{
    double BestMs = 0.0;
    double AverageMs = 0.0;
    double MedianMs = 0.0;
};

struct MethodGraphCase
{
    CompiledGraph Graph{};
    SlotId DeltaSlot{};
};

struct FieldReadGraphCase
{
    CompiledGraph Graph{};
    SlotId OutputSlot{};
};

struct FieldWriteGraphCase
{
    CompiledGraph Graph{};
    SlotId InputSlot{};
};

struct HandleMethodGraphCase
{
    CompiledGraph Graph{};
    SlotId HandleSlot{};
    SlotId DeltaSlot{};
};

struct RealisticGraphCase
{
    CompiledGraph Graph{};
    SlotId DeltaSlot{};
};

constexpr std::size_t kWarmupIterations = 20'000;
constexpr std::size_t kSampleCount = 7;
constexpr std::size_t kInputPatternSize = 4096;
constexpr std::size_t kManyInstanceCount = 10'000;
constexpr int kRealisticLimit = 1'000'000'000;

struct LoopExpectation
{
    std::int64_t Checksum = 0;
    int FinalHealth = 0;
};

struct ManyLoopExpectation
{
    std::int64_t Checksum = 0;
    std::vector<int> FinalHealths{};
};

std::vector<int> BuildInputPattern(const std::size_t Count,
                                   std::uint32_t Seed,
                                   const int MinValue,
                                   const int MaxValue)
{
    if (Count == 0 || MinValue > MaxValue)
    {
        throw std::invalid_argument("BuildInputPattern received invalid bounds");
    }

    std::vector<int> Values(Count);
    const std::uint32_t Range = static_cast<std::uint32_t>(MaxValue - MinValue + 1);
    std::uint32_t State = Seed;
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        State = (State * 1664525u) + 1013904223u;
        Values[Index] = MinValue + static_cast<int>(State % Range);
    }
    return Values;
}

LoopExpectation SimulateMethodLoop(const std::vector<int>& Deltas, const std::uint64_t Iterations)
{
    LoopExpectation Result{};
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        Result.FinalHealth += Deltas[Index % Deltas.size()];
        Result.Checksum += Result.FinalHealth;
    }
    return Result;
}

LoopExpectation SimulateFieldReadLoop(const std::vector<int>& Values, const std::uint64_t Iterations)
{
    LoopExpectation Result{};
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        Result.FinalHealth = Values[Index % Values.size()];
        Result.Checksum += Result.FinalHealth;
    }
    return Result;
}

LoopExpectation SimulateFieldWriteLoop(const std::vector<int>& Values, const std::uint64_t Iterations)
{
    return SimulateFieldReadLoop(Values, Iterations);
}

LoopExpectation SimulateRealisticLoop(const std::vector<int>& Deltas,
                                      const int Limit,
                                      const std::uint64_t Iterations)
{
    LoopExpectation Result{};
    for (std::uint64_t Index = 0; Index < Iterations; ++Index)
    {
        const int Delta = Deltas[Index % Deltas.size()];
        const int Next = Result.FinalHealth + Delta;
        Result.FinalHealth = Next;
        if (Next < Limit)
        {
            Result.FinalHealth += Delta;
        }
        Result.Checksum += Result.FinalHealth;
    }
    return Result;
}

ManyLoopExpectation SimulateManyInstanceRealisticLoop(const std::vector<int>& Deltas,
                                                      const int Limit,
                                                      const std::size_t Count,
                                                      const std::uint64_t Passes)
{
    ManyLoopExpectation Result{};
    Result.FinalHealths.assign(Count, 0);
    for (std::uint64_t Pass = 0; Pass < Passes; ++Pass)
    {
        for (std::size_t Index = 0; Index < Count; ++Index)
        {
            const int Delta = Deltas[(Pass * Count + Index) % Deltas.size()];
            int& Health = Result.FinalHealths[Index];
            const int Next = Health + Delta;
            Health = Next;
            if (Next < Limit)
            {
                Health += Delta;
            }
            Result.Checksum += Health;
        }
    }
    return Result;
}

void StoreIntOrThrow(GraphInstance& Instance, const SlotId Slot, const int Value, const char* const Message)
{
    const Result StoreResult = Instance.Frame().StoreCopy(Slot, &Value);
    if (!StoreResult)
    {
        throw std::runtime_error(std::string(Message) + ": " + StoreResult.error().Message);
    }
}

TExpected<ResolvedTarget> ResolveConduitPerfHandle(const void* UserData,
                                                   const TypeInfo& ExpectedType,
                                                   const TypeInfo& HandleType,
                                                   const void* HandleValue)
{
    if (!UserData || !HandleValue)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "ConduitPerf handle resolver received null input"));
    }
    if (HandleType.Id != StaticTypeId<ConduitPerfHandle>())
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch,
                                         "ConduitPerf handle resolver received the wrong handle type"));
    }

    const auto* State = static_cast<const HandleResolverState*>(UserData);
    const auto* Handle = static_cast<const ConduitPerfHandle*>(HandleValue);
    if (Handle->Id < 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "ConduitPerf handle id is negative"));
    }

    const std::size_t Index = static_cast<std::size_t>(Handle->Id);
    if (Index >= State->Targets.size() || !State->Targets[Index])
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "ConduitPerf handle target was not found"));
    }
    if (State->HarnessType && !TypeRegistry::Instance().IsA(State->HarnessType->Id, ExpectedType.Id))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch,
                                         "ConduitPerf resolved target type does not satisfy the requested type"));
    }

    return ResolvedTarget{
        .Instance = State->Targets[Index],
        .Type = State->HarnessType,
    };
}

void Require(const Result& Value, const char* const Message)
{
    if (!Value)
    {
        throw std::runtime_error(std::string(Message) + ": " + Value.error().Message);
    }
}

template<typename T>
T TakeExpected(TExpected<T>&& Value, const char* const Message)
{
    if (!Value)
    {
        throw std::runtime_error(std::string(Message) + ": " + Value.error().Message);
    }
    return std::move(*Value);
}

MethodGraphCase BuildMethodGraph(const TypeInfo& SelfType)
{
    GraphBuilder Builder(SelfType);
    const SlotId DeltaSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()), "AddSlot(int) failed for method graph");
    const SlotId Inputs[] = {DeltaSlot};
    Require(Builder.AddSelfMethodCall("AddHealth", Inputs), "AddSelfMethodCall(AddHealth) failed");
    return MethodGraphCase{
        .Graph = TakeExpected(std::move(Builder).Build(), "Build() failed for method graph"),
        .DeltaSlot = DeltaSlot,
    };
}

FieldReadGraphCase BuildFieldReadGraph(const TypeInfo& SelfType)
{
    GraphBuilder Builder(SelfType);
    const SlotId OutputSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()), "AddSlot(int) failed for field-read graph");
    Require(Builder.AddSelfFieldRead("Health", OutputSlot), "AddSelfFieldRead(Health) failed");
    return FieldReadGraphCase{
        .Graph = TakeExpected(std::move(Builder).Build(), "Build() failed for field-read graph"),
        .OutputSlot = OutputSlot,
    };
}

FieldWriteGraphCase BuildFieldWriteGraph(const TypeInfo& SelfType)
{
    GraphBuilder Builder(SelfType);
    const SlotId InputSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()), "AddSlot(int) failed for field-write graph");
    Require(Builder.AddSelfFieldWrite("Health", InputSlot), "AddSelfFieldWrite(Health) failed");
    return FieldWriteGraphCase{
        .Graph = TakeExpected(std::move(Builder).Build(), "Build() failed for field-write graph"),
        .InputSlot = InputSlot,
    };
}

HandleMethodGraphCase BuildHandleMethodGraph(const TypeInfo& SelfType)
{
    GraphBuilder Builder(SelfType);
    const SlotId HandleSlot = TakeExpected(Builder.AddSlot(StaticTypeId<ConduitPerfHandle>(), ESlotKind::Handle),
                                           "AddSlot(handle) failed for handle-method graph");
    const SlotId DeltaSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()), "AddSlot(int) failed for handle-method graph");
    const SlotId Inputs[] = {DeltaSlot};
    Require(Builder.AddMethodCall(SelfType, HandleSlot, "AddHealth", Inputs),
            "AddMethodCall(AddHealth) failed for handle-method graph");
    return HandleMethodGraphCase{
        .Graph = TakeExpected(std::move(Builder).Build(), "Build() failed for handle-method graph"),
        .HandleSlot = HandleSlot,
        .DeltaSlot = DeltaSlot,
    };
}

RealisticGraphCase BuildRealisticGraph(const TypeInfo& SelfType)
{
    GraphBuilder Builder(SelfType);
    const SlotId CurrentHealthSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()),
                                                  "AddSlot(CurrentHealth) failed for realistic graph");
    const SlotId DeltaSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()),
                                          "AddSlot(Delta) failed for realistic graph");
    const SlotId LimitSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()),
                                          "AddSlot(Limit) failed for realistic graph");
    const SlotId NextHealthSlot = TakeExpected(Builder.AddSlot(StaticTypeId<int>()),
                                               "AddSlot(NextHealth) failed for realistic graph");
    const SlotId ConditionSlot = TakeExpected(Builder.AddSlot(StaticTypeId<bool>()),
                                              "AddSlot(Condition) failed for realistic graph");

    const LabelId ContinueLabel = Builder.CreateLabel();
    const LabelId EndLabel = Builder.CreateLabel();

    Require(Builder.AddConstant(LimitSlot, Variant::FromValue(kRealisticLimit)),
            "AddConstant(Limit) failed for realistic graph");
    Require(Builder.AddSelfFieldRead("Health", CurrentHealthSlot), "AddSelfFieldRead(Health) failed for realistic graph");
    Require(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Add, CurrentHealthSlot, DeltaSlot, NextHealthSlot),
            "AddBinaryIntrinsic(Add) failed for realistic graph");
    Require(Builder.AddSelfFieldWrite("Health", NextHealthSlot), "AddSelfFieldWrite(Health) failed for realistic graph");
    Require(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Less, NextHealthSlot, LimitSlot, ConditionSlot),
            "AddBinaryIntrinsic(Less) failed for realistic graph");
    Require(Builder.AddBranch(ConditionSlot, ContinueLabel, EndLabel), "AddBranch() failed for realistic graph");
    Require(Builder.MarkLabel(ContinueLabel), "MarkLabel(Continue) failed for realistic graph");
    {
        const SlotId Inputs[] = {DeltaSlot};
        Require(Builder.AddSelfMethodCall("AddHealth", Inputs), "AddSelfMethodCall(AddHealth) failed for realistic graph");
    }
    Require(Builder.MarkLabel(EndLabel), "MarkLabel(End) failed for realistic graph");

    return RealisticGraphCase{
        .Graph = TakeExpected(std::move(Builder).Build(), "Build() failed for realistic graph"),
        .DeltaSlot = DeltaSlot,
    };
}

template<typename TSetup, typename TFn>
BenchmarkStats RunSamplesWithSetup(const std::size_t Samples, TSetup&& Setup, TFn&& Fn)
{
    std::vector<double> DurationsMs{};
    DurationsMs.reserve(Samples);

    for (std::size_t Index = 0; Index < Samples; ++Index)
    {
        Setup();
        const auto Start = std::chrono::steady_clock::now();
        Fn();
        const auto End = std::chrono::steady_clock::now();
        DurationsMs.push_back(std::chrono::duration<double, std::milli>(End - Start).count());
    }

    std::vector<double> Sorted = DurationsMs;
    std::sort(Sorted.begin(), Sorted.end());
    const double Average = std::accumulate(DurationsMs.begin(), DurationsMs.end(), 0.0) /
                           static_cast<double>(DurationsMs.size());

    return BenchmarkStats{
        .BestMs = Sorted.front(),
        .AverageMs = Average,
        .MedianMs = Sorted[Sorted.size() / 2],
    };
}

void PrintStats(const std::string& Label, const BenchmarkStats& Stats, const std::uint64_t Iterations)
{
    const double BestNsPerCall = (Stats.BestMs * 1'000'000.0) / static_cast<double>(Iterations);
    const double AverageNsPerCall = (Stats.AverageMs * 1'000'000.0) / static_cast<double>(Iterations);
    const double MedianNsPerCall = (Stats.MedianMs * 1'000'000.0) / static_cast<double>(Iterations);

    std::cout << std::left << std::setw(18) << Label
              << " best=" << std::fixed << std::setprecision(3) << Stats.BestMs << " ms"
              << " avg=" << Stats.AverageMs << " ms"
              << " median=" << Stats.MedianMs << " ms"
              << " best/call=" << std::setprecision(2) << BestNsPerCall << " ns"
              << " avg/call=" << AverageNsPerCall << " ns"
              << " median/call=" << MedianNsPerCall << " ns"
              << '\n';
}

void PrintComparison(const std::string& Title,
                     const BenchmarkStats& DirectStats,
                     const BenchmarkStats& ConduitStats,
                     const std::uint64_t Iterations)
{
    std::cout << Title << '\n';
    PrintStats("Direct C++", DirectStats, Iterations);
    PrintStats("Conduit", ConduitStats, Iterations);
    std::cout << "Slowdown          best=" << std::fixed << std::setprecision(2)
              << (ConduitStats.BestMs / DirectStats.BestMs) << "x"
              << " avg=" << (ConduitStats.AverageMs / DirectStats.AverageMs) << "x"
              << " median=" << (ConduitStats.MedianMs / DirectStats.MedianMs) << "x"
              << "\n\n";
}

} // namespace

int main(const int argc, char** argv)
{
    try
    {
        EnsureConduitPerfHarnessRegistered();

        std::uint64_t Iterations = 1'000'000;
        if (argc > 1)
        {
            Iterations = std::strtoull(argv[1], nullptr, 10);
        }

        const std::uint64_t ManyInstancePasses = std::max<std::uint64_t>(1, Iterations / kManyInstanceCount);
        const std::uint64_t ManyInstanceExecutions = kManyInstanceCount * ManyInstancePasses;
        const std::vector<int> DeltaPattern =
            BuildInputPattern(kInputPatternSize, static_cast<std::uint32_t>(Iterations ^ 0x13579BDFu), 1, 7);
        const std::vector<int> ReadPattern =
            BuildInputPattern(kInputPatternSize, static_cast<std::uint32_t>(Iterations ^ 0x2468ACE1u), 11, 127);
        const std::vector<int> WritePattern =
            BuildInputPattern(kInputPatternSize, static_cast<std::uint32_t>(Iterations ^ 0x0F1E2D3Cu), 5, 191);

        const LoopExpectation MethodExpected = SimulateMethodLoop(DeltaPattern, Iterations);
        const LoopExpectation FieldReadExpected = SimulateFieldReadLoop(ReadPattern, Iterations);
        const LoopExpectation FieldWriteExpected = SimulateFieldWriteLoop(WritePattern, Iterations);
        const LoopExpectation HandleMethodExpected = SimulateMethodLoop(DeltaPattern, Iterations);
        const LoopExpectation RealisticExpected = SimulateRealisticLoop(DeltaPattern, kRealisticLimit, Iterations);
        const ManyLoopExpectation ManyExpected =
            SimulateManyInstanceRealisticLoop(DeltaPattern, kRealisticLimit, kManyInstanceCount, ManyInstancePasses);

        const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitPerfHarness>());
        if (!SelfType)
        {
            std::cerr << "Failed to resolve reflected Conduit benchmark harness type\n";
            return 1;
        }

        const MethodGraphCase MethodCase = BuildMethodGraph(*SelfType);
        const FieldReadGraphCase FieldReadCase = BuildFieldReadGraph(*SelfType);
        const FieldWriteGraphCase FieldWriteCase = BuildFieldWriteGraph(*SelfType);
        const HandleMethodGraphCase HandleMethodCase = BuildHandleMethodGraph(*SelfType);
        const RealisticGraphCase RealisticCase = BuildRealisticGraph(*SelfType);

        GraphInstance MethodInstance(MethodCase.Graph);
        GraphInstance FieldReadInstance(FieldReadCase.Graph);
        GraphInstance FieldWriteInstance(FieldWriteCase.Graph);

        GraphInstance HandleMethodInstance(HandleMethodCase.Graph);
        const ConduitPerfHandle HandleValue{.Id = 0};
        Require(HandleMethodInstance.Frame().StoreCopy(HandleMethodCase.HandleSlot, &HandleValue),
                "StoreCopy(Handle method handle) failed");

        GraphInstance RealisticInstance(RealisticCase.Graph);

        ConduitPerfHarness MethodHarness{};
        ConduitPerfHarness FieldReadHarness{};
        ConduitPerfHarness FieldWriteHarness{};
        ConduitPerfHarness HandleHarness{};
        ConduitPerfHarness RealisticHarness{};

        HandleResolverState ResolverState{
            .HarnessType = SelfType,
            .Targets = {&HandleHarness},
        };

        const ExecutionContext MethodContext{
            .Self = &MethodHarness,
            .SelfType = SelfType,
        };
        const ExecutionContext FieldReadContext{
            .Self = &FieldReadHarness,
            .SelfType = SelfType,
        };
        const ExecutionContext FieldWriteContext{
            .Self = &FieldWriteHarness,
            .SelfType = SelfType,
        };
        const ExecutionContext HandleContext{
            .ResolveHandle = &ResolveConduitPerfHandle,
            .HandleResolverUserData = &ResolverState,
        };
        const ExecutionContext RealisticContext{
            .Self = &RealisticHarness,
            .SelfType = SelfType,
        };

        for (std::size_t Index = 0; Index < kWarmupIterations; ++Index)
        {
            const int Delta = DeltaPattern[Index % DeltaPattern.size()];
            MethodHarness.AddHealth(Delta);
            StoreIntOrThrow(MethodInstance, MethodCase.DeltaSlot, Delta, "StoreCopy(Method delta) failed during warmup");
            const Result MethodResult = MethodInstance.Execute(MethodContext);
            if (!MethodResult)
            {
                throw std::runtime_error(MethodResult.error().Message);
            }

            FieldReadHarness.Health = ReadPattern[Index % ReadPattern.size()];
            const Result FieldReadResult = FieldReadInstance.Execute(FieldReadContext);
            if (!FieldReadResult)
            {
                throw std::runtime_error(FieldReadResult.error().Message);
            }

            StoreIntOrThrow(FieldWriteInstance,
                            FieldWriteCase.InputSlot,
                            WritePattern[Index % WritePattern.size()],
                            "StoreCopy(Field write input) failed during warmup");
            const Result FieldWriteResult = FieldWriteInstance.Execute(FieldWriteContext);
            if (!FieldWriteResult)
            {
                throw std::runtime_error(FieldWriteResult.error().Message);
            }

            StoreIntOrThrow(HandleMethodInstance,
                            HandleMethodCase.DeltaSlot,
                            Delta,
                            "StoreCopy(Handle method delta) failed during warmup");
            const Result HandleResult = HandleMethodInstance.Execute(HandleContext);
            if (!HandleResult)
            {
                throw std::runtime_error(HandleResult.error().Message);
            }

            StoreIntOrThrow(RealisticInstance,
                            RealisticCase.DeltaSlot,
                            Delta,
                            "StoreCopy(Realistic delta) failed during warmup");
            const Result RealisticResult = RealisticInstance.Execute(RealisticContext);
            if (!RealisticResult)
            {
                throw std::runtime_error(RealisticResult.error().Message);
            }
        }

        const BenchmarkStats MethodDirect = RunSamplesWithSetup(kSampleCount,
                                                                [&MethodHarness] { MethodHarness.Health = 0; },
                                                                [&MethodHarness, &DeltaPattern, &MethodExpected, Iterations] {
                                                                    const std::int64_t Checksum =
                                                                        RunDirectMethodLoop(MethodHarness,
                                                                                            DeltaPattern.data(),
                                                                                            DeltaPattern.size(),
                                                                                            Iterations);
                                                                    if (Checksum != MethodExpected.Checksum ||
                                                                        MethodHarness.Health != MethodExpected.FinalHealth)
                                                                    {
                                                                        throw std::runtime_error("Method direct benchmark produced an unexpected result");
                                                                    }
                                                                });
        const BenchmarkStats MethodConduit = RunSamplesWithSetup(kSampleCount,
                                                                 [&MethodHarness] { MethodHarness.Health = 0; },
                                                                 [&MethodHarness, &MethodInstance, MethodContext, &MethodCase, &DeltaPattern, &MethodExpected, Iterations] {
                                                                     std::int64_t Checksum = 0;
                                                                     for (std::uint64_t Index = 0; Index < Iterations; ++Index)
                                                                     {
                                                                         const int Delta = DeltaPattern[Index % DeltaPattern.size()];
                                                                         StoreIntOrThrow(MethodInstance,
                                                                                         MethodCase.DeltaSlot,
                                                                                         Delta,
                                                                                         "StoreCopy(Method delta) failed");
                                                                         const Result ExecuteResult = MethodInstance.Execute(MethodContext);
                                                                         if (!ExecuteResult)
                                                                         {
                                                                             throw std::runtime_error(ExecuteResult.error().Message);
                                                                         }
                                                                         Checksum += MethodHarness.Health;
                                                                     }
                                                                     if (Checksum != MethodExpected.Checksum ||
                                                                         MethodHarness.Health != MethodExpected.FinalHealth)
                                                                     {
                                                                         throw std::runtime_error("Method Conduit benchmark produced an unexpected result");
                                                                     }
                                                                 });

        const BenchmarkStats FieldReadDirect = RunSamplesWithSetup(kSampleCount,
                                                                   [&FieldReadHarness] { FieldReadHarness.Health = 0; },
                                                                   [&FieldReadHarness, &ReadPattern, &FieldReadExpected, Iterations] {
                                                                       const std::int64_t Checksum =
                                                                           RunDirectFieldReadLoop(FieldReadHarness,
                                                                                                  ReadPattern.data(),
                                                                                                  ReadPattern.size(),
                                                                                                  Iterations);
                                                                       if (Checksum != FieldReadExpected.Checksum ||
                                                                           FieldReadHarness.Health != FieldReadExpected.FinalHealth)
                                                                       {
                                                                           throw std::runtime_error("Field-read direct benchmark produced an unexpected result");
                                                                       }
                                                                   });
        const BenchmarkStats FieldReadConduit = RunSamplesWithSetup(kSampleCount,
                                                                    [&FieldReadHarness] { FieldReadHarness.Health = 0; },
                                                                    [&FieldReadHarness, &FieldReadInstance, FieldReadContext, &FieldReadCase, &ReadPattern, &FieldReadExpected, Iterations] {
                                                                        std::int64_t Checksum = 0;
                                                                        for (std::uint64_t Index = 0; Index < Iterations; ++Index)
                                                                        {
                                                                            FieldReadHarness.Health = ReadPattern[Index % ReadPattern.size()];
                                                                            const Result ExecuteResult = FieldReadInstance.Execute(FieldReadContext);
                                                                            if (!ExecuteResult)
                                                                            {
                                                                                throw std::runtime_error(ExecuteResult.error().Message);
                                                                            }
                                                                            auto Output = FieldReadInstance.Frame().AsConstRef<int>(FieldReadCase.OutputSlot);
                                                                            if (!Output)
                                                                            {
                                                                                throw std::runtime_error("Field-read Conduit benchmark failed to read output slot");
                                                                            }
                                                                            Checksum += Output->get();
                                                                        }
                                                                        if (Checksum != FieldReadExpected.Checksum ||
                                                                            FieldReadHarness.Health != FieldReadExpected.FinalHealth)
                                                                        {
                                                                            throw std::runtime_error("Field-read Conduit benchmark produced an unexpected result");
                                                                        }
                                                                    });

        const BenchmarkStats FieldWriteDirect = RunSamplesWithSetup(kSampleCount,
                                                                    [&FieldWriteHarness] { FieldWriteHarness.Health = 0; },
                                                                    [&FieldWriteHarness, &WritePattern, &FieldWriteExpected, Iterations] {
                                                                        const std::int64_t Checksum =
                                                                            RunDirectFieldWriteLoop(FieldWriteHarness,
                                                                                                    WritePattern.data(),
                                                                                                    WritePattern.size(),
                                                                                                    Iterations);
                                                                        if (Checksum != FieldWriteExpected.Checksum ||
                                                                            FieldWriteHarness.Health != FieldWriteExpected.FinalHealth)
                                                                        {
                                                                            throw std::runtime_error("Field-write direct benchmark produced an unexpected result");
                                                                        }
                                                                    });
        const BenchmarkStats FieldWriteConduit = RunSamplesWithSetup(kSampleCount,
                                                                     [&FieldWriteHarness] { FieldWriteHarness.Health = 0; },
                                                                     [&FieldWriteHarness, &FieldWriteInstance, FieldWriteContext, &FieldWriteCase, &WritePattern, &FieldWriteExpected, Iterations] {
                                                                         std::int64_t Checksum = 0;
                                                                         for (std::uint64_t Index = 0; Index < Iterations; ++Index)
                                                                         {
                                                                             StoreIntOrThrow(FieldWriteInstance,
                                                                                             FieldWriteCase.InputSlot,
                                                                                             WritePattern[Index % WritePattern.size()],
                                                                                             "StoreCopy(Field write input) failed");
                                                                             const Result ExecuteResult = FieldWriteInstance.Execute(FieldWriteContext);
                                                                             if (!ExecuteResult)
                                                                             {
                                                                                 throw std::runtime_error(ExecuteResult.error().Message);
                                                                             }
                                                                             Checksum += FieldWriteHarness.Health;
                                                                         }
                                                                         if (Checksum != FieldWriteExpected.Checksum ||
                                                                             FieldWriteHarness.Health != FieldWriteExpected.FinalHealth)
                                                                         {
                                                                             throw std::runtime_error("Field-write Conduit benchmark produced an unexpected result");
                                                                         }
                                                                     });

        const BenchmarkStats HandleMethodDirect = RunSamplesWithSetup(kSampleCount,
                                                                      [&HandleHarness] { HandleHarness.Health = 0; },
                                                                      [&HandleHarness, &ResolverState, &DeltaPattern, &HandleMethodExpected, Iterations, HandleValue] {
                                                                          const std::int64_t Checksum =
                                                                              RunDirectHandleMethodLoop(ResolverState,
                                                                                                        HandleValue,
                                                                                                        DeltaPattern.data(),
                                                                                                        DeltaPattern.size(),
                                                                                                        Iterations);
                                                                          if (Checksum != HandleMethodExpected.Checksum ||
                                                                              HandleHarness.Health != HandleMethodExpected.FinalHealth)
                                                                          {
                                                                              throw std::runtime_error("Handle-method direct benchmark produced an unexpected result");
                                                                          }
                                                                      });
        const BenchmarkStats HandleMethodConduit = RunSamplesWithSetup(kSampleCount,
                                                                       [&HandleHarness] { HandleHarness.Health = 0; },
                                                                       [&HandleHarness, &HandleMethodInstance, HandleContext, &HandleMethodCase, &DeltaPattern, &HandleMethodExpected, Iterations] {
                                                                           std::int64_t Checksum = 0;
                                                                           for (std::uint64_t Index = 0; Index < Iterations; ++Index)
                                                                           {
                                                                               StoreIntOrThrow(HandleMethodInstance,
                                                                                               HandleMethodCase.DeltaSlot,
                                                                                               DeltaPattern[Index % DeltaPattern.size()],
                                                                                               "StoreCopy(Handle method delta) failed");
                                                                               const Result ExecuteResult = HandleMethodInstance.Execute(HandleContext);
                                                                               if (!ExecuteResult)
                                                                               {
                                                                                   throw std::runtime_error(ExecuteResult.error().Message);
                                                                               }
                                                                               Checksum += HandleHarness.Health;
                                                                           }
                                                                           if (Checksum != HandleMethodExpected.Checksum ||
                                                                               HandleHarness.Health != HandleMethodExpected.FinalHealth)
                                                                           {
                                                                               throw std::runtime_error("Handle-method Conduit benchmark produced an unexpected result");
                                                                           }
                                                                       });

        const BenchmarkStats RealisticDirect = RunSamplesWithSetup(kSampleCount,
                                                                   [&RealisticHarness] { RealisticHarness.Health = 0; },
                                                                   [&RealisticHarness, &DeltaPattern, &RealisticExpected, Iterations] {
                                                                       const std::int64_t Checksum =
                                                                           RunDirectRealisticLoop(RealisticHarness,
                                                                                                  DeltaPattern.data(),
                                                                                                  DeltaPattern.size(),
                                                                                                  kRealisticLimit,
                                                                                                  Iterations);
                                                                       if (Checksum != RealisticExpected.Checksum ||
                                                                           RealisticHarness.Health != RealisticExpected.FinalHealth)
                                                                       {
                                                                           throw std::runtime_error("Realistic direct benchmark produced an unexpected result");
                                                                       }
                                                                   });
        const BenchmarkStats RealisticConduit = RunSamplesWithSetup(kSampleCount,
                                                                    [&RealisticHarness] { RealisticHarness.Health = 0; },
                                                                    [&RealisticHarness, &RealisticInstance, RealisticContext, &RealisticCase, &DeltaPattern, &RealisticExpected, Iterations] {
                                                                        std::int64_t Checksum = 0;
                                                                        for (std::uint64_t Index = 0; Index < Iterations; ++Index)
                                                                        {
                                                                            StoreIntOrThrow(RealisticInstance,
                                                                                            RealisticCase.DeltaSlot,
                                                                                            DeltaPattern[Index % DeltaPattern.size()],
                                                                                            "StoreCopy(Realistic delta) failed");
                                                                            const Result ExecuteResult = RealisticInstance.Execute(RealisticContext);
                                                                            if (!ExecuteResult)
                                                                            {
                                                                                throw std::runtime_error(ExecuteResult.error().Message);
                                                                            }
                                                                            Checksum += RealisticHarness.Health;
                                                                        }
                                                                        if (Checksum != RealisticExpected.Checksum ||
                                                                            RealisticHarness.Health != RealisticExpected.FinalHealth)
                                                                        {
                                                                            throw std::runtime_error("Realistic Conduit benchmark produced an unexpected result");
                                                                        }
                                                                    });

        std::vector<ConduitPerfHarness> ManyHarnesses(kManyInstanceCount);
        std::vector<ExecutionContext> ManyContexts{};
        ManyContexts.reserve(kManyInstanceCount);
        std::deque<GraphInstance> ManyInstances{};
        for (std::size_t Index = 0; Index < kManyInstanceCount; ++Index)
        {
            ManyInstances.emplace_back(RealisticCase.Graph);
            ManyContexts.push_back(ExecutionContext{
                .Self = &ManyHarnesses[Index],
                .SelfType = SelfType,
            });
        }

        const BenchmarkStats ManyDirect = RunSamplesWithSetup(kSampleCount,
                                                              [&ManyHarnesses] {
                                                                  for (ConduitPerfHarness& Harness : ManyHarnesses)
                                                                  {
                                                                      Harness.Health = 0;
                                                                  }
                                                              },
                                                              [&ManyHarnesses, &DeltaPattern, &ManyExpected, ManyInstancePasses] {
                                                                  const std::int64_t Result = RunDirectManyInstanceRealisticLoop(ManyHarnesses.data(),
                                                                                                                                 ManyHarnesses.size(),
                                                                                                                                 DeltaPattern.data(),
                                                                                                                                 DeltaPattern.size(),
                                                                                                                                 kRealisticLimit,
                                                                                                                                 ManyInstancePasses);
                                                                  if (Result != ManyExpected.Checksum)
                                                                  {
                                                                      throw std::runtime_error("Many-instance direct benchmark produced an unexpected aggregate result");
                                                                  }
                                                                  for (std::size_t Index = 0; Index < ManyHarnesses.size(); ++Index)
                                                                  {
                                                                      if (ManyHarnesses[Index].Health != ManyExpected.FinalHealths[Index])
                                                                      {
                                                                          throw std::runtime_error("Many-instance direct benchmark produced an unexpected result");
                                                                      }
                                                                  }
                                                              });
        const BenchmarkStats ManyConduit = RunSamplesWithSetup(kSampleCount,
                                                               [&ManyHarnesses] {
                                                                   for (ConduitPerfHarness& Harness : ManyHarnesses)
                                                                   {
                                                                       Harness.Health = 0;
                                                                   }
                                                               },
                                                               [&ManyInstances, &ManyContexts, &ManyHarnesses, &RealisticCase, &DeltaPattern, &ManyExpected, ManyInstancePasses] {
                                                                   std::int64_t Checksum = 0;
                                                                   for (std::uint64_t Pass = 0; Pass < ManyInstancePasses; ++Pass)
                                                                   {
                                                                       for (std::size_t Index = 0; Index < ManyInstances.size(); ++Index)
                                                                       {
                                                                           StoreIntOrThrow(ManyInstances[Index],
                                                                                           RealisticCase.DeltaSlot,
                                                                                           DeltaPattern[(Pass * ManyInstances.size() + Index) % DeltaPattern.size()],
                                                                                           "StoreCopy(Many-instance realistic delta) failed");
                                                                           const Result ExecuteResult = ManyInstances[Index].Execute(ManyContexts[Index]);
                                                                           if (!ExecuteResult)
                                                                           {
                                                                               throw std::runtime_error(ExecuteResult.error().Message);
                                                                           }
                                                                           Checksum += ManyHarnesses[Index].Health;
                                                                       }
                                                                   }
                                                                   if (Checksum != ManyExpected.Checksum)
                                                                   {
                                                                       throw std::runtime_error("Many-instance Conduit benchmark produced an unexpected aggregate result");
                                                                   }
                                                                   for (std::size_t Index = 0; Index < ManyHarnesses.size(); ++Index)
                                                                   {
                                                                       if (ManyHarnesses[Index].Health != ManyExpected.FinalHealths[Index])
                                                                       {
                                                                           throw std::runtime_error("Many-instance Conduit benchmark produced an unexpected result");
                                                                       }
                                                                   }
                                                               });

        std::cout << "Conduit microbenchmark suite\n";
        std::cout << "config=" << SNAPI_BENCH_BUILD_LABEL
                  << " samples=" << kSampleCount
                  << " warmup=" << kWarmupIterations
                  << " baseIterations=" << Iterations
                  << " inputPatternSize=" << kInputPatternSize
                  << " manyInstanceCount=" << kManyInstanceCount
                  << " manyInstancePasses=" << ManyInstancePasses
                  << "\n\n";

        PrintComparison("Self method call (dynamic input)", MethodDirect, MethodConduit, Iterations);
        PrintComparison("Self field read (dynamic source)", FieldReadDirect, FieldReadConduit, Iterations);
        PrintComparison("Self field write (dynamic input)", FieldWriteDirect, FieldWriteConduit, Iterations);
        PrintComparison("Handle-resolved method call (dynamic input)", HandleMethodDirect, HandleMethodConduit, Iterations);
        PrintComparison("Small realistic graph (dynamic input)", RealisticDirect, RealisticConduit, Iterations);
        PrintComparison("Small realistic graph across many instances", ManyDirect, ManyConduit, ManyInstanceExecutions);

        return 0;
    }
    catch (const std::exception& Ex)
    {
        std::cerr << Ex.what() << '\n';
        return 1;
    }
}
