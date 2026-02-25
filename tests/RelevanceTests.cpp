#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

/**
 * @brief Tick-counting node used to verify relevance gating behavior.
 */
struct RelevanceTickNode : public BaseNode
{
    static constexpr auto kTypeName = "SnAPI::GameFramework::RelevanceTickNode";
    int* Counter = nullptr;


    explicit RelevanceTickNode(int* InCounter)
        : Counter(InCounter)
    {
    }

    void Tick(float)
    {
        if (Counter)
        {
            ++*Counter;
        }
    }
};

SNAPI_REFLECT_TYPE(RelevanceTickNode, (TTypeBuilder<RelevanceTickNode>(RelevanceTickNode::kTypeName)
    .Base<BaseNode>()
    .Register()));

/**
 * @brief Tick-counting component used to verify relevance-gated component ticking.
 */
struct RelevanceCounterComponent : public BaseComponent, public ComponentCRTP<RelevanceCounterComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RelevanceCounterComponent";
    int* Counter = nullptr;

    RelevanceCounterComponent() = default;

    explicit RelevanceCounterComponent(int* InCounter)
        : Counter(InCounter)
    {
    }

    void Tick(float)
    {
        if (Counter)
        {
            ++(*Counter);
        }
    }
};

SNAPI_REFLECT_TYPE(RelevanceCounterComponent, (TTypeBuilder<RelevanceCounterComponent>(RelevanceCounterComponent::kTypeName)
    .Register()));

/**
 * @brief Policy that always returns inactive to force relevance culling.
 */
struct AlwaysInactivePolicy
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AlwaysInactivePolicy";

    bool Evaluate(const RelevanceContext&) const
    {
        return false;
    }
};

TEST_CASE("Relevance can disable node ticking")
{
    Level Graph;
    int Ticks = 0;
    int ComponentTicks = 0;

    auto NodeResult = Graph.CreateNode<RelevanceTickNode>("Node", &Ticks);
    REQUIRE(NodeResult);

    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);
    auto RelResult = Node->Add<RelevanceComponent>();
    REQUIRE(RelResult);
    RelResult->Policy(AlwaysInactivePolicy{});
    auto ComponentResult = Node->Add<RelevanceCounterComponent>(&ComponentTicks);
    REQUIRE(ComponentResult);

    Graph.Tick(0.016f);

    REQUIRE(Ticks == 0);
    REQUIRE(ComponentTicks == 0);
}
