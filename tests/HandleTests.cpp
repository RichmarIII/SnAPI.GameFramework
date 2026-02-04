#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

TEST_CASE("Handle lifecycle honors end-of-frame deletion")
{
    NodeGraph Graph;
    auto NodeResult = Graph.CreateNode("NodeA");
    REQUIRE(NodeResult);
    auto Handle = NodeResult.value();

    REQUIRE(Handle.IsValid());
    REQUIRE(Handle.Borrowed() != nullptr);

    REQUIRE(Graph.DestroyNode(Handle));
    REQUIRE(Handle.IsValid());
    REQUIRE(Handle.Borrowed() != nullptr);

    Graph.EndFrame();

    REQUIRE_FALSE(Handle.IsValid());
    REQUIRE(Handle.Borrowed() == nullptr);
}
