#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

#ifndef SNAPI_BENCH_BUILD_LABEL
#define SNAPI_BENCH_BUILD_LABEL "Unknown"
#endif

using Clock = std::chrono::steady_clock;

struct BenchmarkStats
{
    double BestMs = 0.0;
    double AverageMs = 0.0;
    double MedianMs = 0.0;
};

template<typename THandle>
struct BenchNode
{
    std::uint64_t Id = 0;
    float Position = 0.0f;
    float Velocity = 1.0f;
    float Accumulator = 0.0f;
    std::array<THandle, 5> Components{};

    void PreTick(const float DeltaSeconds)
    {
        Accumulator += (Velocity * 0.5f + 1.0f) * DeltaSeconds;
    }

    void Tick(const float DeltaSeconds)
    {
        Position += Accumulator * DeltaSeconds;
    }

    void LateTick(const float DeltaSeconds)
    {
        Velocity += DeltaSeconds * 0.125f;
        Accumulator *= 0.975f;
    }
};

template<typename TNodeHandle, int TypeIndex>
struct BenchComponent
{
    TNodeHandle Owner{};
    float Strength = 1.0f + static_cast<float>(TypeIndex);
    float State = 0.0f;
    float Bias = 0.25f * static_cast<float>(TypeIndex + 1);

    template<typename TResolveNode>
    void Tick(TResolveNode&& ResolveNode, const float DeltaSeconds)
    {
        auto* Node = ResolveNode(Owner);
        if (!Node)
        {
            return;
        }

        State += (Node->Position + Bias) * Strength * DeltaSeconds;
        Node->Velocity += State * 0.000001f;
    }
};

struct VectorHandle
{
    std::uint32_t Index = std::numeric_limits<std::uint32_t>::max();
};

template<std::size_t PageSize>
struct PackedPagedHandle
{
    static_assert(PageSize > 0);
    static_assert((PageSize & (PageSize - 1)) == 0,
                  "Packed paged handles require a power-of-two page size");

    static constexpr std::uint32_t kInvalid = std::numeric_limits<std::uint32_t>::max();
    static constexpr std::uint32_t kSlotBits = std::countr_zero(static_cast<std::uint32_t>(PageSize));
    static constexpr std::uint32_t kSlotMask = static_cast<std::uint32_t>(PageSize - 1u);

    static PackedPagedHandle Make(const std::uint32_t PageIndex, const std::uint32_t SlotIndex)
    {
        return PackedPagedHandle{(PageIndex << kSlotBits) | SlotIndex};
    }

    std::uint32_t Page() const noexcept
    {
        return Packed >> kSlotBits;
    }

    std::uint32_t Slot() const noexcept
    {
        return Packed & kSlotMask;
    }

    std::uint32_t Packed = kInvalid;
};

struct StorageStats
{
    std::uint64_t GrowthEvents = 0;
    std::uint64_t RelocatedObjects = 0;
};

template<typename TObject>
class VectorDenseStorage
{
public:
    using Handle = VectorHandle;

    template<typename... TArgs>
    Handle Create(TArgs&&... Args)
    {
        if (m_dense.size() == m_dense.capacity())
        {
            ++m_stats.GrowthEvents;
            m_stats.RelocatedObjects += static_cast<std::uint64_t>(m_dense.size());
        }

        m_dense.emplace_back(std::forward<TArgs>(Args)...);
        return Handle{static_cast<std::uint32_t>(m_dense.size() - 1)};
    }

    TObject* Borrowed(const Handle HandleValue)
    {
        if (HandleValue.Index >= m_dense.size())
        {
            return nullptr;
        }
        return &m_dense[HandleValue.Index];
    }

    const TObject* Borrowed(const Handle HandleValue) const
    {
        if (HandleValue.Index >= m_dense.size())
        {
            return nullptr;
        }
        return &m_dense[HandleValue.Index];
    }

    const std::vector<TObject>& Dense() const noexcept
    {
        return m_dense;
    }

    std::vector<TObject>& Dense() noexcept
    {
        return m_dense;
    }

