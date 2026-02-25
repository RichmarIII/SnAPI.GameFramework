#include <iostream>
#include <string>
#include <vector>

#include "AssetManager.h"
#include "AssetPackWriter.h"

#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;

/**
 * @brief Example relevance policy that always keeps nodes active.
 * @remarks Used to demonstrate relevance component plumbing without culling logic.
 */
struct AlwaysActivePolicy
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AlwaysActivePolicy";

    bool Evaluate(const RelevanceContext&) const
    {
        return true;
    }
};

class DemoNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DemoNode";

    int m_health = 0;
    float m_speed = 0.0f;
    std::string m_tag{};
    Vec3 m_spawn{};
    NodeHandle m_target{};
};

class DemoComponent final : public BaseComponent, public ComponentCRTP<DemoComponent>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DemoComponent";

    int m_score = 0;
    std::string m_label{};
    Vec3 m_tint{};
};

SNAPI_REFLECT_TYPE(DemoNode, (TTypeBuilder<DemoNode>(DemoNode::kTypeName)
    .Base<BaseNode>()
    .Field("Health", &DemoNode::m_health)
    .Field("Speed", &DemoNode::m_speed)
    .Field("Tag", &DemoNode::m_tag)
    .Field("Spawn", &DemoNode::m_spawn)
    .Field("Target", &DemoNode::m_target)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(DemoComponent, (TTypeBuilder<DemoComponent>(DemoComponent::kTypeName)
    .Field("Score", &DemoComponent::m_score)
    .Field("Label", &DemoComponent::m_label)
    .Field("Tint", &DemoComponent::m_tint)
    .Constructor<>()
    .Register()));

namespace
{
template<typename TGraphLike>
NodeHandle FindNodeByName(TGraphLike& Graph, const std::string& Name)
{
    NodeHandle Found;
    Graph.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Name() == Name)
        {
            Found = Handle;
        }
    });
    return Found;
}

Level* FindGraphByName(Level& Graph, const std::string& Name)
{
    NodeHandle Handle = FindNodeByName(Graph, Name);
    if (Handle.IsNull())
    {
        return nullptr;
    }
    return NodeCast<Level>(Handle.Borrowed());
}

bool ValidateDemoNode(
    Level& Graph,
    const std::string& NodeName,
    const std::string& TargetName,
    int ExpectedHealth,
    float ExpectedSpeed,
    const std::string& ExpectedTag,
    const Vec3& ExpectedSpawn,
    int ExpectedScore,
    const std::string& ExpectedLabel,
    const Vec3& ExpectedTint)
{
    NodeHandle Handle = FindNodeByName(Graph, NodeName);
    auto* Node = NodeCast<DemoNode>(Handle.Borrowed());
    if (!Node)
    {
        std::cerr << "Missing demo node: " << NodeName << std::endl;
        return false;
    }
    if (Node->m_health != ExpectedHealth ||
        Node->m_speed != ExpectedSpeed ||
        Node->m_tag != ExpectedTag)
    {
        std::cerr << "Demo node fields mismatch on " << NodeName << std::endl;
        return false;
    }
    if (Node->m_spawn.x() != ExpectedSpawn.x() ||
        Node->m_spawn.y() != ExpectedSpawn.y() ||
        Node->m_spawn.z() != ExpectedSpawn.z())
    {
        std::cerr << "Demo node spawn mismatch on " << NodeName << std::endl;
        return false;
    }
    auto* Target = Node->m_target.Borrowed();
    if (!Target || Target->Name() != TargetName)
    {
        std::cerr << "Demo node target mismatch on " << NodeName << std::endl;
        return false;
    }
    auto StatsResult = Node->Component<DemoComponent>();
    if (!StatsResult)
    {
        std::cerr << "Missing DemoComponent on " << NodeName << ": " << StatsResult.error().Message << std::endl;
        return false;
    }
    if (StatsResult->m_score != ExpectedScore ||
        StatsResult->m_label != ExpectedLabel ||
        StatsResult->m_tint.x() != ExpectedTint.x() ||
        StatsResult->m_tint.y() != ExpectedTint.y() ||
        StatsResult->m_tint.z() != ExpectedTint.z())
    {
        std::cerr << "Demo component mismatch on " << NodeName << std::endl;
        return false;
    }
    return true;
}
} // namespace

