#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;

TEST_CASE("Handle lifecycle honors end-of-frame deletion")
{
    World Graph;
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
    World Graph;
    auto NodeResult = Graph.CreateNode("NodeA");
    REQUIRE(NodeResult);
    NodeHandle RuntimeHandle = NodeResult.value();
    REQUIRE(RuntimeHandle.HasRuntimeKey());

    NodeHandle UuidOnlyHandle(RuntimeHandle.Id);
    REQUIRE_FALSE(UuidOnlyHandle.HasRuntimeKey());

    REQUIRE(Graph.EcsRuntime().ResolveNode(RuntimeHandle) != nullptr);
    REQUIRE(Graph.EcsRuntime().ResolveNode(UuidOnlyHandle) == nullptr);
    REQUIRE(UuidOnlyHandle.Borrowed() != nullptr);
    REQUIRE(UuidOnlyHandle.HasRuntimeKey());
    REQUIRE(UuidOnlyHandle.BorrowedSlowByUuid() != nullptr);
}

TEST_CASE("World borrowed helpers rehydrate UUID-only node and component handles")
{
    World Graph;
    auto NodeResult = Graph.CreateNode("NodeA");
    REQUIRE(NodeResult);

    NodeHandle NodeUuidOnlyHandle(NodeResult->Id);
    REQUIRE_FALSE(NodeUuidOnlyHandle.HasRuntimeKey());
    REQUIRE(Graph.BorrowedNode(NodeUuidOnlyHandle) != nullptr);
    REQUIRE(NodeUuidOnlyHandle.HasRuntimeKey());

    auto* Node = Graph.BorrowedNode(NodeUuidOnlyHandle);
    REQUIRE(Node != nullptr);

    auto ComponentResult = Node->Add<TransformComponent>();
    REQUIRE(ComponentResult);

    ComponentHandle ComponentUuidOnlyHandle(ComponentResult->Handle().Id);
    REQUIRE_FALSE(ComponentUuidOnlyHandle.HasRuntimeKey());
    REQUIRE(Graph.BorrowedComponent(ComponentUuidOnlyHandle) != nullptr);
    REQUIRE(ComponentUuidOnlyHandle.HasRuntimeKey());
}

TEST_CASE("Component handles stay valid after same-type dense storage growth")
{
    World Graph;
    auto OwnerResult = Graph.CreateNode("OwnerNode");
    REQUIRE(OwnerResult);

    auto* OwnerNode = Graph.BorrowedNode(*OwnerResult);
    REQUIRE(OwnerNode != nullptr);

    auto OriginalTransform = OwnerNode->Add<TransformComponent>();
    REQUIRE(OriginalTransform);
    OriginalTransform->Position = Vec3(101.0, 202.0, 303.0);

    ComponentHandle TransformHandle = OriginalTransform->Handle();
    REQUIRE(TransformHandle.HasRuntimeKey());

    for (int Index = 0; Index < 32; ++Index)
    {
        auto ExtraNodeResult = Graph.CreateNode("ExtraNode" + std::to_string(Index));
        REQUIRE(ExtraNodeResult);
        auto* ExtraNode = Graph.BorrowedNode(*ExtraNodeResult);
        REQUIRE(ExtraNode != nullptr);
        REQUIRE(ExtraNode->Add<TransformComponent>());
    }

    auto* ResolvedBase = Graph.BorrowedComponent(TransformHandle);
    REQUIRE(ResolvedBase != nullptr);
    auto* ResolvedTransform = static_cast<TransformComponent*>(ResolvedBase);
    CHECK(ResolvedTransform->Position.x() == Catch::Approx(101.0));
    CHECK(ResolvedTransform->Position.y() == Catch::Approx(202.0));
    CHECK(ResolvedTransform->Position.z() == Catch::Approx(303.0));
}

TEST_CASE("BaseComponent owner resolution rehydrates UUID-only owner handles")
{
    World Graph;
    auto NodeResult = Graph.CreateNode("OwnerNode");
    REQUIRE(NodeResult);

    NodeHandle OwnerHandle = NodeResult.value();
    auto* OwnerNode = Graph.BorrowedNode(OwnerHandle);
    REQUIRE(OwnerNode != nullptr);

    auto ComponentResult = OwnerNode->Add<TransformComponent>();
    REQUIRE(ComponentResult);

    ComponentResult->Owner(NodeHandle(OwnerHandle.Id));
    REQUIRE_FALSE(ComponentResult->Owner().HasRuntimeKey());
    REQUIRE(ComponentResult->OwnerNode() == OwnerNode);
    REQUIRE(ComponentResult->Owner().HasRuntimeKey());
}