    template<typename TFn>
    void ForEach(TFn&& Fn)
    {
        for (TObject& Value : m_dense)
        {
            Fn(Value);
        }
    }

    template<typename TFn>
    void ForEach(TFn&& Fn) const
    {
        for (const TObject& Value : m_dense)
        {
            Fn(Value);
        }
    }

    StorageStats Stats() const noexcept
    {
        return m_stats;
    }

private:
    std::vector<TObject> m_dense{};
    StorageStats m_stats{};
};

template<typename TObject, std::size_t PageSize>
class PagedDenseStorage
{
public:
    static_assert(PageSize > 0);
    static_assert((PageSize & (PageSize - 1)) == 0,
                  "PagedDenseStorage uses power-of-two pages for packed handle decoding");

    using Handle = PackedPagedHandle<PageSize>;

    PagedDenseStorage() = default;

    ~PagedDenseStorage()
    {
        Clear();
    }

    PagedDenseStorage(const PagedDenseStorage&) = delete;
    PagedDenseStorage& operator=(const PagedDenseStorage&) = delete;

    template<typename... TArgs>
    Handle Create(TArgs&&... Args)
    {
        if (m_pages.empty() || m_pages.back()->Used == PageSize)
        {
            auto NewPage = std::make_unique<Page>();
            m_pages.push_back(NewPage.get());
            m_pageOwners.push_back(std::move(NewPage));
            ++m_stats.GrowthEvents;
        }

        Page& ActivePage = *m_pages.back();
        const std::uint32_t PageIndex = static_cast<std::uint32_t>(m_pages.size() - 1);
        const std::uint32_t SlotIndex = ActivePage.Used++;
        std::construct_at(ActivePage.Objects + SlotIndex, std::forward<TArgs>(Args)...);
        return Handle::Make(PageIndex, SlotIndex);
    }

    TObject* Borrowed(const Handle HandleValue)
    {
        const std::uint32_t PageIndex = HandleValue.Page();
        if (PageIndex >= m_pages.size())
        {
            return nullptr;
        }

        Page& PageRef = *m_pages[PageIndex];
        const std::uint32_t SlotIndex = HandleValue.Slot();
        if (SlotIndex >= PageRef.Used)
        {
            return nullptr;
        }

        return PageRef.Objects + SlotIndex;
    }

    const TObject* Borrowed(const Handle HandleValue) const
    {
        const std::uint32_t PageIndex = HandleValue.Page();
        if (PageIndex >= m_pages.size())
        {
            return nullptr;
        }

        const Page& PageRef = *m_pages[PageIndex];
        const std::uint32_t SlotIndex = HandleValue.Slot();
        if (SlotIndex >= PageRef.Used)
        {
            return nullptr;
        }

        return PageRef.Objects + SlotIndex;
    }

    template<typename TFn>
    void ForEach(TFn&& Fn)
    {
        for (Page* const PagePtr : m_pages)
        {
            Page& PageRef = *PagePtr;
            TObject* It = PageRef.Objects;
            TObject* const End = PageRef.Objects + PageRef.Used;
            for (; It != End; ++It)
            {
                Fn(*It);
            }
        }
    }

    template<typename TFn>
    void ForEach(TFn&& Fn) const
    {
        for (const Page* const PagePtr : m_pages)
        {
            const Page& PageRef = *PagePtr;
            const TObject* It = PageRef.Objects;
            const TObject* const End = PageRef.Objects + PageRef.Used;
            for (; It != End; ++It)
            {
                Fn(*It);
            }
        }
    }

    StorageStats Stats() const noexcept
    {
        return m_stats;
    }

private:
    struct Page
    {
        Page()
            : Objects(std::allocator<TObject>{}.allocate(PageSize))
        {
        }

        ~Page()
        {
            if (!Objects)
            {
                return;
            }

            for (std::uint32_t Index = 0; Index < Used; ++Index)
            {
                std::destroy_at(Objects + Index);
            }
            std::allocator<TObject>{}.deallocate(Objects, PageSize);
        }

        TObject* Objects = nullptr;
        std::uint32_t Used = 0;
    };

