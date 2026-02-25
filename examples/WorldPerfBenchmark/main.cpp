#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "AssetManager.h"
#include "AssetPackWriter.h"

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

class PerfComponentA final : public BaseComponent
{
public:
    static constexpr auto kTypeName = "SnAPI::GameFramework::PerfComponentA";

    int m_index = 0;
    float m_weight = 0.0f;
    Vec3 m_offset{};
    std::vector<uint8_t> m_blob{};
};

class PerfComponentB final : public BaseComponent
{
public:
    static constexpr auto kTypeName = "SnAPI::GameFramework::PerfComponentB";
    int m_group = 0;
    float m_value = 0.0f;
};

SNAPI_REFLECT_TYPE(PerfComponentA, (TTypeBuilder<PerfComponentA>(PerfComponentA::kTypeName)
    .Field("Index", &PerfComponentA::m_index)
    .Field("Weight", &PerfComponentA::m_weight)
    .Field("Offset", &PerfComponentA::m_offset)
    .Field("Blob", &PerfComponentA::m_blob)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(PerfComponentB, (TTypeBuilder<PerfComponentB>(PerfComponentB::kTypeName)
    .Field("Group", &PerfComponentB::m_group)
    .Field("Value", &PerfComponentB::m_value)
    .Constructor<>()
    .Register()));

namespace
{
using Clock = std::chrono::steady_clock;

double ToMilliseconds(const Clock::duration& Duration)
{
    return std::chrono::duration<double, std::milli>(Duration).count();
}

NodeHandle FindNodeByName(Level& Graph, const std::string& Name)
{
    NodeHandle Found;
    Graph.NodePool().ForEach([&](const NodeHandle& Handle, const BaseNode& Node) {
        if (Node.Name() == Name)
        {
            Found = Handle;
        }
    });
    return Found;
}

Level* FindLevelByName(World& WorldRef, const std::string& Name)
{
    NodeHandle Handle = FindNodeByName(WorldRef, Name);
    if (Handle.IsNull())
    {
        return nullptr;
    }
    return dynamic_cast<Level*>(Handle.Borrowed());
}

Level* FindGraphByName(Level& LevelRef, const std::string& Name)
{
    NodeHandle Handle = FindNodeByName(LevelRef, Name);
    if (Handle.IsNull())
    {
        return nullptr;
    }
    return dynamic_cast<Level*>(Handle.Borrowed());
}
} // namespace

int main()
{
    RegisterBuiltinTypes();

    constexpr int kNodeCount = 10000;
    constexpr const char* kWorldName = "PerfWorld";
    constexpr const char* kLevelName = "PerfLevel";
    constexpr const char* kGraphName = "PerfGraph";
    const std::string PackPath = "WorldPerfBenchmark_10x500MB.snpak";

    World WorldInstance(kWorldName);
    auto LevelHandleResult = WorldInstance.CreateLevel(kLevelName);
    if (!LevelHandleResult)
    {
        std::cerr << "Failed to create level" << std::endl;
        return 1;
    }

    auto* LevelRef = dynamic_cast<Level*>(LevelHandleResult->Borrowed());
    if (!LevelRef)
    {
        std::cerr << "Failed to resolve level" << std::endl;
        return 1;
    }

    auto GraphHandleResult = LevelRef->CreateNode<Level>(kGraphName);
    if (!GraphHandleResult)
    {
        std::cerr << "Failed to create level partition" << std::endl;
        return 1;
    }

    auto* GraphRef = dynamic_cast<Level*>(GraphHandleResult->Borrowed());
    if (!GraphRef)
    {
        std::cerr << "Failed to resolve level partition" << std::endl;
        return 1;
    }

    for (int i = 0; i < kNodeCount; ++i)
    {
        auto NodeResult = GraphRef->CreateNode("Node_" + std::to_string(i));
        if (!NodeResult)
        {
            std::cerr << "Failed to create node " << i << std::endl;
            return 1;
        }
        auto* Node = NodeResult.value().Borrowed();
        if (!Node)
        {
            std::cerr << "Failed to resolve node " << i << std::endl;
            return 1;
        }

        auto ComponentA = Node->Add<PerfComponentA>();
        if (!ComponentA)
        {
            std::cerr << "Failed to add PerfComponentA: " << ComponentA.error().Message << std::endl;
            return 1;
        }
        ComponentA->m_index = i;
        ComponentA->m_weight = static_cast<float>(i % 100) * 0.01f;
        ComponentA->m_offset = Vec3(
            static_cast<float>(i),
            static_cast<float>(i % 250),
            static_cast<float>(i % 500));

        auto ComponentB = Node->Add<PerfComponentB>();
        if (!ComponentB)
        {
            std::cerr << "Failed to add PerfComponentB: " << ComponentB.error().Message << std::endl;
            return 1;
        }
        ComponentB->m_group = i % 64;
        ComponentB->m_value = static_cast<float>(i) * 0.5f;
    }

    std::filesystem::remove(PackPath);

    auto SaveStart = Clock::now();

    auto WorldPayloadResult = WorldSerializer::Serialize(WorldInstance);
    if (!WorldPayloadResult)
    {
        std::cerr << "Failed to serialize world: " << WorldPayloadResult.error().Message << std::endl;
        return 1;
    }

    std::vector<uint8_t> WorldBytes;
    if (!SerializeWorldPayload(WorldPayloadResult.value(), WorldBytes))
    {
        std::cerr << "Failed to serialize world bytes" << std::endl;
        return 1;
    }

    ::SnAPI::AssetPipeline::AssetPackWriter Writer;
    {
        ::SnAPI::AssetPipeline::AssetPackEntry Entry;
        Entry.Id = AssetPipelineAssetIdFromName("perf.world");
        Entry.AssetKind = AssetKindWorld();
        Entry.Name = "perf.world";
        Entry.VariantKey = "";
        Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
            PayloadWorld(),
            WorldSerializer::kSchemaVersion,
            WorldBytes);
        Writer.AddAsset(std::move(Entry));
    }

    if (auto WriteResult = Writer.Write(PackPath); !WriteResult.has_value())
    {
        std::cerr << "Failed to write pack: " << WriteResult.error() << std::endl;
        return 1;
    }

    auto SaveEnd = Clock::now();

    ::SnAPI::AssetPipeline::AssetManager Manager;
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    Manager.GetRegistry().Freeze();
    RegisterAssetPipelineFactories(Manager);

    auto MountStart = Clock::now();
    if (auto MountResult = Manager.MountPack(PackPath); !MountResult.has_value())
    {
        std::cerr << "Failed to mount pack: " << MountResult.error() << std::endl;
        return 1;
    }
    auto MountEnd = Clock::now();

    auto LoadStart = Clock::now();
    auto LoadedWorld = Manager.Get<World>("perf.world");
    auto LoadEnd = Clock::now();

    if (!LoadedWorld.has_value())
    {
        std::cerr << "Failed to load world from AssetManager: " << LoadedWorld.error() << std::endl;
        return 1;
    }

    auto* LoadedLevel = FindLevelByName(*LoadedWorld.value(), kLevelName);
    if (!LoadedLevel)
    {
        std::cerr << "Loaded world missing level: " << kLevelName << std::endl;
        return 1;
    }

    auto* LoadedGraph = FindGraphByName(*LoadedLevel, kGraphName);
    if (!LoadedGraph)
    {
        std::cerr << "Loaded level missing graph: " << kGraphName << std::endl;
        return 1;
    }

    size_t NodeCount = 0;
    size_t ComponentACount = 0;
    size_t ComponentBCount = 0;
    LoadedGraph->NodePool().ForEach([&](const NodeHandle&, BaseNode& NodeRef) {
        ++NodeCount;
        if (NodeRef.Component<PerfComponentA>())
        {
            ++ComponentACount;
        }
        if (NodeRef.Component<PerfComponentB>())
        {
            ++ComponentBCount;
        }
    });

    const double SaveMs = ToMilliseconds(SaveEnd - SaveStart);
    const double MountMs = ToMilliseconds(MountEnd - MountStart);
    const double LoadMs = ToMilliseconds(LoadEnd - LoadStart);
    const double TotalLoadMs = MountMs + LoadMs;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "WorldPerfBenchmark results" << std::endl;
    std::cout << "Nodes: " << NodeCount << " (components A: " << ComponentACount
              << ", components B: " << ComponentBCount << ")" << std::endl;
    std::cout << "Save (serialize + pack write): " << SaveMs << " ms" << std::endl;
    std::cout << "Load (mount): " << MountMs << " ms" << std::endl;
    std::cout << "Load (asset load): " << LoadMs << " ms" << std::endl;
    std::cout << "Load (total): " << TotalLoadMs << " ms" << std::endl;

    if (std::filesystem::exists(PackPath))
    {
        std::cout << "Pack size: " << std::filesystem::file_size(PackPath) << " bytes" << std::endl;
    }

    return (NodeCount == static_cast<size_t>(kNodeCount) &&
                ComponentACount == static_cast<size_t>(kNodeCount) &&
                ComponentBCount == static_cast<size_t>(kNodeCount))
        ? 0
        : 1;
}
