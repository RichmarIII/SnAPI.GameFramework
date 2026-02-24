#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

#include <SnAPI/Math/LinearAlgebra.h>

#include <cmath>

using namespace SnAPI::GameFramework;

namespace
{

Quat ZRotation(const Quat::Scalar Radians)
{
    return Quat(SnAPI::Math::AngleAxis3D(Radians, SnAPI::Math::Vector3::UnitZ()));
}

bool NearlyEqual(const Vec3& Left, const Vec3& Right, const double Epsilon = 1.0e-5)
{
    const auto Close = [Epsilon](const Vec3::Scalar A, const Vec3::Scalar B) {
        return std::abs(static_cast<double>(A - B)) <= Epsilon;
    };
    return Close(Left.x(), Right.x()) && Close(Left.y(), Right.y()) && Close(Left.z(), Right.z());
}

bool NearlyEqualRotation(const Quat& Left, const Quat& Right, const double DotThreshold = 0.99999)
{
    Quat A = Left;
    Quat B = Right;
    if (A.squaredNorm() <= static_cast<Quat::Scalar>(0) || B.squaredNorm() <= static_cast<Quat::Scalar>(0))
    {
        return false;
    }
    A.normalize();
    B.normalize();
    return std::abs(static_cast<double>(A.dot(B))) >= DotThreshold;
}

} // namespace

/**
 * @brief Test node that increments an external counter on tick.
 * @remarks Validates node tick traversal over parent/child hierarchy.
 */
struct TickNode : public BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TickNode";
    int* Counter = nullptr;

    explicit TickNode(int* InCounter)
        : Counter(InCounter)
    {
    }

    void Tick(float) override
    {
        if (Counter)
        {
            ++(*Counter);
        }
    }
};

/**
 * @brief Test component that increments an external counter on tick.
 * @remarks Validates component tick invocation during node traversal.
 */
struct CounterComponent : public IComponent
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CounterComponent";
    int* Counter = nullptr;

    CounterComponent() = default;
    explicit CounterComponent(int* InCounter)
        : Counter(InCounter)
    {
    }

    void Tick(float) override
    {
        if (Counter)
        {
            ++(*Counter);
        }
    }
};

/**
 * @brief Test node with per-phase counters.
 * @remarks Used to validate exactly-once phase dispatch.
 */
struct PhaseTickNode : public BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::PhaseTickNode";
    int* TickCounter = nullptr;
    int* FixedCounter = nullptr;
    int* LateCounter = nullptr;

    PhaseTickNode() = default;
    PhaseTickNode(int* InTickCounter, int* InFixedCounter, int* InLateCounter)
        : TickCounter(InTickCounter)
        , FixedCounter(InFixedCounter)
        , LateCounter(InLateCounter)
    {
    }

    void Tick(float) override
    {
        if (TickCounter)
        {
            ++(*TickCounter);
        }
    }

    void FixedTick(float) override
    {
        if (FixedCounter)
        {
            ++(*FixedCounter);
        }
    }

    void LateTick(float) override
    {
        if (LateCounter)
        {
            ++(*LateCounter);
        }
    }
};

/**
 * @brief Test component with per-phase counters.
 * @remarks Used to validate exactly-once storage-driven phase dispatch.
 */
struct PhaseCounterComponent : public IComponent
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::PhaseCounterComponent";
    int* TickCounter = nullptr;
    int* FixedCounter = nullptr;
    int* LateCounter = nullptr;

    PhaseCounterComponent() = default;
    PhaseCounterComponent(int* InTickCounter, int* InFixedCounter, int* InLateCounter)
        : TickCounter(InTickCounter)
        , FixedCounter(InFixedCounter)
        , LateCounter(InLateCounter)
    {
    }

    void Tick(float) override
    {
        if (TickCounter)
        {
            ++(*TickCounter);
        }
    }

    void FixedTick(float) override
    {
        if (FixedCounter)
        {
            ++(*FixedCounter);
        }
    }

    void LateTick(float) override
    {
        if (LateCounter)
        {
            ++(*LateCounter);
        }
    }
};