    void Clear()
    {
        m_pages.clear();
        m_pageOwners.clear();
    }

    std::vector<Page*> m_pages{};
    std::vector<std::unique_ptr<Page>> m_pageOwners{};
    StorageStats m_stats{};
};

template<template<typename> class TNodeStorage,
         template<typename> class TComponentStorage,
         typename TNodeHandle>
class VectorWorldModel;

template<typename TObject>
using VectorStorageAlias = VectorDenseStorage<TObject>;

template<typename TObject>
using PagedStorageAlias = PagedDenseStorage<TObject, 1024>;

template<typename TNodeStorage,
         typename TCompStorage0,
         typename TCompStorage1,
         typename TCompStorage2,
         typename TCompStorage3,
         typename TCompStorage4,
         typename TNodeHandle>
class BenchWorld
{
public:
    using Node = BenchNode<TNodeHandle>;
    using Comp0 = BenchComponent<TNodeHandle, 0>;
    using Comp1 = BenchComponent<TNodeHandle, 1>;
    using Comp2 = BenchComponent<TNodeHandle, 2>;
    using Comp3 = BenchComponent<TNodeHandle, 3>;
    using Comp4 = BenchComponent<TNodeHandle, 4>;

    void Spawn(const std::size_t NodeCount)
    {
        for (std::size_t Index = 0; Index < NodeCount; ++Index)
        {
            const TNodeHandle NodeHandle = CreateNode(m_nextNodeId++);
            auto* NodePtr = BorrowNode(NodeHandle);
            if (!NodePtr)
            {
                throw std::runtime_error("Node creation failed");
            }

            if (m_sampleNodeHandles.size() < kSampleLimit)
            {
                m_sampleNodeHandles.push_back(NodeHandle);
            }

            NodePtr->Components[0] = CreateComponent<0>(NodeHandle);
            MaybeCaptureComponent(NodePtr->Components[0]);
            NodePtr->Components[1] = CreateComponent<1>(NodeHandle);
            MaybeCaptureComponent(NodePtr->Components[1]);
            NodePtr->Components[2] = CreateComponent<2>(NodeHandle);
            MaybeCaptureComponent(NodePtr->Components[2]);
            NodePtr->Components[3] = CreateComponent<3>(NodeHandle);
            MaybeCaptureComponent(NodePtr->Components[3]);
            NodePtr->Components[4] = CreateComponent<4>(NodeHandle);
            MaybeCaptureComponent(NodePtr->Components[4]);
        }
    }

    void SetPointerSampleLimit(const std::size_t SampleLimit)
    {
        kSampleLimit = SampleLimit;
        m_sampleNodeHandles.reserve(SampleLimit);
        m_sampleComponentHandles.reserve(SampleLimit);
    }

    void TickFrame(const float DeltaSeconds)
    {
        ForEachNode([&](Node& NodeRef)
        {
            NodeRef.PreTick(DeltaSeconds);
        });

        ForEachComponent<0>([&](Comp0& Component)
        {
            Component.Tick([&](const TNodeHandle HandleValue)
            {
                return BorrowNode(HandleValue);
            }, DeltaSeconds);
        });
        ForEachComponent<1>([&](Comp1& Component)
        {
            Component.Tick([&](const TNodeHandle HandleValue)
            {
                return BorrowNode(HandleValue);
            }, DeltaSeconds);
        });
        ForEachComponent<2>([&](Comp2& Component)
        {
            Component.Tick([&](const TNodeHandle HandleValue)
            {
                return BorrowNode(HandleValue);
            }, DeltaSeconds);
        });
        ForEachComponent<3>([&](Comp3& Component)
        {
            Component.Tick([&](const TNodeHandle HandleValue)
            {
                return BorrowNode(HandleValue);
            }, DeltaSeconds);
        });
        ForEachComponent<4>([&](Comp4& Component)
        {
            Component.Tick([&](const TNodeHandle HandleValue)
            {
                return BorrowNode(HandleValue);
            }, DeltaSeconds);
        });

        ForEachNode([&](Node& NodeRef)
        {
            NodeRef.Tick(DeltaSeconds);
        });

        ForEachNode([&](Node& NodeRef)
        {
            NodeRef.LateTick(DeltaSeconds);
        });
    }

