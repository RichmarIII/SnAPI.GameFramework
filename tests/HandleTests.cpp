#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

TEST_CASE("Handle lifecycle honors end-of-frame deletion")
{
    Level Graph;
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

TEST_CASE("Runtime and UUID-only node handles resolve consistently")
{
    Level Graph;
    auto NodeResult = Graph.CreateNode("NodeA");
    REQUIRE(NodeResult);
    const NodeHandle RuntimeHandle = NodeResult.value();
    REQUIRE(RuntimeHandle.HasRuntimeKey());

    const NodeHandle UuidOnlyHandle(RuntimeHandle.Id);
    REQUIRE_FALSE(UuidOnlyHandle.HasRuntimeKey());

    REQUIRE(Graph.NodePool().Borrowed(RuntimeHandle) != nullptr);
    REQUIRE(Graph.NodePool().Borrowed(UuidOnlyHandle) == nullptr);
    REQUIRE(UuidOnlyHandle.Borrowed() != nullptr);
    REQUIRE(UuidOnlyHandle.HasRuntimeKey());
    REQUIRE(UuidOnlyHandle.BorrowedSlowByUuid() != nullptr);
}

TEST_CASE("DestroyNode is idempotent while deferred destruction is pending")
{
    Level Graph;
    auto NodeResult = Graph.CreateNode("NodeA");
    REQUIRE(NodeResult);
    const NodeHandle Handle = NodeResult.value();

    REQUIRE(Graph.DestroyNode(Handle));
    REQUIRE(Graph.DestroyNode(Handle));
    REQUIRE(Handle.IsValid());

    Graph.EndFrame();
    REQUIRE_FALSE(Handle.IsValid());
}