SNAPI_REFLECT_TYPE(TickNode, (TTypeBuilder<TickNode>(TickNode::kTypeName)
    .Base<BaseNode>()
    .Register()));

SNAPI_REFLECT_TYPE(CounterComponent, (TTypeBuilder<CounterComponent>(CounterComponent::kTypeName)
    .Register()));

SNAPI_REFLECT_TYPE(PhaseTickNode, (TTypeBuilder<PhaseTickNode>(PhaseTickNode::kTypeName)
    .Base<BaseNode>()
    .Register()));

SNAPI_REFLECT_TYPE(PhaseCounterComponent, (TTypeBuilder<PhaseCounterComponent>(PhaseCounterComponent::kTypeName)
    .Register()));

TEST_CASE("NodeGraph ticks nodes and components")
{
    NodeGraph Graph;
    int NodeTicks = 0;
    int ComponentTicks = 0;

    auto ParentResult = Graph.CreateNode<TickNode>("Parent", &NodeTicks);
    REQUIRE(ParentResult);
    auto Parent = ParentResult.value();

    auto ChildResult = Graph.CreateNode<TickNode>("Child", &NodeTicks);
    REQUIRE(ChildResult);
    auto Child = ChildResult.value();

    REQUIRE(Graph.AttachChild(Parent, Child));

    auto* ParentNode = Parent.Borrowed();
    REQUIRE(ParentNode != nullptr);
    auto CompResult = ParentNode->Add<CounterComponent>(&ComponentTicks);
    REQUIRE(CompResult);

    Graph.Tick(0.016f);

    REQUIRE(NodeTicks == 2);
    REQUIRE(ComponentTicks == 1);
}

TEST_CASE("NodeGraph skips component tick when owner node is inactive")
{
    NodeGraph Graph;
    int ComponentTicks = 0;

    auto NodeResult = Graph.CreateNode<BaseNode>("Node");
    REQUIRE(NodeResult);
    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);

    auto CompResult = Node->Add<CounterComponent>(&ComponentTicks);
    REQUIRE(CompResult);

    Node->Active(false);
    Graph.Tick(0.016f);

    REQUIRE(ComponentTicks == 0);
}

TEST_CASE("NodeGraph does not double tick nodes/components when detach is repeated")
{
    NodeGraph Graph;
    int NodeTicks = 0;
    int ComponentTicks = 0;

    auto ParentResult = Graph.CreateNode<BaseNode>("Parent");
    REQUIRE(ParentResult);
    auto ChildResult = Graph.CreateNode<TickNode>("Child", &NodeTicks);
    REQUIRE(ChildResult);

    REQUIRE(Graph.AttachChild(ParentResult.value(), ChildResult.value()));

    auto* ChildNode = ChildResult.value().Borrowed();
    REQUIRE(ChildNode != nullptr);
    auto ComponentResult = ChildNode->Add<CounterComponent>(&ComponentTicks);
    REQUIRE(ComponentResult);

    REQUIRE(Graph.DetachChild(ChildResult.value()));
    REQUIRE(Graph.DetachChild(ChildResult.value()));

    Graph.Tick(0.016f);

    REQUIRE(NodeTicks == 1);
    REQUIRE(ComponentTicks == 1);
}

