#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

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

SNAPI_REFLECT_TYPE(TickNode, (TTypeBuilder<TickNode>(TickNode::kTypeName)
    .Base<BaseNode>()
    .Register()));

SNAPI_REFLECT_TYPE(CounterComponent, (TTypeBuilder<CounterComponent>(CounterComponent::kTypeName)
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