    double Checksum() const
    {
        double Result = 0.0;
        ForEachNodeConst([&](const Node& NodeRef)
        {
            Result += static_cast<double>(NodeRef.Position);
            Result += static_cast<double>(NodeRef.Velocity);
            Result += static_cast<double>(NodeRef.Accumulator);
        });
        ForEachComponentConst<0>([&](const Comp0& Component)
        {
            Result += static_cast<double>(Component.State);
        });
        ForEachComponentConst<1>([&](const Comp1& Component)
        {
            Result += static_cast<double>(Component.State);
        });
        ForEachComponentConst<2>([&](const Comp2& Component)
        {
            Result += static_cast<double>(Component.State);
        });
        ForEachComponentConst<3>([&](const Comp3& Component)
        {
            Result += static_cast<double>(Component.State);
        });
        ForEachComponentConst<4>([&](const Comp4& Component)
        {
            Result += static_cast<double>(Component.State);
        });
        return Result;
    }

    StorageStats NodeStorageStats() const
    {
        return m_nodes.Stats();
    }

    StorageStats ComponentStorageStats() const
    {
        StorageStats Result{};
        const StorageStats Stats0 = m_comp0.Stats();
        const StorageStats Stats1 = m_comp1.Stats();
        const StorageStats Stats2 = m_comp2.Stats();
        const StorageStats Stats3 = m_comp3.Stats();
        const StorageStats Stats4 = m_comp4.Stats();
        Result.GrowthEvents = Stats0.GrowthEvents + Stats1.GrowthEvents + Stats2.GrowthEvents + Stats3.GrowthEvents + Stats4.GrowthEvents;
        Result.RelocatedObjects = Stats0.RelocatedObjects + Stats1.RelocatedObjects + Stats2.RelocatedObjects + Stats3.RelocatedObjects + Stats4.RelocatedObjects;
        return Result;
    }

    std::vector<const void*> SampleNodePointers() const
    {
        std::vector<const void*> Result;
        Result.reserve(m_sampleNodeHandles.size());
        for (const TNodeHandle HandleValue : m_sampleNodeHandles)
        {
            Result.push_back(BorrowNodeConst(HandleValue));
        }
        return Result;
    }

    std::vector<const void*> SampleComponentPointers() const
    {
        std::vector<const void*> Result;
        Result.reserve(m_sampleComponentHandles.size());
        for (const TNodeHandle HandleValue : m_sampleComponentHandles)
        {
            Result.push_back(BorrowAnyComponentConst(HandleValue));
        }
        return Result;
    }

    std::size_t SampleCount() const
    {
        return std::min(m_sampleNodeHandles.size(), m_sampleComponentHandles.size());
    }

private:
    TNodeHandle CreateNode(const std::uint64_t Id)
    {
        return m_nodes.Create(Node{.Id = Id});
    }

    template<int Index>
    auto CreateComponent(const TNodeHandle Owner)
    {
        if constexpr (Index == 0)
        {
            return m_comp0.Create(Comp0{.Owner = Owner});
        }
        else if constexpr (Index == 1)
        {
            return m_comp1.Create(Comp1{.Owner = Owner});
        }
        else if constexpr (Index == 2)
        {
            return m_comp2.Create(Comp2{.Owner = Owner});
        }
        else if constexpr (Index == 3)
        {
            return m_comp3.Create(Comp3{.Owner = Owner});
        }
        else
        {
            static_assert(Index == 4);
            return m_comp4.Create(Comp4{.Owner = Owner});
        }
    }

    Node* BorrowNode(const TNodeHandle HandleValue)
    {
        return m_nodes.Borrowed(HandleValue);
    }

    const Node* BorrowNodeConst(const TNodeHandle HandleValue) const
    {
        return m_nodes.Borrowed(HandleValue);
    }