TEST_CASE("NodeGraph dispatches each tick phase exactly once for nodes and components")
{
    NodeGraph Graph;
    int NodeTick = 0;
    int NodeFixed = 0;
    int NodeLate = 0;
    int ComponentTick = 0;
    int ComponentFixed = 0;
    int ComponentLate = 0;

    auto NodeResult = Graph.CreateNode<PhaseTickNode>("PhasedNode", &NodeTick, &NodeFixed, &NodeLate);
    REQUIRE(NodeResult);
    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);

    auto ComponentResult = Node->Add<PhaseCounterComponent>(&ComponentTick, &ComponentFixed, &ComponentLate);
    REQUIRE(ComponentResult);

    Graph.Tick(0.016f);
    Graph.FixedTick(0.008f);
    Graph.LateTick(0.016f);

    REQUIRE(NodeTick == 1);
    REQUIRE(NodeFixed == 1);
    REQUIRE(NodeLate == 1);
    REQUIRE(ComponentTick == 1);
    REQUIRE(ComponentFixed == 1);
    REQUIRE(ComponentLate == 1);
}

TEST_CASE("Node world transform composes full parent hierarchy and ignores transform-less intermediates")
{
    NodeGraph Graph;

    auto RootResult = Graph.CreateNode<BaseNode>("Root");
    REQUIRE(RootResult);
    auto MidResult = Graph.CreateNode<BaseNode>("Middle");
    REQUIRE(MidResult);
    auto LeafResult = Graph.CreateNode<BaseNode>("Leaf");
    REQUIRE(LeafResult);

    REQUIRE(Graph.AttachChild(RootResult.value(), MidResult.value()));
    REQUIRE(Graph.AttachChild(MidResult.value(), LeafResult.value()));

    auto* RootNode = RootResult.value().Borrowed();
    auto* LeafNode = LeafResult.value().Borrowed();
    REQUIRE(RootNode != nullptr);
    REQUIRE(LeafNode != nullptr);

    auto RootTransform = RootNode->Add<TransformComponent>();
    REQUIRE(RootTransform);
    RootTransform->Position = Vec3(10.0f, 0.0f, 0.0f);
    RootTransform->Rotation = ZRotation(static_cast<Quat::Scalar>(1.5707963267948966));
    RootTransform->Scale = Vec3(2.0f, 3.0f, 1.0f);

    auto LeafTransform = LeafNode->Add<TransformComponent>();
    REQUIRE(LeafTransform);
    LeafTransform->Position = Vec3(1.0f, 2.0f, 0.0f);
    LeafTransform->Rotation = Quat::Identity();
    LeafTransform->Scale = Vec3(0.5f, 2.0f, 1.0f);

    NodeTransform World{};
    REQUIRE(TransformComponent::TryGetNodeWorldTransform(*LeafNode, World));
    REQUIRE(NearlyEqual(World.Position, Vec3(4.0f, 2.0f, 0.0f)));
    REQUIRE(NearlyEqual(World.Scale, Vec3(1.0f, 6.0f, 1.0f)));
    REQUIRE(NearlyEqualRotation(World.Rotation, RootTransform->Rotation));
}

TEST_CASE("Node world transform updates immediately without requiring tick order")
{
    NodeGraph Graph;

    auto ParentResult = Graph.CreateNode<BaseNode>("Parent");
    REQUIRE(ParentResult);
    auto ChildResult = Graph.CreateNode<BaseNode>("Child");
    REQUIRE(ChildResult);
    REQUIRE(Graph.AttachChild(ParentResult.value(), ChildResult.value()));

    auto* ParentNode = ParentResult.value().Borrowed();
    auto* ChildNode = ChildResult.value().Borrowed();
    REQUIRE(ParentNode != nullptr);
    REQUIRE(ChildNode != nullptr);

    auto ParentTransform = ParentNode->Add<TransformComponent>();
    REQUIRE(ParentTransform);
    ParentTransform->Position = Vec3(1.0f, 0.0f, 0.0f);

    auto ChildTransform = ChildNode->Add<TransformComponent>();
    REQUIRE(ChildTransform);
    ChildTransform->Position = Vec3(2.0f, 0.0f, 0.0f);

    NodeTransform WorldBefore{};
    REQUIRE(TransformComponent::TryGetNodeWorldTransform(*ChildNode, WorldBefore));
    REQUIRE(NearlyEqual(WorldBefore.Position, Vec3(3.0f, 0.0f, 0.0f)));

    ParentTransform->Position = Vec3(5.0f, 0.0f, 0.0f);

    NodeTransform WorldAfter{};
    REQUIRE(TransformComponent::TryGetNodeWorldTransform(*ChildNode, WorldAfter));
    REQUIRE(NearlyEqual(WorldAfter.Position, Vec3(7.0f, 0.0f, 0.0f)));
}

