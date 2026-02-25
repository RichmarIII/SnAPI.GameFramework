#include <sstream>
#include <unordered_map>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace SnAPI::GameFramework
{
/**
 * @brief Custom value type used to verify TValueCodec forwarding behavior.
 */
struct CustomPackedValue
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CustomPackedValue";
    int32_t A = 0;
    int32_t B = 0;
};

template<>
struct TValueCodec<CustomPackedValue>
{
    static TExpected<void> Encode(const CustomPackedValue& Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext&)
    {
        const int32_t Packed = Value.A + Value.B;
        Archive(Packed);
        return Ok();
    }

    static TExpected<CustomPackedValue> Decode(cereal::BinaryInputArchive& Archive, const TSerializationContext&)
    {
        int32_t Packed = 0;
        Archive(Packed);
        return CustomPackedValue{Packed, Packed + 1};
    }

    static TExpected<void> DecodeInto(CustomPackedValue& Value, cereal::BinaryInputArchive& Archive, const TSerializationContext&)
    {
        int32_t Packed = 0;
        Archive(Packed);
        Value.A = Packed;
        Value.B = Packed + 1;
        return Ok();
    }
};
} // namespace SnAPI::GameFramework

/**
 * @brief Simple component containing a node-handle link used for serialization tests.
 */
struct LinkComponent : public BaseComponent, public ComponentCRTP<LinkComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::LinkComponent";
    NodeHandle Target{};
};

namespace
{
/**
 * @brief Base node type used to validate inherited node field serialization.
 */
struct BaseStatsNode : public BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BaseStatsNode";
    int m_baseValue = 0;
};

/**
 * @brief Derived node type used to validate base+derived field round-trip behavior.
 */
struct DerivedStatsNode : public BaseStatsNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DerivedStatsNode";
    int m_health = 0;
    Vec3 m_spawn{};
    NodeHandle m_target{};
};

/**
 * @brief Cross-graph handle component used for reference remap tests.
 */
struct CrossRefComponent : public BaseComponent, public ComponentCRTP<CrossRefComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CrossRefComponent";
    NodeHandle Target{};
};
} // namespace