    const void* BorrowAnyComponentConst(const TNodeHandle HandleValue) const
    {
        if (const auto* Comp = m_comp0.Borrowed(HandleValue))
        {
            return Comp;
        }
        if (const auto* Comp = m_comp1.Borrowed(HandleValue))
        {
            return Comp;
        }
        if (const auto* Comp = m_comp2.Borrowed(HandleValue))
        {
            return Comp;
        }
        if (const auto* Comp = m_comp3.Borrowed(HandleValue))
        {
            return Comp;
        }
        if (const auto* Comp = m_comp4.Borrowed(HandleValue))
        {
            return Comp;
        }
        return nullptr;
    }

    void MaybeCaptureComponent(const TNodeHandle HandleValue)
    {
        if (m_sampleComponentHandles.size() < kSampleLimit)
        {
            m_sampleComponentHandles.push_back(HandleValue);
        }
    }

    template<typename TFn>
    void ForEachNode(TFn&& Fn)
    {
        if constexpr (requires(TNodeStorage Storage, TFn Callback)
        {
            Storage.ForEach(Callback);
        })
        {
            m_nodes.ForEach(std::forward<TFn>(Fn));
        }
        else
        {
            for (Node& NodeRef : m_nodes.Dense())
            {
                Fn(NodeRef);
            }
        }
    }

    template<typename TFn>
    void ForEachNodeConst(TFn&& Fn) const
    {
        m_nodes.ForEach(std::forward<TFn>(Fn));
    }

    template<int Index, typename TFn>
    void ForEachComponent(TFn&& Fn)
    {
        if constexpr (Index == 0)
        {
            ForEachStorage(m_comp0, std::forward<TFn>(Fn));
        }
        else if constexpr (Index == 1)
        {
            ForEachStorage(m_comp1, std::forward<TFn>(Fn));
        }
        else if constexpr (Index == 2)
        {
            ForEachStorage(m_comp2, std::forward<TFn>(Fn));
        }
        else if constexpr (Index == 3)
        {
            ForEachStorage(m_comp3, std::forward<TFn>(Fn));
        }
        else
        {
            static_assert(Index == 4);
            ForEachStorage(m_comp4, std::forward<TFn>(Fn));
        }
    }

    template<int Index, typename TFn>
    void ForEachComponentConst(TFn&& Fn) const
    {
        if constexpr (Index == 0)
        {
            ForEachStorageConst(m_comp0, std::forward<TFn>(Fn));
        }
        else if constexpr (Index == 1)
        {
            ForEachStorageConst(m_comp1, std::forward<TFn>(Fn));
        }
        else if constexpr (Index == 2)
        {
            ForEachStorageConst(m_comp2, std::forward<TFn>(Fn));
        }
        else if constexpr (Index == 3)
        {
            ForEachStorageConst(m_comp3, std::forward<TFn>(Fn));
        }
        else
        {
            static_assert(Index == 4);
            ForEachStorageConst(m_comp4, std::forward<TFn>(Fn));
        }
    }

    template<typename TStorage, typename TFn>
    static void ForEachStorage(TStorage& Storage, TFn&& Fn)
    {
        if constexpr (requires(TStorage ConcreteStorage, TFn Callback)
        {
            ConcreteStorage.ForEach(Callback);
        })
        {
            Storage.ForEach(std::forward<TFn>(Fn));
        }
        else
        {
            for (auto& Value : Storage.Dense())
            {
                Fn(Value);
            }
        }
    }

    template<typename TStorage, typename TFn>
    static void ForEachStorageConst(const TStorage& Storage, TFn&& Fn)
    {
        Storage.ForEach(std::forward<TFn>(Fn));
    }

    TNodeStorage m_nodes{};
    TCompStorage0 m_comp0{};
    TCompStorage1 m_comp1{};
    TCompStorage2 m_comp2{};
    TCompStorage3 m_comp3{};
    TCompStorage4 m_comp4{};
    std::uint64_t m_nextNodeId = 1;
    std::size_t kSampleLimit = 0;
    std::vector<TNodeHandle> m_sampleNodeHandles{};
    std::vector<TNodeHandle> m_sampleComponentHandles{};
};