TEST_CASE("Node world transform crosses prefab graph boundaries into owning parent hierarchy")
{
    NodeGraph WorldGraph;

    auto ParentResult = WorldGraph.CreateNode<BaseNode>("Parent");
    REQUIRE(ParentResult);
    auto PrefabGraphResult = WorldGraph.CreateNode<NodeGraph>("PrefabGraph");
    REQUIRE(PrefabGraphResult);
    REQUIRE(WorldGraph.AttachChild(ParentResult.value(), PrefabGraphResult.value()));

    auto* ParentNode = ParentResult.value().Borrowed();
    auto* PrefabGraphNode = dynamic_cast<NodeGraph*>(PrefabGraphResult.value().Borrowed());
    REQUIRE(ParentNode != nullptr);
    REQUIRE(PrefabGraphNode != nullptr);

    auto ParentTransform = ParentNode->Add<TransformComponent>();
    REQUIRE(ParentTransform);
    ParentTransform->Position = Vec3(10.0f, 0.0f, 0.0f);

    auto PrefabRootResult = PrefabGraphNode->CreateNode<BaseNode>("PrefabRoot");
    REQUIRE(PrefabRootResult);
    auto LeafResult = PrefabGraphNode->CreateNode<BaseNode>("Leaf");
    REQUIRE(LeafResult);
    REQUIRE(PrefabGraphNode->AttachChild(PrefabRootResult.value(), LeafResult.value()));

    auto* LeafNode = LeafResult.value().Borrowed();
    REQUIRE(LeafNode != nullptr);

    auto LeafTransform = LeafNode->Add<TransformComponent>();
    REQUIRE(LeafTransform);
    LeafTransform->Position = Vec3(1.0f, 2.0f, 0.0f);
    LeafTransform->Rotation = Quat::Identity();
    LeafTransform->Scale = Vec3(1.0f, 1.0f, 1.0f);

    NodeTransform World{};
    REQUIRE(TransformComponent::TryGetNodeWorldTransform(*LeafNode, World));
    REQUIRE(NearlyEqual(World.Position, Vec3(11.0f, 2.0f, 0.0f)));

    REQUIRE(TransformComponent::TrySetNodeWorldPose(*LeafNode, Vec3(13.0f, 2.0f, 0.0f), Quat::Identity(), true));
    REQUIRE(NearlyEqual(LeafTransform->Position, Vec3(3.0f, 2.0f, 0.0f)));
}