SNAPI_REFLECT_TYPE(LinkComponent, (TTypeBuilder<LinkComponent>(LinkComponent::kTypeName)
    .Field("Target", &LinkComponent::Target)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(BaseStatsNode, (TTypeBuilder<BaseStatsNode>(BaseStatsNode::kTypeName)
    .Base<BaseNode>()
    .Field("BaseValue", &BaseStatsNode::m_baseValue)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(DerivedStatsNode, (TTypeBuilder<DerivedStatsNode>(DerivedStatsNode::kTypeName)
    .Base<BaseStatsNode>()
    .Field("Health", &DerivedStatsNode::m_health)
    .Field("Spawn", &DerivedStatsNode::m_spawn)
    .Field("Target", &DerivedStatsNode::m_target)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(CrossRefComponent, (TTypeBuilder<CrossRefComponent>(CrossRefComponent::kTypeName)
    .Field("Target", &CrossRefComponent::Target)
    .Constructor<>()
    .Register()));

TEST_CASE("Level serialization round-trips with components and handles")
{
    RegisterBuiltinTypes();

    Level Graph;
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

    auto PayloadResult = LevelGraphSerializer::Serialize(Graph);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes;
    auto BytesResult = SerializeLevelGraphPayload(PayloadResult.value(), Bytes);
    REQUIRE(BytesResult);
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeLevelGraphPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    Level Graph2;
    auto LoadResult = LevelGraphSerializer::Deserialize(PayloadRoundTrip.value(), Graph2);
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
    REQUIRE(TransformLoaded->Position.x() == Catch::Approx(4.0f));
    REQUIRE(TransformLoaded->Position.y() == Catch::Approx(5.0f));
    REQUIRE(TransformLoaded->Position.z() == Catch::Approx(6.0f));

    auto LinkLoaded = LoadedA->Component<LinkComponent>();
    REQUIRE(LinkLoaded);
    auto* LinkedNode = LinkLoaded->Target.Borrowed();
    REQUIRE(LinkedNode != nullptr);
    REQUIRE(LinkedNode->Name() == "B");
}

TEST_CASE("Level serialization round-trips node fields across inheritance")
{
    RegisterBuiltinTypes();

    Level Graph;
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

    auto PayloadResult = LevelGraphSerializer::Serialize(Graph);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes;
    auto BytesResult = SerializeLevelGraphPayload(PayloadResult.value(), Bytes);
    REQUIRE(BytesResult);
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeLevelGraphPayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    Level Graph2;
    auto LoadResult = LevelGraphSerializer::Deserialize(PayloadRoundTrip.value(), Graph2);
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
    REQUIRE(LoadedActor->m_spawn.x() == Catch::Approx(1.0f));
    REQUIRE(LoadedActor->m_spawn.y() == Catch::Approx(2.0f));
    REQUIRE(LoadedActor->m_spawn.z() == Catch::Approx(3.0f));

    auto* LoadedTarget = LoadedActor->m_target.Borrowed();
    REQUIRE(LoadedTarget != nullptr);
    REQUIRE(LoadedTarget->Name() == "Target");
}

TEST_CASE("Cross-graph node handles use explicit UUID slow resolve after deserialization")
{
    RegisterBuiltinTypes();

    Level GraphA("GraphA");
    Level GraphB("GraphB");

    auto TargetResult = GraphB.CreateNode("TargetNode");
    REQUIRE(TargetResult);

    auto OwnerResult = GraphA.CreateNode("OwnerNode");
    REQUIRE(OwnerResult);
    auto* OwnerNode = OwnerResult.value().Borrowed();
    REQUIRE(OwnerNode != nullptr);

    auto RefResult = OwnerNode->Add<CrossRefComponent>();
    REQUIRE(RefResult);
    RefResult->Target = TargetResult.value();

    auto PayloadA = LevelGraphSerializer::Serialize(GraphA);
    REQUIRE(PayloadA);
    auto PayloadB = LevelGraphSerializer::Serialize(GraphB);
    REQUIRE(PayloadB);

    std::vector<uint8_t> BytesA;
    std::vector<uint8_t> BytesB;
    REQUIRE(SerializeLevelGraphPayload(PayloadA.value(), BytesA));
    REQUIRE(SerializeLevelGraphPayload(PayloadB.value(), BytesB));

    GraphA.Clear();
    GraphB.Clear();

    Level LoadedA("LoadedA");
    Level LoadedB("LoadedB");

    auto PayloadARoundTrip = DeserializeLevelGraphPayload(BytesA.data(), BytesA.size());
    REQUIRE(PayloadARoundTrip);
    auto PayloadBRoundTrip = DeserializeLevelGraphPayload(BytesB.data(), BytesB.size());
    REQUIRE(PayloadBRoundTrip);

    REQUIRE(LevelGraphSerializer::Deserialize(PayloadARoundTrip.value(), LoadedA));

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

    REQUIRE(LevelGraphSerializer::Deserialize(PayloadBRoundTrip.value(), LoadedB));

    auto* ResolvedTarget = LoadedRef->Target.BorrowedSlowByUuid();
    REQUIRE(ResolvedTarget != nullptr);
    REQUIRE(ResolvedTarget->Name() == "TargetNode");

    auto RehydratedHandle = LoadedB.NodeHandleByIdSlow(LoadedRef->Target.Id);
    REQUIRE(RehydratedHandle);
    LoadedRef->Target = RehydratedHandle.value();
    REQUIRE(LoadedRef->Target.Borrowed() == ResolvedTarget);
}

TEST_CASE("ValueCodecRegistry forwards to TValueCodec specializations")
{
    RegisterBuiltinTypes();

    auto& Registry = ValueCodecRegistry::Instance();
    Registry.Register<CustomPackedValue>();

    CustomPackedValue Input{4, 5};
    TSerializationContext Context;
    std::stringstream Stream(std::ios::in | std::ios::out | std::ios::binary);

    {
        cereal::BinaryOutputArchive Archive(Stream);
        auto EncodeResult = Registry.Encode(TypeIdFromName(TTypeNameV<CustomPackedValue>), &Input, Archive, Context);
        REQUIRE(EncodeResult);
    }

    Stream.seekg(0);
    {
        cereal::BinaryInputArchive Archive(Stream);
        auto DecodeResult = Registry.Decode(TypeIdFromName(TTypeNameV<CustomPackedValue>), Archive, Context);
        REQUIRE(DecodeResult);
        auto Value = DecodeResult->AsConstRef<CustomPackedValue>();
        REQUIRE(Value);
        REQUIRE(Value->get().A == 9);
        REQUIRE(Value->get().B == 10);
    }

    Stream.clear();
    Stream.seekg(0);
    {
        cereal::BinaryInputArchive Archive(Stream);
        CustomPackedValue Output{};
        auto DecodeIntoResult = Registry.DecodeInto(TypeIdFromName(TTypeNameV<CustomPackedValue>), &Output, Archive, Context);
        REQUIRE(DecodeIntoResult);
        REQUIRE(Output.A == 9);
        REQUIRE(Output.B == 10);
    }
}