using VectorWorld = BenchWorld<VectorDenseStorage<BenchNode<VectorHandle>>,
                               VectorDenseStorage<BenchComponent<VectorHandle, 0>>,
                               VectorDenseStorage<BenchComponent<VectorHandle, 1>>,
                               VectorDenseStorage<BenchComponent<VectorHandle, 2>>,
                               VectorDenseStorage<BenchComponent<VectorHandle, 3>>,
                               VectorDenseStorage<BenchComponent<VectorHandle, 4>>,
                               VectorHandle>;

template<std::size_t PageSize>
using PagedWorldFor = BenchWorld<PagedDenseStorage<BenchNode<PackedPagedHandle<PageSize>>, PageSize>,
                                 PagedDenseStorage<BenchComponent<PackedPagedHandle<PageSize>, 0>, PageSize>,
                                 PagedDenseStorage<BenchComponent<PackedPagedHandle<PageSize>, 1>, PageSize>,
                                 PagedDenseStorage<BenchComponent<PackedPagedHandle<PageSize>, 2>, PageSize>,
                                 PagedDenseStorage<BenchComponent<PackedPagedHandle<PageSize>, 3>, PageSize>,
                                 PagedDenseStorage<BenchComponent<PackedPagedHandle<PageSize>, 4>, PageSize>,
                                 PackedPagedHandle<PageSize>>;

template<typename TFn>
double MeasureOnceMs(TFn&& Fn)
{
    const auto Start = Clock::now();
    Fn();
    const auto End = Clock::now();
    return std::chrono::duration<double, std::milli>(End - Start).count();
}

BenchmarkStats Summarize(std::vector<double> Samples)
{
    if (Samples.empty())
    {
        return {};
    }

    std::sort(Samples.begin(), Samples.end());
    const double Total = std::accumulate(Samples.begin(), Samples.end(), 0.0);
    return BenchmarkStats{
        .BestMs = Samples.front(),
        .AverageMs = Total / static_cast<double>(Samples.size()),
        .MedianMs = Samples[Samples.size() / 2],
    };
}

template<typename TWorld>
struct ScenarioResult
{
    double SpawnMs = 0.0;
    double BurstSpawnMs = 0.0;
    BenchmarkStats TickStats{};
    double Checksum = 0.0;
    StorageStats NodeStats{};
    StorageStats ComponentStats{};
    std::size_t StableSampleNodePointers = 0;
    std::size_t StableSampleComponentPointers = 0;
    std::size_t SampleCount = 0;
};

template<typename TWorld>
ScenarioResult<TWorld> RunScenario(const std::size_t NodeCount,
                                   const std::size_t BurstNodeCount,
                                   const std::size_t PointerSampleCount,
                                   const std::size_t TickSamples,
                                   const std::size_t WarmupTicks,
                                   const float DeltaSeconds)
{
    ScenarioResult<TWorld> Result{};
    TWorld World{};
    World.SetPointerSampleLimit(PointerSampleCount);

    Result.SpawnMs = MeasureOnceMs([&]()
    {
        World.Spawn(NodeCount);
    });

    const std::vector<const void*> NodePointersBeforeBurst = World.SampleNodePointers();
    const std::vector<const void*> ComponentPointersBeforeBurst = World.SampleComponentPointers();

    Result.BurstSpawnMs = MeasureOnceMs([&]()
    {
        World.Spawn(BurstNodeCount);
    });

    const std::vector<const void*> NodePointersAfterBurst = World.SampleNodePointers();
    const std::vector<const void*> ComponentPointersAfterBurst = World.SampleComponentPointers();
    Result.SampleCount = std::min({NodePointersBeforeBurst.size(),
                                   NodePointersAfterBurst.size(),
                                   ComponentPointersBeforeBurst.size(),
                                   ComponentPointersAfterBurst.size()});

    for (std::size_t Index = 0; Index < Result.SampleCount; ++Index)
    {
        if (NodePointersBeforeBurst[Index] == NodePointersAfterBurst[Index])
        {
            ++Result.StableSampleNodePointers;
        }
        if (ComponentPointersBeforeBurst[Index] == ComponentPointersAfterBurst[Index])
        {
            ++Result.StableSampleComponentPointers;
        }
    }

    for (std::size_t Index = 0; Index < WarmupTicks; ++Index)
    {
        World.TickFrame(DeltaSeconds);
    }

    std::vector<double> TickSamplesMs;
    TickSamplesMs.reserve(TickSamples);
    for (std::size_t Index = 0; Index < TickSamples; ++Index)
    {
        TickSamplesMs.push_back(MeasureOnceMs([&]()
        {
            World.TickFrame(DeltaSeconds);
        }));
    }

    Result.TickStats = Summarize(std::move(TickSamplesMs));
    Result.Checksum = World.Checksum();
    Result.NodeStats = World.NodeStorageStats();
    Result.ComponentStats = World.ComponentStorageStats();
    return Result;
}