TEST_CASE("Serialized prefab graphs preserve parent-relative world transforms when instantiated")
{
    NodeGraph PrefabSource;

    auto PrefabRootResult = PrefabSource.CreateNode<BaseNode>("PrefabRoot");
    REQUIRE(PrefabRootResult);
    auto LeafResult = PrefabSource.CreateNode<BaseNode>("Leaf");
    REQUIRE(LeafResult);
    REQUIRE(PrefabSource.AttachChild(PrefabRootResult.value(), LeafResult.value()));

    auto* PrefabRootNode = PrefabRootResult.value().Borrowed();
    auto* PrefabLeafNode = LeafResult.value().Borrowed();
    REQUIRE(PrefabRootNode != nullptr);
    REQUIRE(PrefabLeafNode != nullptr);

    auto PrefabRootTransform = PrefabRootNode->Add<TransformComponent>();
    REQUIRE(PrefabRootTransform);
    PrefabRootTransform->Position = Vec3(2.0f, 0.0f, 0.0f);

    auto PrefabLeafTransform = PrefabLeafNode->Add<TransformComponent>();
    REQUIRE(PrefabLeafTransform);
    PrefabLeafTransform->Position = Vec3(1.0f, 0.0f, 0.0f);

    auto PayloadResult = NodeGraphSerializer::Serialize(PrefabSource);
    REQUIRE(PayloadResult);

    NodeGraph WorldGraph;
    auto ParentResult = WorldGraph.CreateNode<BaseNode>("Parent");
    REQUIRE(ParentResult);
    auto PrefabContainerResult = WorldGraph.CreateNode<NodeGraph>("PrefabContainer");
    REQUIRE(PrefabContainerResult);
    REQUIRE(WorldGraph.AttachChild(ParentResult.value(), PrefabContainerResult.value()));

    auto* ParentNode = ParentResult.value().Borrowed();
    auto* PrefabContainer = dynamic_cast<NodeGraph*>(PrefabContainerResult.value().Borrowed());
    REQUIRE(ParentNode != nullptr);
    REQUIRE(PrefabContainer != nullptr);

    auto ParentTransform = ParentNode->Add<TransformComponent>();
    REQUIRE(ParentTransform);
    ParentTransform->Position = Vec3(10.0f, 0.0f, 0.0f);

    REQUIRE(NodeGraphSerializer::Deserialize(PayloadResult.value(), *PrefabContainer));

    BaseNode* InstantiatedLeaf = nullptr;
    PrefabContainer->NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        if (Node.Name() == "Leaf")
        {
            InstantiatedLeaf = &Node;
        }
    });
    REQUIRE(InstantiatedLeaf != nullptr);

    NodeTransform World{};
    REQUIRE(TransformComponent::TryGetNodeWorldTransform(*InstantiatedLeaf, World));
    REQUIRE(NearlyEqual(World.Position, Vec3(13.0f, 0.0f, 0.0f)));
}