int main()
{
    RegisterBuiltinTypes();

    World WorldInstance("FeatureWorld");
    auto LevelHandleResult = WorldInstance.CreateLevel("MainLevel");
    if (!LevelHandleResult)
    {
        std::cerr << "Failed to create level" << std::endl;
        return 1;
    }

    auto LevelRef = NodeCast<Level>(LevelHandleResult->Borrowed());
    if (!LevelRef)
    {
        std::cerr << "Failed to resolve level" << std::endl;
        return 1;
    }

    auto LevelPartitionHandleResult = LevelRef->CreateNode<Level>("Gameplay");
    if (!LevelPartitionHandleResult)
    {
        std::cerr << "Failed to create level partition" << std::endl;
        return 1;
    }

    auto GraphRef = NodeCast<Level>(LevelPartitionHandleResult->Borrowed());
    if (!GraphRef)
    {
        std::cerr << "Failed to resolve level partition" << std::endl;
        return 1;
    }

    auto TargetResult = GraphRef->CreateNode("Target");
    if (!TargetResult)
    {
        std::cerr << "Failed to create target node" << std::endl;
        return 1;
    }

    auto PlayerResult = GraphRef->CreateNode<DemoNode>("Player");
    if (!PlayerResult)
    {
        std::cerr << "Failed to create player node" << std::endl;
        return 1;
    }
    auto* Player = NodeCast<DemoNode>(PlayerResult.value().Borrowed());
    if (!Player)
    {
        std::cerr << "Failed to resolve player node" << std::endl;
        return 1;
    }
    Player->m_health = 150;
    Player->m_speed = 4.5f;
    Player->m_tag = "Hero";
    Player->m_spawn = Vec3(1.0f, 2.0f, 3.0f);
    Player->m_target = TargetResult.value();

    auto DemoComponentResult = Player->Add<DemoComponent>();
    if (!DemoComponentResult)
    {
        std::cerr << "Failed to add DemoComponent: " << DemoComponentResult.error().Message << std::endl;
        return 1;
    }
    DemoComponentResult->m_score = 9001;
    DemoComponentResult->m_label = "Ranger";
    DemoComponentResult->m_tint = Vec3(0.2f, 0.6f, 1.0f);

    if (auto TransformResult = Player->Add<TransformComponent>())
    {
        TransformResult->Position = Vec3(1.0f, 2.0f, 3.0f);
    }

    if (auto RelevanceResult = Player->Add<RelevanceComponent>())
    {
        RelevanceResult->Policy(AlwaysActivePolicy{});
    }

    World StandalonePrefabWorld("StandalonePrefabWorld");
    auto PrefabTargetResult = StandalonePrefabWorld.CreateNode("PrefabTarget");
    if (!PrefabTargetResult)
    {
        std::cerr << "Failed to create prefab target" << std::endl;
        return 1;
    }
    auto PrefabActorResult = StandalonePrefabWorld.CreateNode<DemoNode>("PrefabActor");
    if (!PrefabActorResult)
    {
        std::cerr << "Failed to create prefab actor" << std::endl;
        return 1;
    }
    auto* PrefabActor = NodeCast<DemoNode>(PrefabActorResult.value().Borrowed());
    if (!PrefabActor)
    {
        std::cerr << "Failed to resolve prefab actor" << std::endl;
        return 1;
    }
    if (auto AttachResult = StandalonePrefabWorld.AttachChild(PrefabActorResult.value(), PrefabTargetResult.value()); !AttachResult)
    {
        std::cerr << "Failed to attach prefab target: " << AttachResult.error().Message << std::endl;
        return 1;
    }
    PrefabActor->m_health = 60;
    PrefabActor->m_speed = 2.5f;
    PrefabActor->m_tag = "Prefab";
    PrefabActor->m_spawn = Vec3(5.0f, 6.0f, 7.0f);
    PrefabActor->m_target = PrefabTargetResult.value();

    auto PrefabComponentResult = PrefabActor->Add<DemoComponent>();
    if (!PrefabComponentResult)
    {
        std::cerr << "Failed to add prefab DemoComponent: " << PrefabComponentResult.error().Message << std::endl;
        return 1;
    }
    PrefabComponentResult->m_score = 777;
    PrefabComponentResult->m_label = "PrefabComponent";
    PrefabComponentResult->m_tint = Vec3(1.0f, 0.4f, 0.1f);

    auto WorldPayloadResult = WorldSerializer::Serialize(WorldInstance);
    if (!WorldPayloadResult)
    {
        std::cerr << "Failed to serialize world: " << WorldPayloadResult.error().Message << std::endl;
        return 1;
    }
    auto LevelPayloadResult = LevelSerializer::Serialize(*LevelRef);
    if (!LevelPayloadResult)
    {
        std::cerr << "Failed to serialize level: " << LevelPayloadResult.error().Message << std::endl;
        return 1;
    }
    auto NodePayloadResult = NodeSerializer::Serialize(*PrefabActor);
    if (!NodePayloadResult)
    {
        std::cerr << "Failed to serialize node: " << NodePayloadResult.error().Message << std::endl;
        return 1;
    }

    std::vector<uint8_t> WorldBytes;
    std::vector<uint8_t> LevelBytes;
    std::vector<uint8_t> NodeBytes;
    if (!SerializeWorldPayload(WorldPayloadResult.value(), WorldBytes))
    {
        std::cerr << "Failed to serialize world bytes" << std::endl;
        return 1;
    }
    if (!SerializeLevelPayload(LevelPayloadResult.value(), LevelBytes))
    {
        std::cerr << "Failed to serialize level bytes" << std::endl;
        return 1;
    }
    if (!SerializeNodePayload(NodePayloadResult.value(), NodeBytes))
    {
        std::cerr << "Failed to serialize node bytes" << std::endl;
        return 1;
    }

    const std::string PackPath = "FeatureShowcase_Content.snpak";

    ::SnAPI::AssetPipeline::AssetPackWriter Writer;
    {
        ::SnAPI::AssetPipeline::AssetPackEntry Entry;
        Entry.Id = AssetPipelineAssetIdFromName("feature.world");
        Entry.AssetKind = AssetKindWorld();
        Entry.Name = "feature.world";
        Entry.VariantKey = "";
        Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadWorld(), WorldSerializer::kSchemaVersion, WorldBytes);
        Writer.AddAsset(std::move(Entry));
    }
    {
        ::SnAPI::AssetPipeline::AssetPackEntry Entry;
        Entry.Id = AssetPipelineAssetIdFromName("feature.level");
        Entry.AssetKind = AssetKindLevel();
        Entry.Name = "feature.level";
        Entry.VariantKey = "";
        Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadLevel(), LevelSerializer::kSchemaVersion, LevelBytes);
        Writer.AddAsset(std::move(Entry));
    }
    {
        ::SnAPI::AssetPipeline::AssetPackEntry Entry;
        Entry.Id = AssetPipelineAssetIdFromName("feature.graph");
        Entry.AssetKind = AssetKindNode();
        Entry.Name = "feature.graph";
        Entry.VariantKey = "";
        Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadNode(), NodeSerializer::kSchemaVersion, NodeBytes);
        Writer.AddAsset(std::move(Entry));
    }

    if (auto WriteResult = Writer.Write(PackPath); !WriteResult.has_value())
    {
        std::cerr << "Failed to write pack: " << WriteResult.error() << std::endl;
        return 1;
    }

    ::SnAPI::AssetPipeline::AssetManager Manager;
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    Manager.GetRegistry().Freeze();
    RegisterAssetPipelineFactories(Manager);

    if (auto MountResult = Manager.MountPack(PackPath); !MountResult.has_value())
    {
        std::cerr << "Failed to mount pack: " << MountResult.error() << std::endl;
        return 1;
    }

    auto LoadedWorldResult = Manager.Load<World>("feature.world");
    if (!LoadedWorldResult.has_value())
    {
        std::cerr << "Failed to load world from AssetManager: " << LoadedWorldResult.error() << std::endl;
        return 1;
    }
    World& LoadedWorld = *LoadedWorldResult.value();

    auto* WorldLevel = NodeCast<Level>(FindNodeByName(LoadedWorld, "MainLevel").Borrowed());
    if (!WorldLevel)
    {
        std::cerr << "World missing MainLevel node" << std::endl;
        return 1;
    }
    auto* WorldGameplay = FindGraphByName(*WorldLevel, "Gameplay");
    if (!WorldGameplay)
    {
        std::cerr << "World MainLevel missing Gameplay graph" << std::endl;
        return 1;
    }
    if (!ValidateDemoNode(
            *WorldGameplay,
            "Player",
            "Target",
            150,
            4.5f,
            "Hero",
            Vec3(1.0f, 2.0f, 3.0f),
            9001,
            "Ranger",
            Vec3(0.2f, 0.6f, 1.0f)))
    {
        return 1;
    }

    World LoadedLevelWorld("LoadedLevelWorld");
    LevelAssetLoadParams LevelLoadParams{};
    LevelLoadParams.TargetWorld = &LoadedLevelWorld;
    LevelLoadParams.NameOverride = "LoadedLevel";
    auto LoadedLevelResult = Manager.Load<Level>("feature.level", LevelLoadParams);
    if (!LoadedLevelResult.has_value())
    {
        std::cerr << "Failed to load level from AssetManager: " << LoadedLevelResult.error() << std::endl;
        return 1;
    }

    NodeHandle LoadedLevelHandle = FindNodeByName(LoadedLevelWorld, "LoadedLevel");
    if (LoadedLevelHandle.IsNull())
    {
        LoadedLevelHandle = FindNodeByName(LoadedLevelWorld, "MainLevel");
    }
    auto* LoadedLevel = NodeCast<Level>(LoadedLevelHandle.Borrowed());
    if (!LoadedLevel)
    {
        std::cerr << "Failed to resolve loaded level destination" << std::endl;
        return 1;
    }

    auto* LevelGameplay = FindGraphByName(*LoadedLevel, "Gameplay");
    if (!LevelGameplay)
    {
        std::cerr << "Loaded level missing Gameplay graph" << std::endl;
        return 1;
    }
    if (!ValidateDemoNode(
            *LevelGameplay,
            "Player",
            "Target",
            150,
            4.5f,
            "Hero",
            Vec3(1.0f, 2.0f, 3.0f),
            9001,
            "Ranger",
            Vec3(0.2f, 0.6f, 1.0f)))
    {
        return 1;
    }

    World LoadedPrefabWorld("LoadedPrefabWorld");
    auto PrefabLevelHandleResult = LoadedPrefabWorld.CreateLevel("PrefabLevel");
    if (!PrefabLevelHandleResult)
    {
        std::cerr << "Failed to create prefab validation level" << std::endl;
        return 1;
    }
    auto* PrefabLevel = NodeCast<Level>(PrefabLevelHandleResult->Borrowed());
    if (!PrefabLevel)
    {
        std::cerr << "Failed to resolve prefab validation level" << std::endl;
        return 1;
    }

    NodeAssetLoadParams NodeLoadParams{};
    NodeLoadParams.TargetWorld = &LoadedPrefabWorld;
    NodeLoadParams.Parent = PrefabLevel->Handle();
    auto LoadedNodeResult = Manager.Load<BaseNode>("feature.graph", NodeLoadParams);
    if (!LoadedNodeResult.has_value())
    {
        std::cerr << "Failed to load node from AssetManager: " << LoadedNodeResult.error() << std::endl;
        return 1;
    }

    if (!ValidateDemoNode(
            *PrefabLevel,
            "PrefabActor",
            "PrefabTarget",
            60,
            2.5f,
            "Prefab",
            Vec3(5.0f, 6.0f, 7.0f),
            777,
            "PrefabComponent",
            Vec3(1.0f, 0.4f, 0.1f)))
    {
        return 1;
    }

    WorldInstance.Tick(0.016f);
    WorldInstance.EndFrame();

    std::cout << "FeatureShowcase ran successfully" << std::endl;
    return 0;
}
