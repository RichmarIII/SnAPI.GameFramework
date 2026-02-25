#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;

namespace
{
BaseNode* ResolveNodeHandle(const NodeHandle& Handle, const IWorld* WorldRef)
{
    if (BaseNode* Node = Handle.Borrowed())
    {
        return Node;
    }
    if (WorldRef && !Handle.Id.is_nil())
    {
        if (auto Rehydrated = WorldRef->NodeHandleById(Handle.Id); Rehydrated)
        {
            return Rehydrated->Borrowed();
        }
    }
    return Handle.BorrowedSlowByUuid();
}

BaseNode* FindNodeByNameInSubtree(const BaseNode& Root, const std::string& Name)
{
    if (Root.Name() == Name)
    {
        return const_cast<BaseNode*>(&Root);
    }

    const IWorld* WorldRef = Root.World();
    for (const NodeHandle& ChildHandle : Root.Children())
    {
        BaseNode* Child = ResolveNodeHandle(ChildHandle, WorldRef);
        if (!Child)
        {
            continue;
        }

        if (BaseNode* Found = FindNodeByNameInSubtree(*Child, Name))
        {
            return Found;
        }
    }

    return nullptr;
}

Level* FindLevelByName(const World& WorldRef, const std::string& Name)
{
    Level* Found = nullptr;
    WorldRef.NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        if (!Found && Node.Name() == Name)
        {
            Found = NodeCast<Level>(&Node);
        }
    });
    return Found;
}
} // namespace

TEST_CASE("Level serialization round-trips nested levels")
{
    RegisterBuiltinTypes();

    World SourceWorld("SourceWorld");
    auto MainLevelHandle = SourceWorld.CreateLevel("MainLevel");
    REQUIRE(MainLevelHandle);

    auto* MainLevel = NodeCast<Level>(MainLevelHandle->Borrowed());
    REQUIRE(MainLevel != nullptr);

    auto GameplayHandle = MainLevel->CreateNode<Level>("Gameplay");
    REQUIRE(GameplayHandle);
    auto* GameplayLevel = NodeCast<Level>(GameplayHandle->Borrowed());
    REQUIRE(GameplayLevel != nullptr);

    auto HeroHandle = GameplayLevel->CreateNode("Hero");
    REQUIRE(HeroHandle);
    auto* Hero = HeroHandle->Borrowed();
    REQUIRE(Hero != nullptr);

    auto TransformResult = Hero->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3(7.0f, 8.0f, 9.0f);

    auto PayloadResult = LevelSerializer::Serialize(*MainLevel);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeLevelPayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeLevelPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld("LoadedWorld");
    auto LoadedLevelHandle = LoadedWorld.CreateLevel("LoadedMain");
    REQUIRE(LoadedLevelHandle);
    auto* LoadedLevel = NodeCast<Level>(LoadedLevelHandle->Borrowed());
    REQUIRE(LoadedLevel != nullptr);

    REQUIRE(LevelSerializer::Deserialize(PayloadRoundTrip.value(), *LoadedLevel));

    BaseNode* GameplayNode = FindNodeByNameInSubtree(*LoadedLevel, "Gameplay");
    REQUIRE(GameplayNode != nullptr);
    auto* LoadedGameplay = NodeCast<Level>(GameplayNode);
    REQUIRE(LoadedGameplay != nullptr);

    BaseNode* HeroNode = FindNodeByNameInSubtree(*LoadedGameplay, "Hero");
    REQUIRE(HeroNode != nullptr);

    auto LoadedTransform = HeroNode->Component<TransformComponent>();
    REQUIRE(LoadedTransform);
    REQUIRE(LoadedTransform->Position.x() == Catch::Approx(7.0f));
    REQUIRE(LoadedTransform->Position.y() == Catch::Approx(8.0f));
    REQUIRE(LoadedTransform->Position.z() == Catch::Approx(9.0f));
}

TEST_CASE("World serialization round-trips levels")
{
    RegisterBuiltinTypes();

    World SourceWorld("TestWorld");

    auto LevelHandleResult = SourceWorld.CreateLevel("LevelOne");
    REQUIRE(LevelHandleResult);

    auto LevelResult = SourceWorld.LevelRef(LevelHandleResult.value());
    REQUIRE(LevelResult);
    auto& LevelRef = *LevelResult;

    auto NodeResult = LevelRef.CreateNode("NodeA");
    REQUIRE(NodeResult);
    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);

    auto TransformResult = Node->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3(1.0f, 2.0f, 3.0f);

    auto PayloadResult = WorldSerializer::Serialize(SourceWorld);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeWorldPayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeWorldPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld;
    REQUIRE(WorldSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld));
    REQUIRE(LoadedWorld.Name() == "TestWorld");

    Level* LoadedLevel = FindLevelByName(LoadedWorld, "LevelOne");
    REQUIRE(LoadedLevel != nullptr);

    BaseNode* LoadedNode = FindNodeByNameInSubtree(*LoadedLevel, "NodeA");
    REQUIRE(LoadedNode != nullptr);

    auto LoadedTransform = LoadedNode->Component<TransformComponent>();
    REQUIRE(LoadedTransform);
    REQUIRE(LoadedTransform->Position.x() == Catch::Approx(1.0f));
    REQUIRE(LoadedTransform->Position.y() == Catch::Approx(2.0f));
    REQUIRE(LoadedTransform->Position.z() == Catch::Approx(3.0f));
}

TEST_CASE("Level deserialization can regenerate UUIDs for repeated instantiation")
{
    RegisterBuiltinTypes();

    World SourceWorld("SourceWorld");
    auto SourceLevelHandle = SourceWorld.CreateLevel("SourceLevel");
    REQUIRE(SourceLevelHandle);

    auto SourceLevelResult = SourceWorld.LevelRef(SourceLevelHandle.value());
    REQUIRE(SourceLevelResult);
    auto& SourceLevel = *SourceLevelResult;

    auto SourceNodeResult = SourceLevel.CreateNode("NodeA");
    REQUIRE(SourceNodeResult);
    const Uuid SourceNodeId = SourceNodeResult->Id;

    auto PayloadResult = LevelSerializer::Serialize(SourceLevel);
    REQUIRE(PayloadResult);

    World LoadedWorld("LoadedWorld");
    auto FirstLevelHandle = LoadedWorld.CreateLevel("First");
    REQUIRE(FirstLevelHandle);
    auto* FirstLevel = NodeCast<Level>(FirstLevelHandle->Borrowed());
    REQUIRE(FirstLevel != nullptr);
    REQUIRE(LevelSerializer::Deserialize(PayloadResult.value(), *FirstLevel));

    BaseNode* FirstNode = FindNodeByNameInSubtree(*FirstLevel, "NodeA");
    REQUIRE(FirstNode != nullptr);
    REQUIRE(FirstNode->Id() == SourceNodeId);

    auto SecondLevelHandle = LoadedWorld.CreateLevel("Second");
    REQUIRE(SecondLevelHandle);
    auto* SecondLevel = NodeCast<Level>(SecondLevelHandle->Borrowed());
    REQUIRE(SecondLevel != nullptr);

    TDeserializeOptions CopyOptions{};
    CopyOptions.RegenerateObjectIds = true;
    REQUIRE(LevelSerializer::Deserialize(PayloadResult.value(), *SecondLevel, CopyOptions));

    BaseNode* SecondNode = FindNodeByNameInSubtree(*SecondLevel, "NodeA");
    REQUIRE(SecondNode != nullptr);
    REQUIRE(SecondNode->Id() != SourceNodeId);
    REQUIRE(SecondNode->Id() != FirstNode->Id());
}