TEST_CASE("Prefab asset load-reserialize flow preserves hierarchy-relative transforms")
{
    NodeGraph SourcePrefab;

    auto RootResult = SourcePrefab.CreateNode<BaseNode>("Root");
    REQUIRE(RootResult);
    auto MidResult = SourcePrefab.CreateNode<BaseNode>("Mid");
    REQUIRE(MidResult);
    auto LeafResult = SourcePrefab.CreateNode<BaseNode>("Leaf");
    REQUIRE(LeafResult);
    REQUIRE(SourcePrefab.AttachChild(RootResult.value(), MidResult.value()));
    REQUIRE(SourcePrefab.AttachChild(MidResult.value(), LeafResult.value()));

    auto* RootNode = RootResult.value().Borrowed();
    auto* MidNode = MidResult.value().Borrowed();
    auto* LeafNode = LeafResult.value().Borrowed();
    REQUIRE(RootNode != nullptr);
    REQUIRE(MidNode != nullptr);
    REQUIRE(LeafNode != nullptr);

    auto RootTransform = RootNode->Add<TransformComponent>();
    REQUIRE(RootTransform);
    RootTransform->Position = Vec3(2.0f, 0.0f, 0.0f);

    auto MidTransform = MidNode->Add<TransformComponent>();
    REQUIRE(MidTransform);
    MidTransform->Position = Vec3(3.0f, 0.0f, 0.0f);

    auto LeafTransform = LeafNode->Add<TransformComponent>();
    REQUIRE(LeafTransform);
    LeafTransform->Position = Vec3(1.0f, 0.0f, 0.0f);

    auto InitialPayloadResult = NodeGraphSerializer::Serialize(SourcePrefab);
    REQUIRE(InitialPayloadResult);

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeNodeGraphPayload(InitialPayloadResult.value(), Bytes));
    REQUIRE_FALSE(Bytes.empty());

    auto RoundTripPayload = DeserializeNodeGraphPayload(Bytes.data(), Bytes.size());
    REQUIRE(RoundTripPayload);

    NodeGraph LoadedAssetGraph;
    REQUIRE(NodeGraphSerializer::Deserialize(RoundTripPayload.value(), LoadedAssetGraph));

    auto InstantiationPayloadResult = NodeGraphSerializer::Serialize(LoadedAssetGraph);
    REQUIRE(InstantiationPayloadResult);

    NodeGraph WorldGraph;
    auto ParentResult = WorldGraph.CreateNode<BaseNode>("Parent");
    REQUIRE(ParentResult);
    auto PrefabGraphResult = WorldGraph.CreateNode<NodeGraph>("PrefabGraph");
    REQUIRE(PrefabGraphResult);
    REQUIRE(WorldGraph.AttachChild(ParentResult.value(), PrefabGraphResult.value()));

    auto* ParentNode = ParentResult.value().Borrowed();
    auto* PrefabGraph = dynamic_cast<NodeGraph*>(PrefabGraphResult.value().Borrowed());
    REQUIRE(ParentNode != nullptr);
    REQUIRE(PrefabGraph != nullptr);

    auto ParentTransform = ParentNode->Add<TransformComponent>();
    REQUIRE(ParentTransform);
    ParentTransform->Position = Vec3(10.0f, 0.0f, 0.0f);

    REQUIRE(NodeGraphSerializer::Deserialize(InstantiationPayloadResult.value(), *PrefabGraph));

    BaseNode* InstantiatedLeaf = nullptr;
    PrefabGraph->NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        if (Node.Name() == "Leaf")
        {
            InstantiatedLeaf = &Node;
        }
    });
    REQUIRE(InstantiatedLeaf != nullptr);

    NodeTransform World{};
    REQUIRE(TransformComponent::TryGetNodeWorldTransform(*InstantiatedLeaf, World));
    REQUIRE(NearlyEqual(World.Position, Vec3(16.0f, 0.0f, 0.0f)));
}

TEST_CASE("Setting node world pose writes parent-relative local transform")
{
    NodeGraph Graph;

    auto RootResult = Graph.CreateNode<BaseNode>("Root");
    REQUIRE(RootResult);
    auto MidResult = Graph.CreateNode<BaseNode>("Middle");
    REQUIRE(MidResult);
    auto LeafResult = Graph.CreateNode<BaseNode>("Leaf");
    REQUIRE(LeafResult);

    REQUIRE(Graph.AttachChild(RootResult.value(), MidResult.value()));
    REQUIRE(Graph.AttachChild(MidResult.value(), LeafResult.value()));

    auto* RootNode = RootResult.value().Borrowed();
    auto* LeafNode = LeafResult.value().Borrowed();
    REQUIRE(RootNode != nullptr);
    REQUIRE(LeafNode != nullptr);

    auto RootTransform = RootNode->Add<TransformComponent>();
    REQUIRE(RootTransform);
    RootTransform->Position = Vec3(5.0f, 0.0f, 0.0f);
    RootTransform->Rotation = ZRotation(static_cast<Quat::Scalar>(1.5707963267948966));
    RootTransform->Scale = Vec3(2.0f, 1.0f, 1.0f);

    REQUIRE(TransformComponent::TrySetNodeWorldPose(*LeafNode, Vec3(5.0f, 2.0f, 0.0f), Quat::Identity(), true));
    REQUIRE(LeafNode->Has<TransformComponent>());

    auto LeafTransform = LeafNode->Component<TransformComponent>();
    REQUIRE(LeafTransform);
    REQUIRE(NearlyEqual(LeafTransform->Position, Vec3(1.0f, 0.0f, 0.0f)));
    REQUIRE(NearlyEqualRotation(LeafTransform->Rotation, ZRotation(static_cast<Quat::Scalar>(-1.5707963267948966))));
}