void PrintStatsLine(const std::string& Label, const BenchmarkStats& Stats)
{
    std::cout << "  " << Label
              << ": best=" << std::fixed << std::setprecision(3) << Stats.BestMs << " ms"
              << " avg=" << Stats.AverageMs << " ms"
              << " median=" << Stats.MedianMs << " ms\n";
}

template<typename TWorldResult>
void PrintResult(const std::string& Label, const TWorldResult& Result)
{
    std::cout << Label << "\n";
    std::cout << "  initial_spawn: " << std::fixed << std::setprecision(3) << Result.SpawnMs << " ms\n";
    std::cout << "  burst_spawn: " << std::fixed << std::setprecision(3) << Result.BurstSpawnMs << " ms\n";
    PrintStatsLine("tick", Result.TickStats);
    std::cout << "  node growth events: " << Result.NodeStats.GrowthEvents << "\n";
    std::cout << "  node relocated objects: " << Result.NodeStats.RelocatedObjects << "\n";
    std::cout << "  component growth events: " << Result.ComponentStats.GrowthEvents << "\n";
    std::cout << "  component relocated objects: " << Result.ComponentStats.RelocatedObjects << "\n";
    std::cout << "  stable sampled node pointers: " << Result.StableSampleNodePointers << "/" << Result.SampleCount << "\n";
    std::cout << "  stable sampled component pointers: " << Result.StableSampleComponentPointers << "/" << Result.SampleCount << "\n";
    std::cout << "  checksum: " << std::setprecision(6) << Result.Checksum << "\n";
}

