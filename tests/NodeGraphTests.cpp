#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

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
