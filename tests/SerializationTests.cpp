#include <unordered_map>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

struct LinkComponent : public IComponent
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::LinkComponent";
    NodeHandle Target{};
};

namespace
{
struct BaseStatsNode : public BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BaseStatsNode";
    int m_baseValue = 0;
};

struct DerivedStatsNode : public BaseStatsNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DerivedStatsNode";
    int m_health = 0;
    Vec3 m_spawn{};
    NodeHandle m_target{};
};

struct CrossRefComponent : public IComponent
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CrossRefComponent";
    NodeHandle Target{};
};
} // namespace

TEST_CASE("NodeGraph serialization round-trips with components and handles")
{
    RegisterBuiltinTypes();

    (void)TTypeBuilder<LinkComponent>(LinkComponent::kTypeName)
        .Field("Target", &LinkComponent::Target)
        .Constructor<>()
        .Register();

    ComponentSerializationRegistry::Instance().Register<LinkComponent>();

    NodeGraph Graph;
    auto AResult = Graph.CreateNode("A");
    REQUIRE(AResult);
    auto BResult = Graph.CreateNode("B");
    REQUIRE(BResult);
    REQUIRE(Graph.AttachChild(AResult.value(), BResult.value()));

    auto* NodeA = AResult.value().Borrowed();
    REQUIRE(NodeA != nullptr);
    auto* NodeB = BResult.value().Borrowed();
    REQUIRE(NodeB != nullptr);

    auto TransformResult = NodeA->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3(4.0f, 5.0f, 6.0f);

    auto LinkResult = NodeA->Add<LinkComponent>();
    REQUIRE(LinkResult);
    LinkResult->Target = BResult.value();

    auto ScriptResult = NodeB->Add<ScriptComponent>();
    REQUIRE(ScriptResult);
    ScriptResult->ScriptModule = "Player.lua";
    ScriptResult->ScriptType = "PlayerController";
    ScriptResult->Instance = 42;

    auto PayloadResult = NodeGraphSerializer::Serialize(Graph);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes;
    auto BytesResult = SerializeNodeGraphPayload(PayloadResult.value(), Bytes);
    REQUIRE(BytesResult);
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeNodeGraphPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    NodeGraph Graph2;
    auto LoadResult = NodeGraphSerializer::Deserialize(PayloadRoundTrip.value(), Graph2);
    REQUIRE(LoadResult);

    std::unordered_map<std::string, NodeHandle> NodesByName;
    Graph2.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        NodesByName.emplace(Node.Name(), Handle);
    });

    REQUIRE(NodesByName.size() == 2);
    REQUIRE(NodesByName.find("A") != NodesByName.end());
    REQUIRE(NodesByName.find("B") != NodesByName.end());

    auto* LoadedA = NodesByName["A"].Borrowed();
    REQUIRE(LoadedA != nullptr);
    auto TransformLoaded = LoadedA->Component<TransformComponent>();
    REQUIRE(TransformLoaded);
    REQUIRE(TransformLoaded->Position.X == Catch::Approx(4.0f));
    REQUIRE(TransformLoaded->Position.Y == Catch::Approx(5.0f));
    REQUIRE(TransformLoaded->Position.Z == Catch::Approx(6.0f));

    auto LinkLoaded = LoadedA->Component<LinkComponent>();
    REQUIRE(LinkLoaded);
    auto* LinkedNode = LinkLoaded->Target.Borrowed();
    REQUIRE(LinkedNode != nullptr);
    REQUIRE(LinkedNode->Name() == "B");
}