std::size_t ParseSizeArg(const char* Value, const char* Name)
{
    if (!Value)
    {
        throw std::invalid_argument(std::string("Missing value for ") + Name);
    }

    char* End = nullptr;
    const unsigned long long Parsed = std::strtoull(Value, &End, 10);
    if (End == Value || *End != '\0')
    {
        throw std::invalid_argument(std::string("Invalid numeric value for ") + Name + ": " + Value);
    }
    return static_cast<std::size_t>(Parsed);
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::size_t NodeCount = 1'000'000;
        std::size_t BurstNodeCount = 100'000;
        std::size_t PointerSampleCount = 4096;
        std::size_t TickSamples = 7;
        std::size_t WarmupTicks = 3;
        std::size_t PagedPageSize = 16384;
        float DeltaSeconds = 1.0f / 60.0f;

        for (int Index = 1; Index < argc; ++Index)
        {
            const std::string_view Arg = argv[Index];
            if (Arg == "--nodes")
            {
                NodeCount = ParseSizeArg(argv[++Index], "--nodes");
            }
            else if (Arg == "--burst-nodes")
            {
                BurstNodeCount = ParseSizeArg(argv[++Index], "--burst-nodes");
            }
            else if (Arg == "--pointer-samples")
            {
                PointerSampleCount = ParseSizeArg(argv[++Index], "--pointer-samples");
            }
            else if (Arg == "--page-size")
            {
                PagedPageSize = ParseSizeArg(argv[++Index], "--page-size");
            }
            else if (Arg == "--samples")
            {
                TickSamples = ParseSizeArg(argv[++Index], "--samples");
            }
            else if (Arg == "--warmup")
            {
                WarmupTicks = ParseSizeArg(argv[++Index], "--warmup");
            }
            else if (Arg == "--help")
            {
                std::cout << "Usage: EcsStoragePerf [--nodes N] [--burst-nodes N] [--pointer-samples N] [--page-size 1024|2048|4096|8192|16384] [--samples N] [--warmup N]\n";
                return 0;
            }
            else
            {
                throw std::invalid_argument(std::string("Unknown argument: ") + std::string(Arg));
            }
        }

        std::cout << "ECS storage microbenchmark\n";
        std::cout << "build=" << SNAPI_BENCH_BUILD_LABEL
                  << " nodes=" << NodeCount
                  << " burst_nodes=" << BurstNodeCount
                  << " components_per_node=5"
                  << " pointer_samples=" << PointerSampleCount
                  << " warmup_ticks=" << WarmupTicks
                  << " tick_samples=" << TickSamples
                  << " page_size=" << PagedPageSize << "\n\n";

        const auto VectorResult = RunScenario<VectorWorld>(NodeCount, BurstNodeCount, PointerSampleCount, TickSamples, WarmupTicks, DeltaSeconds);
        PrintResult("vector_dense_storage", VectorResult);
        std::cout << "\n";

        auto PrintRelative = [&](const auto& PagedResult)
        {
            PrintResult("paged_dense_storage", PagedResult);
            std::cout << "\n";

            std::cout << "relative\n";
            std::cout << "  initial_spawn_speedup_paged_vs_vector: "
                      << std::fixed << std::setprecision(3)
                      << (VectorResult.SpawnMs / std::max(PagedResult.SpawnMs, 0.000001))
                      << "x\n";
            std::cout << "  burst_spawn_speedup_paged_vs_vector: "
                      << (VectorResult.BurstSpawnMs / std::max(PagedResult.BurstSpawnMs, 0.000001))
                      << "x\n";
            std::cout << "  tick_speedup_paged_vs_vector: "
                      << (VectorResult.TickStats.AverageMs / std::max(PagedResult.TickStats.AverageMs, 0.000001))
                      << "x\n";
        };

        switch (PagedPageSize)
        {
        case 1024:
        {
            const auto PagedResult = RunScenario<PagedWorldFor<1024>>(NodeCount,
                                                                      BurstNodeCount,
                                                                      PointerSampleCount,
                                                                      TickSamples,
                                                                      WarmupTicks,
                                                                      DeltaSeconds);
            PrintRelative(PagedResult);
            break;
        }
        case 2048:
        {
            const auto PagedResult = RunScenario<PagedWorldFor<2048>>(NodeCount,
                                                                      BurstNodeCount,
                                                                      PointerSampleCount,
                                                                      TickSamples,
                                                                      WarmupTicks,
                                                                      DeltaSeconds);
            PrintRelative(PagedResult);
            break;
        }
        case 4096:
        {
            const auto PagedResult = RunScenario<PagedWorldFor<4096>>(NodeCount,
                                                                      BurstNodeCount,
                                                                      PointerSampleCount,
                                                                      TickSamples,
                                                                      WarmupTicks,
                                                                      DeltaSeconds);
            PrintRelative(PagedResult);
            break;
        }
        case 8192:
        {
            const auto PagedResult = RunScenario<PagedWorldFor<8192>>(NodeCount,
                                                                      BurstNodeCount,
                                                                      PointerSampleCount,
                                                                      TickSamples,
                                                                      WarmupTicks,
                                                                      DeltaSeconds);
            PrintRelative(PagedResult);
            break;
        }
        case 16384:
        {
            const auto PagedResult = RunScenario<PagedWorldFor<16384>>(NodeCount,
                                                                       BurstNodeCount,
                                                                       PointerSampleCount,
                                                                       TickSamples,
                                                                       WarmupTicks,
                                                                       DeltaSeconds);
            PrintRelative(PagedResult);
            break;
        }
        default:
            throw std::invalid_argument("Unsupported --page-size. Supported values: 1024, 2048, 4096, 8192, 16384");
        }

        return 0;
    }
    catch (const std::exception& Exception)
    {
        std::cerr << "EcsStoragePerf failed: " << Exception.what() << "\n";
        return 1;
    }
}
