#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;

namespace
{

void EnsureBuiltinsRegistered()
{
    static const bool Registered = [] {
        RegisterBuiltinTypes();
        return true;
    }();
    (void)Registered;
}

struct TEcsTickComponent final : TRuntimeTickCRTP<TEcsTickComponent>
{
    static constexpr const char* kTypeName = "Tests::TEcsTickComponent";

    explicit TEcsTickComponent(int* InCounter = nullptr)
        : Counter(InCounter)
    {
    }

    void Tick(IWorld&, float)
    {
        if (Counter)
        {
            ++(*Counter);
        }
    }

    int* Counter = nullptr;
};

static_assert(RuntimeTickType<TEcsTickComponent>);

} // namespace

TEST_CASE("World ECS-only tick updates runtime components")
{
    EnsureBuiltinsRegistered();
    World WorldInstance{"EcsOnlyTickWorld"};

    auto NodeResult = WorldInstance.CreateNode<BaseNode>("Node");
    REQUIRE(NodeResult.has_value());

    BaseNode* Node = NodeResult->Borrowed();
    REQUIRE(Node != nullptr);

    int TickCount = 0;
    auto AddResult = Node->AddRuntimeComponent<TEcsTickComponent>(&TickCount);
    REQUIRE(AddResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    REQUIRE(TickCount == 1);
}

TEST_CASE("World ECS hierarchy attach and detach updates node parent links")
{
    EnsureBuiltinsRegistered();
    World WorldInstance{"EcsOnlyHierarchyWorld"};

    auto ParentResult = WorldInstance.CreateNode<BaseNode>("Parent");
    auto ChildResult = WorldInstance.CreateNode<BaseNode>("Child");
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildResult.has_value());

    NodeHandle Parent = *ParentResult;
    NodeHandle Child = *ChildResult;

    REQUIRE(WorldInstance.AttachChild(Parent, Child));
    REQUIRE(Child.Borrowed() != nullptr);
    REQUIRE(Child.Borrowed()->Parent() == Parent);

    REQUIRE(WorldInstance.DetachChild(Child));
    REQUIRE(Child.Borrowed() != nullptr);
    REQUIRE(Child.Borrowed()->Parent().IsNull());
}

TEST_CASE("World ECS destroy is recursive for node subtrees")
{
    EnsureBuiltinsRegistered();
    World WorldInstance{"EcsOnlyDestroyWorld"};

    auto ParentResult = WorldInstance.CreateNode<BaseNode>("Parent");
    auto ChildResult = WorldInstance.CreateNode<BaseNode>("Child");
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildResult.has_value());

    NodeHandle Parent = *ParentResult;
    NodeHandle Child = *ChildResult;

    REQUIRE(WorldInstance.AttachChild(Parent, Child));
    REQUIRE(WorldInstance.DestroyNode(Parent));

    WorldInstance.EndFrame();

    REQUIRE(Parent.Borrowed() == nullptr);
    REQUIRE(Child.Borrowed() == nullptr);
}

TEST_CASE("Level nodes are lightweight wrappers over world-owned storage")
{
    EnsureBuiltinsRegistered();
    World WorldInstance{"EcsOnlyLevelWorld"};

    auto LevelResult = WorldInstance.CreateLevel("GameplayLevel");
    REQUIRE(LevelResult.has_value());

    auto* LevelNode = NodeCast<Level>(LevelResult->Borrowed());
    REQUIRE(LevelNode != nullptr);

    auto NestedResult = LevelNode->CreateNode<BaseNode>("Nested");
    REQUIRE(NestedResult.has_value());

    REQUIRE(NestedResult->Borrowed() != nullptr);
    REQUIRE(NestedResult->Borrowed()->Parent() == *LevelResult);
}