TEST_CASE("NodeGraph serialization round-trips node fields across inheritance")
{
    RegisterBuiltinTypes();

    (void)TTypeBuilder<BaseStatsNode>(BaseStatsNode::kTypeName)
        .Base<BaseNode>()
        .Field("BaseValue", &BaseStatsNode::m_baseValue)
        .Constructor<>()
        .Register();

    (void)TTypeBuilder<DerivedStatsNode>(DerivedStatsNode::kTypeName)
        .Base<BaseStatsNode>()
        .Field("Health", &DerivedStatsNode::m_health)
        .Field("Spawn", &DerivedStatsNode::m_spawn)
        .Field("Target", &DerivedStatsNode::m_target)
        .Constructor<>()
        .Register();

    NodeGraph Graph;
    auto TargetResult = Graph.CreateNode("Target");
    REQUIRE(TargetResult);

    auto ActorResult = Graph.CreateNode<DerivedStatsNode>("Actor");
    REQUIRE(ActorResult);

    auto* Actor = dynamic_cast<DerivedStatsNode*>(ActorResult.value().Borrowed());
    REQUIRE(Actor != nullptr);
    Actor->m_baseValue = 7;
    Actor->m_health = 42;
    Actor->m_spawn = Vec3(1.0f, 2.0f, 3.0f);
    Actor->m_target = TargetResult.value();

    auto PayloadResult = NodeGraphSerializer::Serialize(Graph);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes;
    auto BytesResult = SerializeNodeGraphPayload(PayloadResult.value(), Bytes);
    REQUIRE(BytesResult);
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeNodeGraphPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    NodeGraph Graph2;
    auto LoadResult = NodeGraphSerializer::Deserialize(PayloadRoundTrip.value(), Graph2);
    REQUIRE(LoadResult);

    NodeHandle LoadedActorHandle;
    Graph2.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Name() == "Actor")
        {
            LoadedActorHandle = Handle;
        }
    });

    auto* LoadedActor = dynamic_cast<DerivedStatsNode*>(LoadedActorHandle.Borrowed());
    REQUIRE(LoadedActor != nullptr);
    REQUIRE(LoadedActor->m_baseValue == 7);
    REQUIRE(LoadedActor->m_health == 42);
    REQUIRE(LoadedActor->m_spawn.X == Catch::Approx(1.0f));
    REQUIRE(LoadedActor->m_spawn.Y == Catch::Approx(2.0f));
    REQUIRE(LoadedActor->m_spawn.Z == Catch::Approx(3.0f));

    auto* LoadedTarget = LoadedActor->m_target.Borrowed();
    REQUIRE(LoadedTarget != nullptr);
    REQUIRE(LoadedTarget->Name() == "Target");
}

TEST_CASE("Node handles resolve across graphs after deserialization")
{
    RegisterBuiltinTypes();

    (void)TTypeBuilder<CrossRefComponent>(CrossRefComponent::kTypeName)
        .Field("Target", &CrossRefComponent::Target)
        .Constructor<>()
        .Register();

    ComponentSerializationRegistry::Instance().Register<CrossRefComponent>();

    NodeGraph GraphA("GraphA");
    NodeGraph GraphB("GraphB");

    auto TargetResult = GraphB.CreateNode("TargetNode");
    REQUIRE(TargetResult);

    auto OwnerResult = GraphA.CreateNode("OwnerNode");
    REQUIRE(OwnerResult);
    auto* OwnerNode = OwnerResult.value().Borrowed();
    REQUIRE(OwnerNode != nullptr);

    auto RefResult = OwnerNode->Add<CrossRefComponent>();
    REQUIRE(RefResult);
    RefResult->Target = TargetResult.value();

    auto PayloadA = NodeGraphSerializer::Serialize(GraphA);
    REQUIRE(PayloadA);
    auto PayloadB = NodeGraphSerializer::Serialize(GraphB);
    REQUIRE(PayloadB);

    std::vector<uint8_t> BytesA;
    std::vector<uint8_t> BytesB;
    REQUIRE(SerializeNodeGraphPayload(PayloadA.value(), BytesA));
    REQUIRE(SerializeNodeGraphPayload(PayloadB.value(), BytesB));

    GraphA.Clear();
    GraphB.Clear();

    NodeGraph LoadedA("LoadedA");
    NodeGraph LoadedB("LoadedB");

    auto PayloadARoundTrip = DeserializeNodeGraphPayload(BytesA.data(), BytesA.size());
    REQUIRE(PayloadARoundTrip);
    auto PayloadBRoundTrip = DeserializeNodeGraphPayload(BytesB.data(), BytesB.size());
    REQUIRE(PayloadBRoundTrip);

    REQUIRE(NodeGraphSerializer::Deserialize(PayloadARoundTrip.value(), LoadedA));

    NodeHandle LoadedOwner;
    LoadedA.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Name() == "OwnerNode")
        {
            LoadedOwner = Handle;
        }
    });
    REQUIRE(LoadedOwner.IsValid());
    auto* LoadedOwnerNode = LoadedOwner.Borrowed();
    REQUIRE(LoadedOwnerNode != nullptr);

    auto LoadedRef = LoadedOwnerNode->Component<CrossRefComponent>();
    REQUIRE(LoadedRef);
    REQUIRE(LoadedRef->Target.Borrowed() == nullptr);

    REQUIRE(NodeGraphSerializer::Deserialize(PayloadBRoundTrip.value(), LoadedB));

    auto* ResolvedTarget = LoadedRef->Target.Borrowed();
    REQUIRE(ResolvedTarget != nullptr);
    REQUIRE(ResolvedTarget->Name() == "TargetNode");
}
