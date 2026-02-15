#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace
{

struct RuntimeTickNode final : BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::RuntimeTickNode";

    int TickCount = 0;
    int FixedTickCount = 0;
    int LateTickCount = 0;

    void Tick(float) override
    {
        ++TickCount;
    }

    void FixedTick(float) override
    {
        ++FixedTickCount;
    }

    void LateTick(float) override
    {
        ++LateTickCount;
    }
};

SNAPI_REFLECT_TYPE(RuntimeTickNode, (TTypeBuilder<RuntimeTickNode>(RuntimeTickNode::kTypeName)
    .Base<BaseNode>()
    .Constructor<>()
    .Register()));

} // namespace

TEST_CASE("GameRuntime drives world ticks through Update")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "RuntimeLifecycleWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 0.01f;
    Settings.Tick.MaxFixedStepsPerUpdate = 3;
    Settings.Tick.EnableLateTick = true;
    Settings.Tick.EnableEndFrame = true;
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    Settings.Networking.reset();
#endif

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.IsInitialized());

    auto NodeResult = Runtime.World().CreateNode<RuntimeTickNode>("RuntimeTickNode");
    REQUIRE(NodeResult);
    auto* Node = static_cast<RuntimeTickNode*>(NodeResult->Borrowed());
    REQUIRE(Node != nullptr);

    Runtime.Update(0.035f);

    REQUIRE(Node->TickCount == 1);
    REQUIRE(Node->FixedTickCount == 3);
    REQUIRE(Node->LateTickCount == 1);

    Runtime.Shutdown();
    REQUIRE_FALSE(Runtime.IsInitialized());
    REQUIRE(Runtime.WorldPtr() == nullptr);
}

TEST_CASE("GameRuntime preserves bounded fixed-step backlog across updates")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "RuntimeBacklogWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 0.01f;
    Settings.Tick.MaxFixedStepsPerUpdate = 3;
    Settings.Tick.EnableLateTick = false;
    Settings.Tick.EnableEndFrame = false;
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    Settings.Networking.reset();
#endif

    REQUIRE(Runtime.Init(Settings));

    auto NodeResult = Runtime.World().CreateNode<RuntimeTickNode>("RuntimeBacklogNode");
    REQUIRE(NodeResult);
    auto* Node = static_cast<RuntimeTickNode*>(NodeResult->Borrowed());
    REQUIRE(Node != nullptr);

    // 80 ms requires 8 fixed ticks at 10 ms. First update is capped at 3 steps.
    Runtime.Update(0.08f);
    REQUIRE(Node->FixedTickCount == 3);

    // Backlog should be preserved and drained on subsequent updates.
    Runtime.Update(0.0f);
    REQUIRE(Node->FixedTickCount == 6);

    Runtime.Shutdown();
}

#if defined(SNAPI_GF_ENABLE_NETWORKING)
TEST_CASE("GameRuntime initializes world networking subsystem")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "RuntimeNetworkingWorld";
    Settings.RegisterBuiltins = true;

    GameRuntimeNetworkingSettings Net{};
    Net.Role = SnAPI::Networking::ESessionRole::Server;
    Net.Net.Threading.UseInternalThreads = false;
    Net.BindAddress = "127.0.0.1";
    Net.BindPort = 0;
    Net.AutoConnect = false;
    Settings.Networking = Net;

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.World().Networking().Session() != nullptr);
    REQUIRE(Runtime.World().Networking().Transport() != nullptr);
    REQUIRE(Runtime.World().Networking().IsServer());

    Runtime.Update(0.016f);

    Runtime.Shutdown();
    REQUIRE(Runtime.WorldPtr() == nullptr);
}
#endif