TEST_CASE("Component attachments stay isolated across different dense node storages")
{
    RegisterBuiltinTypes();

    World Graph;
    auto PlayerStartResult = Graph.CreateNode<PlayerStart>("Start");
    auto PawnResult = Graph.CreateNode<PawnBase>("Pawn");
    REQUIRE(PlayerStartResult);
    REQUIRE(PawnResult);

    NodeHandle PlayerStartHandle = *PlayerStartResult;
    NodeHandle PawnHandle = *PawnResult;

    REQUIRE(Graph.RequestNodeOnCreate(PlayerStartHandle));
    REQUIRE(Graph.RequestNodeOnCreate(PawnHandle));

    auto* PlayerStartNode = NodeCast<PlayerStart>(Graph.BorrowedNode(PlayerStartHandle));
    auto* PawnNode = NodeCast<PawnBase>(Graph.BorrowedNode(PawnHandle));
    REQUIRE(PlayerStartNode != nullptr);
    REQUIRE(PawnNode != nullptr);

    auto PlayerStartTransform = PlayerStartNode->Component<TransformComponent>();
    auto PawnTransform = PawnNode->Component<TransformComponent>();
    REQUIRE(PlayerStartTransform);
    REQUIRE(PawnTransform);
    CHECK(PlayerStartTransform->Handle().Id != PawnTransform->Handle().Id);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    REQUIRE(PawnNode->Component<CameraComponent>());
    REQUIRE(PawnNode->Component<SprintArmComponent>());
#endif
}

TEST_CASE("Level child nodes keep their own components when other node storages share slot zero")
{
    RegisterBuiltinTypes();

    World Graph;
    auto LevelResult = Graph.CreateLevel("Gameplay");
    auto PlayerStartResult = Graph.CreateNode<PlayerStart>("Start");
    REQUIRE(LevelResult);
    REQUIRE(PlayerStartResult);

    NodeHandle PlayerStartHandle = *PlayerStartResult;
    REQUIRE(Graph.RequestNodeOnCreate(PlayerStartHandle));

    auto* LevelNode = NodeCast<Level>(Graph.BorrowedNode(*LevelResult));
    REQUIRE(LevelNode != nullptr);

    auto ChildResult = LevelNode->CreateNode<BaseNode>("Child");
    REQUIRE(ChildResult);

    auto* ChildNode = Graph.BorrowedNode(*ChildResult);
    REQUIRE(ChildNode != nullptr);
    CHECK(ChildNode->Parent() == *LevelResult);

    REQUIRE(ChildNode->Add<TransformComponent>());
    REQUIRE(ChildNode->Component<TransformComponent>());

#if defined(SNAPI_GF_ENABLE_RENDERER)
    REQUIRE(ChildNode->Add<StaticMeshComponent>());
    REQUIRE(ChildNode->Component<StaticMeshComponent>());
#endif
}

TEST_CASE("Level borrowed helpers rehydrate UUID-only node and component handles")
{
    World Graph;
    auto LevelResult = Graph.CreateLevel("Level");
    REQUIRE(LevelResult);

    NodeHandle LevelHandle = LevelResult.value();
    auto* LevelNode = NodeCast<Level>(Graph.BorrowedNode(LevelHandle));
    REQUIRE(LevelNode != nullptr);

    auto ChildResult = LevelNode->CreateNode<BaseNode>("Child");
    REQUIRE(ChildResult);

    NodeHandle ChildUuidOnlyHandle(ChildResult->Id);
    REQUIRE_FALSE(ChildUuidOnlyHandle.HasRuntimeKey());
    REQUIRE(LevelNode->BorrowedNode(ChildUuidOnlyHandle) != nullptr);
    REQUIRE(ChildUuidOnlyHandle.HasRuntimeKey());

    auto* ChildNode = LevelNode->BorrowedNode(ChildUuidOnlyHandle);
    REQUIRE(ChildNode != nullptr);

    auto ComponentResult = ChildNode->Add<TransformComponent>();
    REQUIRE(ComponentResult);

    ComponentHandle ComponentUuidOnlyHandle(ComponentResult->Handle().Id);
    REQUIRE_FALSE(ComponentUuidOnlyHandle.HasRuntimeKey());
    REQUIRE(LevelNode->BorrowedComponent(ComponentUuidOnlyHandle) != nullptr);
    REQUIRE(ComponentUuidOnlyHandle.HasRuntimeKey());
}

TEST_CASE("LocalPlayer possession setter rehydrates UUID-only target handles")
{
    World Graph;
    auto PlayerResult = Graph.CreateNode<LocalPlayer>("Player");
    REQUIRE(PlayerResult);
    auto TargetResult = Graph.CreateNode("Target");
    REQUIRE(TargetResult);

    NodeHandle PlayerHandle = PlayerResult.value();
    auto* PlayerNode = NodeCast<LocalPlayer>(Graph.BorrowedNode(PlayerHandle));
    REQUIRE(PlayerNode != nullptr);

    PlayerNode->SetPossessedNode(NodeHandle(TargetResult->Id));
    REQUIRE(PlayerNode->GetPossessedNode().Id == TargetResult->Id);
    REQUIRE(PlayerNode->GetPossessedNode().HasRuntimeKey());
}

TEST_CASE("DestroyNode is idempotent while deferred destruction is pending")
{
    World Graph;
    auto NodeResult = Graph.CreateNode("NodeA");
    REQUIRE(NodeResult);
    NodeHandle Handle = NodeResult.value();

    REQUIRE(Graph.DestroyNode(Handle));
    REQUIRE(Graph.DestroyNode(Handle));
    REQUIRE(Handle.IsValid());

    Graph.EndFrame();
    REQUIRE_FALSE(Handle.IsValid());
}
