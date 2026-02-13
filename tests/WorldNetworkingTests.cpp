#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

#include "NetSession.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::Networking;

namespace
{

struct RpcTestNode final : BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::RpcTestNode";

    int ServerValue = 0;
    int ClientValue = 0;

    void ServerOp(int Delta)
    {
        ServerValue += Delta;
    }

    void ClientOp(int Delta)
    {
        ClientValue += Delta;
    }
};

struct RpcTestComponent final : IComponent
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::RpcTestComponent";

    int ServerValue = 0;
    int ClientValue = 0;

    void ServerOp(int Delta)
    {
        ServerValue += Delta;
    }

    void ClientOp(int Delta)
    {
        ClientValue += Delta;
    }
};

SNAPI_REFLECT_TYPE(RpcTestNode, (TTypeBuilder<RpcTestNode>(RpcTestNode::kTypeName)
    .Base<BaseNode>()
    .Method("ServerOp",
            &RpcTestNode::ServerOp,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ClientOp",
            &RpcTestNode::ClientOp,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetClient)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(RpcTestComponent, (TTypeBuilder<RpcTestComponent>(RpcTestComponent::kTypeName)
    .Method("ServerOp",
            &RpcTestComponent::ServerOp,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ClientOp",
            &RpcTestComponent::ClientOp,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetClient)
    .Constructor<>()
    .Register()));

} // namespace

TEST_CASE("World networking role is visible to nodes and components")
{
    RegisterBuiltinTypes();

    World WorldRef("NetworkedWorld");
    NetworkBootstrapSettings Net{};
    Net.Role = ESessionRole::ServerAndClient;
    Net.Net.Threading.UseInternalThreads = false;
    Net.BindAddress = "127.0.0.1";
    Net.BindPort = 0;
    Net.AutoConnect = false;
    REQUIRE(WorldRef.Networking().InitializeOwnedSession(Net));
    auto* Session = WorldRef.Networking().Session();
    REQUIRE(Session != nullptr);

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

    Session->Role(ESessionRole::Client);
    REQUIRE_FALSE(Node->IsServer());
    REQUIRE(Node->IsClient());
    REQUIRE_FALSE(Node->IsListenServer());
    REQUIRE_FALSE(TransformResult->IsServer());
    REQUIRE(TransformResult->IsClient());
    REQUIRE_FALSE(TransformResult->IsListenServer());

    Session->Role(ESessionRole::Server);
    REQUIRE(Node->IsServer());
    REQUIRE_FALSE(Node->IsClient());
    REQUIRE_FALSE(Node->IsListenServer());
    REQUIRE(TransformResult->IsServer());
    REQUIRE_FALSE(TransformResult->IsClient());
    REQUIRE_FALSE(TransformResult->IsListenServer());
}

TEST_CASE("CallRPC routes node and component methods based on role")
{
    RegisterBuiltinTypes();

    World WorldRef("RpcWorld");
    NetworkBootstrapSettings Net{};
    Net.Role = ESessionRole::ServerAndClient;
    Net.Net.Threading.UseInternalThreads = false;
    Net.BindAddress = "127.0.0.1";
    Net.BindPort = 0;
    Net.AutoConnect = false;
    REQUIRE(WorldRef.Networking().InitializeOwnedSession(Net));
    auto* Session = WorldRef.Networking().Session();
    REQUIRE(Session != nullptr);

    auto NodeResult = WorldRef.CreateNode<RpcTestNode>("RpcActor");
    REQUIRE(NodeResult);
    auto* Node = static_cast<RpcTestNode*>(NodeResult->Borrowed());
    REQUIRE(Node != nullptr);

    auto ComponentResult = Node->Add<RpcTestComponent>();
    REQUIRE(ComponentResult);
    auto* Component = &*ComponentResult;
    REQUIRE(Component->TypeKey() == StaticTypeId<RpcTestComponent>());

    REQUIRE(Node->CallRPC("ServerOp", {Variant::FromValue(3)}));
    REQUIRE(Node->ServerValue == 3);

    REQUIRE(Component->CallRPC("ServerOp", {Variant::FromValue(4)}));
    REQUIRE(Component->ServerValue == 4);

    REQUIRE(Node->CallRPC("ClientOp", {Variant::FromValue(5)}));
    REQUIRE(Node->ClientValue == 5);

    REQUIRE(Component->CallRPC("ClientOp", {Variant::FromValue(6)}));
    REQUIRE(Component->ClientValue == 6);

    Session->Role(ESessionRole::Client);

    REQUIRE_FALSE(Node->CallRPC("ServerOp", {Variant::FromValue(1)}));
    REQUIRE(Node->ServerValue == 3);

    REQUIRE_FALSE(Component->CallRPC("ServerOp", {Variant::FromValue(1)}));
    REQUIRE(Component->ServerValue == 4);

    REQUIRE(Node->CallRPC("ClientOp", {Variant::FromValue(7)}));
    REQUIRE(Node->ClientValue == 12);

    REQUIRE(Component->CallRPC("ClientOp", {Variant::FromValue(8)}));
    REQUIRE(Component->ClientValue == 14);

    REQUIRE_FALSE(Node->CallRPC("MissingMethod"));
    REQUIRE_FALSE(Component->CallRPC("MissingMethod"));
}
