#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

struct RelevanceTickNode : public BaseNode
{
    static constexpr auto kTypeName = "SnAPI::GameFramework::RelevanceTickNode";
    int* Counter = nullptr;


    explicit RelevanceTickNode(int* InCounter)
        : Counter(InCounter)
    {
    }

    void Tick(float) override
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
    NodeGraph Graph;
    int Ticks = 0;

    auto NodeResult = Graph.CreateNode<RelevanceTickNode>("Node", &Ticks);
    REQUIRE(NodeResult);

    auto* Node = NodeResult.value().Borrowed();
    REQUIRE(Node != nullptr);
    auto RelResult = Node->Add<RelevanceComponent>();
    REQUIRE(RelResult);
    RelResult->Policy(AlwaysInactivePolicy{});

    Graph.Tick(0.016f);

    REQUIRE(Ticks == 0);
}
