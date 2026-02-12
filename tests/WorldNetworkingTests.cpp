#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

#include "NetSession.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::Networking;

TEST_CASE("World networking role is visible to nodes and components")
{
    RegisterBuiltinTypes();

    NetConfig Config{};
    Config.Threading.UseInternalThreads = false;

    NetSession Session(Config);
    Session.Role(ESessionRole::ServerAndClient);

    World WorldRef("NetworkedWorld");
    REQUIRE(WorldRef.Networking().AttachSession(Session));

    auto NodeResult = WorldRef.CreateNode("Actor");
    REQUIRE(NodeResult);
    auto* Node = NodeResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto TransformResult = Node->Add<TransformComponent>();
    REQUIRE(TransformResult);

    REQUIRE(Node->IsServer());
    REQUIRE(Node->IsClient());
    REQUIRE(Node->IsListenServer());
    REQUIRE(TransformResult->IsServer());
    REQUIRE(TransformResult->IsClient());
    REQUIRE(TransformResult->IsListenServer());

    Session.Role(ESessionRole::Client);
    REQUIRE_FALSE(Node->IsServer());
    REQUIRE(Node->IsClient());
    REQUIRE_FALSE(Node->IsListenServer());
    REQUIRE_FALSE(TransformResult->IsServer());
    REQUIRE(TransformResult->IsClient());
    REQUIRE_FALSE(TransformResult->IsListenServer());

    Session.Role(ESessionRole::Server);
    REQUIRE(Node->IsServer());
    REQUIRE_FALSE(Node->IsClient());
    REQUIRE_FALSE(Node->IsListenServer());
    REQUIRE(TransformResult->IsServer());
    REQUIRE_FALSE(TransformResult->IsClient());
    REQUIRE_FALSE(TransformResult->IsListenServer());
}

