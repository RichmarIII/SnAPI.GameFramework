#include <algorithm>
#include <sstream>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "NodeCast.h"

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

/**
 * @brief Component containing a component-handle link for serialization remap tests.
 */
struct ComponentLinkComponent : public BaseComponent, public ComponentCRTP<ComponentLinkComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ComponentLinkComponent";
    ComponentHandle TargetComponent{};
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
 * @brief Cross-world handle component used for reference remap tests.
 */
struct CrossRefComponent : public BaseComponent, public ComponentCRTP<CrossRefComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CrossRefComponent";
    NodeHandle Target{};
};

struct ReflectionNotifyNode : public BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ReflectionNotifyNode";
    int Value = 0;
    int PropertyChangeCount = 0;
    std::string LastChangedProperty{};

    void EditorOnPropertyChanged(const std::string_view Name)
    {
        ++PropertyChangeCount;
        LastChangedProperty = std::string(Name);
    }
};

NodeHandle FindNodeByName(const World& WorldRef, const std::string& Name)
{
    NodeHandle Found{};
    WorldRef.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Name() == Name)
        {
            Found = Handle;
        }
    });
    return Found;
}
} // namespace

SNAPI_REFLECT_TYPE(LinkComponent, (TTypeBuilder<LinkComponent>(LinkComponent::kTypeName)
    .Field("Target", &LinkComponent::Target)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(ComponentLinkComponent, (TTypeBuilder<ComponentLinkComponent>(ComponentLinkComponent::kTypeName)
    .Field("TargetComponent", &ComponentLinkComponent::TargetComponent)
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

SNAPI_REFLECT_TYPE(ReflectionNotifyNode, (TTypeBuilder<ReflectionNotifyNode>(ReflectionNotifyNode::kTypeName)
    .Base<BaseNode>()
    .Field("Value", &ReflectionNotifyNode::Value)
    .Constructor<>()
    .Register()));

TEST_CASE("Node serialization round-trips subtree with components and handles")
{
    RegisterBuiltinTypes();

    World SourceWorld("Source");
    auto AResult = SourceWorld.CreateNode("A");
    REQUIRE(AResult);
    auto BResult = SourceWorld.CreateNode("B");
    REQUIRE(BResult);
    REQUIRE(SourceWorld.AttachChild(AResult.value(), BResult.value()));

    auto* NodeA = AResult.value().Borrowed();
    REQUIRE(NodeA != nullptr);

    auto TransformResult = NodeA->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3(4.0f, 5.0f, 6.0f);

    auto LinkResult = NodeA->Add<LinkComponent>();
    REQUIRE(LinkResult);
    LinkResult->Target = BResult.value();

    auto PayloadResult = NodeSerializer::Serialize(*NodeA);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeNodePayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeNodePayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld("Loaded");
    auto DeserializeResult = NodeSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld);
    REQUIRE(DeserializeResult);

    NodeHandle LoadedAHandle = FindNodeByName(LoadedWorld, "A");
    NodeHandle LoadedBHandle = FindNodeByName(LoadedWorld, "B");
    REQUIRE(LoadedAHandle.IsValid());
    REQUIRE(LoadedBHandle.IsValid());

    auto* LoadedA = LoadedAHandle.Borrowed();
    REQUIRE(LoadedA != nullptr);
    auto LoadedTransform = LoadedA->Component<TransformComponent>();
    REQUIRE(LoadedTransform);
    REQUIRE(LoadedTransform->Position.x() == Catch::Approx(4.0f));
    REQUIRE(LoadedTransform->Position.y() == Catch::Approx(5.0f));
    REQUIRE(LoadedTransform->Position.z() == Catch::Approx(6.0f));

    auto LoadedLink = LoadedA->Component<LinkComponent>();
    REQUIRE(LoadedLink);
    auto* LinkedNode = LoadedLink->Target.Borrowed();
    REQUIRE(LinkedNode != nullptr);
    REQUIRE(LinkedNode->Name() == "B");
}

TEST_CASE("Node serialization preserves logical scheme paths")
{
    RegisterBuiltinTypes();

    World SourceWorld("Source");
    auto NodeResult = SourceWorld.CreateNode("SchemeNode");
    REQUIRE(NodeResult);

    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);

    auto ScriptResult = Node->Add<ScriptComponent>();
    REQUIRE(ScriptResult);
    ScriptResult->ScriptModule = "asset://scripts/example.lua";
    ScriptResult->ScriptType = "ExampleMain";

#if defined(SNAPI_GF_ENABLE_AUDIO)
    auto AudioResult = Node->Add<AudioSourceComponent>();
    REQUIRE(AudioResult);
    auto& AudioSettings = AudioResult->EditSettings();
    AudioSettings.SoundPath = "asset://audio/example.wav";
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto StaticMeshResult = Node->Add<StaticMeshComponent>();
    REQUIRE(StaticMeshResult);
    auto& StaticMeshSettings = StaticMeshResult->EditSettings();
    StaticMeshSettings.MeshPath = "asset://meshes/example.mesh";

    auto SkeletalMeshResult = Node->Add<SkeletalMeshComponent>();
    REQUIRE(SkeletalMeshResult);
    auto& SkeletalMeshSettings = SkeletalMeshResult->EditSettings();
    SkeletalMeshSettings.MeshPath = "asset://meshes/example_skeletal.mesh";
#endif

    auto PayloadResult = NodeSerializer::Serialize(*Node);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeNodePayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeNodePayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld("Loaded");
    auto DeserializeResult = NodeSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld);
    REQUIRE(DeserializeResult);

    NodeHandle LoadedNodeHandle = FindNodeByName(LoadedWorld, "SchemeNode");
    REQUIRE(LoadedNodeHandle.IsValid());
    auto* LoadedNode = LoadedNodeHandle.Borrowed();
    REQUIRE(LoadedNode != nullptr);

    auto LoadedScript = LoadedNode->Component<ScriptComponent>();
    REQUIRE(LoadedScript);
    REQUIRE(LoadedScript->ScriptModule == "asset://scripts/example.lua");
    REQUIRE(LoadedScript->ScriptType == "ExampleMain");

#if defined(SNAPI_GF_ENABLE_AUDIO)
    auto LoadedAudio = LoadedNode->Component<AudioSourceComponent>();
    REQUIRE(LoadedAudio);
    REQUIRE(LoadedAudio->GetSettings().SoundPath == "asset://audio/example.wav");
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto LoadedStaticMesh = LoadedNode->Component<StaticMeshComponent>();
    REQUIRE(LoadedStaticMesh);
    REQUIRE(LoadedStaticMesh->GetSettings().MeshPath == "asset://meshes/example.mesh");

    auto LoadedSkeletalMesh = LoadedNode->Component<SkeletalMeshComponent>();
    REQUIRE(LoadedSkeletalMesh);
    REQUIRE(LoadedSkeletalMesh->GetSettings().MeshPath == "asset://meshes/example_skeletal.mesh");
#endif
}

#if defined(WITH_EDITOR) && WITH_EDITOR
TEST_CASE("Reflection field setter invokes editor property callback only when value changes")
{
    RegisterBuiltinTypes();

    World WorldInstance{"ReflectionNotifyWorld"};
    auto NodeResult = WorldInstance.CreateNode<ReflectionNotifyNode>("Notify");
    REQUIRE(NodeResult);

    auto* Node = NodeCast<ReflectionNotifyNode>(NodeResult->Borrowed());
    REQUIRE(Node != nullptr);

    const TypeInfo* Type = TypeRegistry::Instance().Find(StaticTypeId<ReflectionNotifyNode>());
    REQUIRE(Type != nullptr);

    const auto FieldIt = std::find_if(Type->Fields.begin(), Type->Fields.end(), [](const FieldInfo& Field) {
        return Field.Name == "Value";
    });
    REQUIRE(FieldIt != Type->Fields.end());
    REQUIRE(FieldIt->Setter);

    REQUIRE(FieldIt->Setter(Node, Variant::FromValue(5)));
    REQUIRE(Node->Value == 5);
    REQUIRE(Node->PropertyChangeCount == 1);
    REQUIRE(Node->LastChangedProperty == "Value");

    REQUIRE(FieldIt->Setter(Node, Variant::FromValue(5)));
    REQUIRE(Node->Value == 5);
    REQUIRE(Node->PropertyChangeCount == 1);

    REQUIRE(FieldIt->Setter(Node, Variant::FromValue(8)));
    REQUIRE(Node->Value == 8);
    REQUIRE(Node->PropertyChangeCount == 2);
    REQUIRE(Node->LastChangedProperty == "Value");
}
#endif

TEST_CASE("Node serialization round-trips node fields across inheritance")
{
    RegisterBuiltinTypes();

    World SourceWorld("Source");
    auto TargetResult = SourceWorld.CreateNode("Target");
    REQUIRE(TargetResult);

    auto ActorResult = SourceWorld.CreateNode<DerivedStatsNode>("Actor");
    REQUIRE(ActorResult);

    auto* Actor = NodeCast<DerivedStatsNode>(ActorResult.value().Borrowed());
    REQUIRE(Actor != nullptr);
    Actor->m_baseValue = 7;
    Actor->m_health = 42;
    Actor->m_spawn = Vec3(1.0f, 2.0f, 3.0f);
    Actor->m_target = TargetResult.value();

    auto PayloadResult = NodeSerializer::Serialize(*Actor);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeNodePayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeNodePayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld("Loaded");
    auto DeserializeResult = NodeSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld);
    REQUIRE(DeserializeResult);

    NodeHandle LoadedActorHandle = FindNodeByName(LoadedWorld, "Actor");
    REQUIRE(LoadedActorHandle.IsValid());

    auto* LoadedActor = NodeCast<DerivedStatsNode>(LoadedActorHandle.Borrowed());
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

TEST_CASE("Cross-world node handles use explicit UUID slow resolve after deserialization")
{
    RegisterBuiltinTypes();

    std::vector<uint8_t> OwnerBytes{};
    std::vector<uint8_t> TargetBytes{};

    {
        World SourceA("SourceA");
        World SourceB("SourceB");

        auto TargetResult = SourceB.CreateNode("TargetNode");
        REQUIRE(TargetResult);

        auto OwnerResult = SourceA.CreateNode("OwnerNode");
        REQUIRE(OwnerResult);
        auto* OwnerNode = OwnerResult.value().Borrowed();
        REQUIRE(OwnerNode != nullptr);

        auto RefResult = OwnerNode->Add<CrossRefComponent>();
        REQUIRE(RefResult);
        RefResult->Target = TargetResult.value();

        auto OwnerPayload = NodeSerializer::Serialize(*OwnerNode);
        REQUIRE(OwnerPayload);

        auto* TargetNode = TargetResult.value().Borrowed();
        REQUIRE(TargetNode != nullptr);
        auto TargetPayload = NodeSerializer::Serialize(*TargetNode);
        REQUIRE(TargetPayload);

        REQUIRE(SerializeNodePayload(OwnerPayload.value(), OwnerBytes));
        REQUIRE(SerializeNodePayload(TargetPayload.value(), TargetBytes));
    }

    World LoadedA("LoadedA");
    World LoadedB("LoadedB");

    auto OwnerRoundTrip = DeserializeNodePayload(OwnerBytes.data(), OwnerBytes.size());
    REQUIRE(OwnerRoundTrip);
    auto TargetRoundTrip = DeserializeNodePayload(TargetBytes.data(), TargetBytes.size());
    REQUIRE(TargetRoundTrip);

    REQUIRE(NodeSerializer::Deserialize(OwnerRoundTrip.value(), LoadedA));

    NodeHandle LoadedOwner = FindNodeByName(LoadedA, "OwnerNode");
    REQUIRE(LoadedOwner.IsValid());
    auto* LoadedOwnerNode = LoadedOwner.Borrowed();
    REQUIRE(LoadedOwnerNode != nullptr);

    auto LoadedRef = LoadedOwnerNode->Component<CrossRefComponent>();
    REQUIRE(LoadedRef);
    REQUIRE(LoadedRef->Target.Borrowed() == nullptr);

    REQUIRE(NodeSerializer::Deserialize(TargetRoundTrip.value(), LoadedB));

    auto* ResolvedTarget = LoadedRef->Target.BorrowedSlowByUuid();
    REQUIRE(ResolvedTarget != nullptr);
    REQUIRE(ResolvedTarget->Name() == "TargetNode");

    auto RehydratedHandle = LoadedB.NodeHandleById(LoadedRef->Target.Id);
    REQUIRE(RehydratedHandle);
    LoadedRef->Target = *RehydratedHandle;
    REQUIRE(LoadedRef->Target.Borrowed() == ResolvedTarget);
}

TEST_CASE("Node deserialization can regenerate object UUIDs and remap handles")
{
    RegisterBuiltinTypes();

    World SourceWorld("Source");
    auto OwnerResult = SourceWorld.CreateNode("Owner");
    REQUIRE(OwnerResult);
    auto TargetResult = SourceWorld.CreateNode("Target");
    REQUIRE(TargetResult);
    REQUIRE(SourceWorld.AttachChild(OwnerResult.value(), TargetResult.value()));

    auto* OwnerNode = OwnerResult.value().Borrowed();
    REQUIRE(OwnerNode != nullptr);
    auto* TargetNode = TargetResult.value().Borrowed();
    REQUIRE(TargetNode != nullptr);

    auto TargetTransform = TargetNode->Add<TransformComponent>();
    REQUIRE(TargetTransform);

    auto OwnerLink = OwnerNode->Add<LinkComponent>();
    REQUIRE(OwnerLink);
    OwnerLink->Target = TargetResult.value();

    auto OwnerComponentLink = OwnerNode->Add<ComponentLinkComponent>();
    REQUIRE(OwnerComponentLink);
    OwnerComponentLink->TargetComponent = TargetTransform->Handle();

    const Uuid SourceOwnerId = OwnerNode->Id();
    const Uuid SourceTargetId = TargetNode->Id();
    const Uuid SourceTransformId = TargetTransform->Id();
    const Uuid SourceLinkId = OwnerLink->Id();
    const Uuid SourceComponentLinkId = OwnerComponentLink->Id();

    auto PayloadResult = NodeSerializer::Serialize(*OwnerNode);
    REQUIRE(PayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeNodePayload(PayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto PayloadRoundTrip = DeserializeNodePayload(Bytes.data(), Bytes.size());
    REQUIRE(PayloadRoundTrip);

    World LoadedWorld("Loaded");

    auto FirstResult = NodeSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld);
    REQUIRE(FirstResult);
    REQUIRE(FirstResult->Id == SourceOwnerId);

    TDeserializeOptions CopyOptions{};
    CopyOptions.RegenerateObjectIds = true;
    auto SecondResult = NodeSerializer::Deserialize(PayloadRoundTrip.value(), LoadedWorld, {}, CopyOptions);
    REQUIRE(SecondResult);
    REQUIRE(SecondResult->Id != SourceOwnerId);
    REQUIRE(SecondResult->Id != FirstResult->Id);

    auto* SecondOwner = SecondResult->Borrowed();
    REQUIRE(SecondOwner != nullptr);
    REQUIRE(SecondOwner->Children().size() == 1);
    NodeHandle SecondTargetHandle = SecondOwner->Children().front();
    auto* SecondTarget = SecondTargetHandle.Borrowed();
    REQUIRE(SecondTarget != nullptr);

    auto SecondLink = SecondOwner->Component<LinkComponent>();
    REQUIRE(SecondLink);
    REQUIRE(SecondLink->Id() != SourceLinkId);
    REQUIRE(SecondLink->Target.Id == SecondTarget->Id());
    REQUIRE(SecondLink->Target.Borrowed() == SecondTarget);

    auto SecondComponentLink = SecondOwner->Component<ComponentLinkComponent>();
    REQUIRE(SecondComponentLink);
    REQUIRE(SecondComponentLink->Id() != SourceComponentLinkId);

    auto SecondTargetTransform = SecondTarget->Component<TransformComponent>();
    REQUIRE(SecondTargetTransform);
    REQUIRE(SecondTarget->Id() != SourceTargetId);
    REQUIRE(SecondTargetTransform->Id() != SourceTransformId);

    BaseComponent* LinkedComponent = SecondComponentLink->TargetComponent.Borrowed();
    REQUIRE(LinkedComponent != nullptr);
    REQUIRE(LinkedComponent->Id() == SecondTargetTransform->Id());
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
