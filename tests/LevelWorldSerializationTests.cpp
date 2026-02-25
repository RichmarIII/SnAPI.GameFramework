#include <unordered_map>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

TEST_CASE("Level serialization round-trips with nested graphs")
{
    RegisterBuiltinTypes();

    Level LevelRef("MainLevel");
    auto GraphHandleResult = LevelRef.CreateNode<Level>("Gameplay");
    REQUIRE(GraphHandleResult);

    auto* Graph = dynamic_cast<Level*>(GraphHandleResult.value().Borrowed());
    REQUIRE(Graph != nullptr);

    auto NodeResult = Graph->CreateNode("Hero");
    REQUIRE(NodeResult);
    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);

    auto TransformResult = Node->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3(7.0f, 8.0f, 9.0f);

    auto PayloadResult = LevelSerializer::Serialize(LevelRef);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes;
    REQUIRE(SerializeLevelPayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeLevelPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    Level LoadedLevel;
    REQUIRE(LevelSerializer::Deserialize(PayloadRoundTrip.value(), LoadedLevel));

    Level* LoadedGraph = nullptr;
    LoadedLevel.NodePool().ForEach([&](const NodeHandle&, BaseNode& NodeRef) {
        if (NodeRef.Name() == "Gameplay")
        {
            LoadedGraph = dynamic_cast<Level*>(&NodeRef);
        }
    });
    REQUIRE(LoadedGraph != nullptr);

    std::unordered_map<std::string, NodeHandle> NodesByName;
    LoadedGraph->NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& NodeRef) {
        NodesByName.emplace(NodeRef.Name(), Handle);
    });

    REQUIRE(NodesByName.find("Hero") != NodesByName.end());
    auto* LoadedNode = NodesByName["Hero"].Borrowed();
    REQUIRE(LoadedNode != nullptr);

    auto LoadedTransform = LoadedNode->Component<TransformComponent>();
    REQUIRE(LoadedTransform);
    REQUIRE(LoadedTransform->Position.x() == Catch::Approx(7.0f));
    REQUIRE(LoadedTransform->Position.y() == Catch::Approx(8.0f));
    REQUIRE(LoadedTransform->Position.z() == Catch::Approx(9.0f));
}

TEST_CASE("World serialization round-trips levels")
{
    RegisterBuiltinTypes();

    World WorldRef;
    WorldRef.Name("TestWorld");

    auto LevelHandleResult = WorldRef.CreateLevel("LevelOne");
    REQUIRE(LevelHandleResult);
    auto LevelResult = WorldRef.LevelRef(LevelHandleResult.value());
    REQUIRE(LevelResult);
    auto& LevelRef = *LevelResult;

    auto NodeResult = LevelRef.CreateNode("NodeA");
    REQUIRE(NodeResult);
    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);
    auto TransformResult = Node->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3(1.0f, 2.0f, 3.0f);

    auto PayloadResult = WorldSerializer::Serialize(WorldRef);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes;
    REQUIRE(SerializeWorldPayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeWorldPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld;
    REQUIRE(WorldSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld));
    REQUIRE(LoadedWorld.Name() == "TestWorld");
    auto LoadedLevels = LoadedWorld.Levels();
    REQUIRE(LoadedLevels.size() == 1);
    auto* LoadedLevel = LoadedLevels.front().Borrowed();
    REQUIRE(LoadedLevel != nullptr);
    REQUIRE(LoadedLevel->Name() == "LevelOne");
}
