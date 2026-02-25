#include <array>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

#include "NetSession.h"
#include "Services/ReplicationService.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::Networking;

namespace
{

/**
 * @brief In-memory loopback datagram transport used for deterministic replication tests.
 * @remarks Provides a controllable no-socket transport for two-session pumping.
 */
class TestDatagramTransport final : public INetDatagramTransport
{
public:
    TestDatagramTransport(TransportHandle HandleValue, NetEndpoint LocalEndpointValue)
        : m_handle(HandleValue)
        , m_local(std::move(LocalEndpointValue))
    {
    }

    TransportHandle Handle() const override { return m_handle; }

    void Link(TestDatagramTransport* PeerValue) { m_peer = PeerValue; }

    bool Receive(NetDatagram& OutDatagram) override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_queue.empty())
        {
            return false;
        }
        OutDatagram = std::move(m_queue.front());
        m_queue.pop_front();
        return true;
    }

    bool Send(const NetDatagram& Datagram) override
    {
        if (!m_peer)
        {
            return false;
        }
        NetDatagram Delivered = Datagram;
        Delivered.Remote = m_local;
        std::lock_guard<std::mutex> Lock(m_peer->m_mutex);
        m_peer->m_queue.push_back(std::move(Delivered));
        return true;
    }

private:
    TransportHandle m_handle = 0;
    NetEndpoint m_local{};
    TestDatagramTransport* m_peer = nullptr;
    std::deque<NetDatagram> m_queue{};
    std::mutex m_mutex{};
};

/**
 * @brief Replicated test node type for spawn/update verification.
 */
struct ReplicatedNode final : public BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ReplicatedNode";

    int Health = 0;
};

/**
 * @brief Replicated test component type for spawn/update verification.
 */
struct ReplicatedComponent final : public BaseComponent, public ComponentCRTP<ReplicatedComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ReplicatedComponent";

    int Value = 0;
    Vec3 Offset{};
};

SNAPI_REFLECT_TYPE(ReplicatedNode, (TTypeBuilder<ReplicatedNode>(ReplicatedNode::kTypeName)
    .Base<BaseNode>()
    .Field("Health", &ReplicatedNode::Health, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(ReplicatedComponent, (TTypeBuilder<ReplicatedComponent>(ReplicatedComponent::kTypeName)
    .Field("Value", &ReplicatedComponent::Value, EFieldFlagBits::Replication)
    .Field("Offset", &ReplicatedComponent::Offset, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

void RegisterReplicationTestTypes()
{
    static bool Registered = false;
    if (Registered)
    {
        return;
    }
    RegisterBuiltinTypes();
    Registered = true;
}

/**
 * @brief Helper payload bundle used by spawn-header decode tests.
 */
struct ReplicationPayload
{
    ReplicationEntityState Entity{};
    std::vector<Byte> Bytes{};
    std::uint8_t Kind = 0;
    Uuid ObjectId{};
    Uuid OwnerId{};
};

bool DecodeUuid(NetByteReader& Reader, Uuid& Out)
{
    std::array<Byte, 16> Data{};
    if (!Reader.ReadBytes(ByteSpan(Data.data(), Data.size())))
    {
        return false;
    }
    Out = Uuid(Data);
    return true;
}

bool DecodeReplicationHeader(ConstByteSpan Payload, ReplicationPayload& Out)
{
    NetByteReader Reader(Payload);
    if (!Reader.ReadU8(Out.Kind))
    {
        return false;
    }
    Uuid TypeId{};
    return DecodeUuid(Reader, Out.ObjectId)
        && DecodeUuid(Reader, TypeId)
        && DecodeUuid(Reader, Out.OwnerId);
}

void PumpPair(NetSession& Client, NetSession& Server, TimePoint& Now, int Steps)
{
    for (int Step = 0; Step < Steps; ++Step)
    {
        Server.Pump(Now);
        Client.Pump(Now);
        Now += Milliseconds{10};
    }
}

} // namespace

TEST_CASE("NetReplicationBridge spawns nodes and components")
{
    RegisterReplicationTestTypes();

    World ServerWorld("ServerWorld");
    auto NodeResult = ServerWorld.CreateNode<ReplicatedNode>("Actor");
    REQUIRE(NodeResult);
    auto* ServerNode = static_cast<ReplicatedNode*>(NodeResult->Borrowed());
    REQUIRE(ServerNode != nullptr);
    ServerNode->Replicated(true);
    ServerNode->Health = 12;

    auto ComponentResult = ServerNode->Add<ReplicatedComponent>();
    REQUIRE(ComponentResult);
    auto* ServerComponent = &*ComponentResult;
    ServerComponent->Replicated(true);
    ServerComponent->Value = 42;
    ServerComponent->Offset = Vec3(1.0f, 2.0f, 3.0f);

    const Uuid NodeId = ServerNode->Id();
    const Uuid ComponentId = ServerComponent->Id();

    NetReplicationBridge ServerBridge(ServerWorld);
    std::vector<ReplicationEntityState> Entities;
    ServerBridge.GatherEntities(Entities);
    REQUIRE(Entities.size() == 2);

    std::vector<ReplicationPayload> Payloads;
    Payloads.reserve(Entities.size());
    for (const auto& Entity : Entities)
    {
        ReplicationPayload Payload;
        Payload.Entity = Entity;
        REQUIRE(ServerBridge.BuildSnapshot(Entity.EntityIdValue, Entity.TypeIdValue, Payload.Bytes));
        REQUIRE(DecodeReplicationHeader(ConstByteSpan(Payload.Bytes.data(), Payload.Bytes.size()), Payload));
        Payloads.push_back(std::move(Payload));
    }

    // Avoid ObjectRegistry collisions by clearing the server graph before spawning on client.
    ServerWorld.Clear();

    World ClientWorld("ClientWorld");
    NetReplicationBridge ClientBridge(ClientWorld);

    for (const auto& Payload : Payloads)
    {
        ClientBridge.OnSpawn(NetConnectionHandle{0},
                             Payload.Entity.EntityIdValue,
                             Payload.Entity.TypeIdValue,
                             ConstByteSpan(Payload.Bytes.data(), Payload.Bytes.size()));
    }

    auto* ClientNodeBase = ObjectRegistry::Instance().Resolve<BaseNode>(NodeId);
    REQUIRE(ClientNodeBase != nullptr);
    auto* ClientNode = dynamic_cast<ReplicatedNode*>(ClientNodeBase);
    REQUIRE(ClientNode != nullptr);
    REQUIRE(ClientNode->Health == 12);

    auto* ClientComponentBase = ObjectRegistry::Instance().Resolve<BaseComponent>(ComponentId);
    REQUIRE(ClientComponentBase != nullptr);
    auto* ClientComponent = static_cast<ReplicatedComponent*>(ClientComponentBase);
    REQUIRE(ClientComponent != nullptr);
    REQUIRE(ClientComponent->Value == 42);
    REQUIRE(ClientComponent->Offset.x() == 1.0f);
    REQUIRE(ClientComponent->Offset.y() == 2.0f);
    REQUIRE(ClientComponent->Offset.z() == 3.0f);
}

TEST_CASE("NetReplicationBridge updates replicated fields")
{
    RegisterReplicationTestTypes();

    World ServerWorld("ServerWorld");
    auto NodeResult = ServerWorld.CreateNode<ReplicatedNode>("Actor");
    REQUIRE(NodeResult);
    auto* ServerNode = static_cast<ReplicatedNode*>(NodeResult->Borrowed());
    REQUIRE(ServerNode != nullptr);
    ServerNode->Replicated(true);
    ServerNode->Health = 1;

    auto ComponentResult = ServerNode->Add<ReplicatedComponent>();
    REQUIRE(ComponentResult);
    auto* ServerComponent = &*ComponentResult;
    ServerComponent->Replicated(true);
    ServerComponent->Value = 7;
    ServerComponent->Offset = Vec3(0.0f, 0.0f, 0.0f);

    const Uuid NodeId = ServerNode->Id();
    const Uuid ComponentId = ServerComponent->Id();

    NetReplicationBridge ServerBridge(ServerWorld);
    World ClientWorld("ClientWorld");
    NetReplicationBridge ClientBridge(ClientWorld);

    std::vector<ReplicationEntityState> Entities;
    ServerBridge.GatherEntities(Entities);
    REQUIRE(Entities.size() == 2);

    for (const auto& Entity : Entities)
    {
        std::vector<Byte> Snapshot;
        REQUIRE(ServerBridge.BuildSnapshot(Entity.EntityIdValue, Entity.TypeIdValue, Snapshot));
        ClientBridge.OnSpawn(NetConnectionHandle{0},
                             Entity.EntityIdValue,
                             Entity.TypeIdValue,
                             ConstByteSpan(Snapshot.data(), Snapshot.size()));
    }

    ServerNode->Health = 9;
    ServerComponent->Value = 18;
    ServerComponent->Offset = Vec3(4.0f, 5.0f, 6.0f);

    ServerBridge.GatherEntities(Entities);
    for (const auto& Entity : Entities)
    {
        std::vector<Byte> Snapshot;
        REQUIRE(ServerBridge.BuildSnapshot(Entity.EntityIdValue, Entity.TypeIdValue, Snapshot));
        ClientBridge.OnUpdate(NetConnectionHandle{0},
                              Entity.EntityIdValue,
                              Entity.TypeIdValue,
                              ConstByteSpan(Snapshot.data(), Snapshot.size()));
    }

    auto* ClientNodeBase = ObjectRegistry::Instance().Resolve<BaseNode>(NodeId);
    REQUIRE(ClientNodeBase != nullptr);
    auto* ClientNode = dynamic_cast<ReplicatedNode*>(ClientNodeBase);
    REQUIRE(ClientNode != nullptr);
    REQUIRE(ClientNode->Health == 9);

    auto* ClientComponentBase = ObjectRegistry::Instance().Resolve<BaseComponent>(ComponentId);
    REQUIRE(ClientComponentBase != nullptr);
    auto* ClientComponent = static_cast<ReplicatedComponent*>(ClientComponentBase);
    REQUIRE(ClientComponent != nullptr);
    REQUIRE(ClientComponent->Value == 18);
    REQUIRE(ClientComponent->Offset.x() == 4.0f);
    REQUIRE(ClientComponent->Offset.y() == 5.0f);
    REQUIRE(ClientComponent->Offset.z() == 6.0f);
}

TEST_CASE("NetReplicationBridge resolves pending parents and components")
{
    RegisterReplicationTestTypes();

    World ServerWorld("ServerWorld");
    auto ParentResult = ServerWorld.CreateNode<ReplicatedNode>("Parent");
    auto ChildResult = ServerWorld.CreateNode<ReplicatedNode>("Child");
    REQUIRE(ParentResult);
    REQUIRE(ChildResult);
    REQUIRE(ServerWorld.AttachChild(ParentResult.value(), ChildResult.value()));

    auto* ParentNode = static_cast<ReplicatedNode*>(ParentResult->Borrowed());
    auto* ChildNode = static_cast<ReplicatedNode*>(ChildResult->Borrowed());
    REQUIRE(ParentNode != nullptr);
    REQUIRE(ChildNode != nullptr);
    ParentNode->Replicated(true);
    ChildNode->Replicated(true);
    auto ComponentResult = ChildNode->Add<ReplicatedComponent>();
    REQUIRE(ComponentResult);
    auto* ChildComponent = &*ComponentResult;
    ChildComponent->Replicated(true);
    ChildComponent->Value = 5;
    ChildComponent->Offset = Vec3(1.0f, 0.0f, 0.0f);

    const Uuid ParentId = ParentResult->Id;
    const Uuid ChildId = ChildResult->Id;
    const Uuid ComponentId = ChildComponent->Id();

    NetReplicationBridge ServerBridge(ServerWorld);
    std::vector<ReplicationEntityState> Entities;
    ServerBridge.GatherEntities(Entities);
    REQUIRE(Entities.size() == 3);

    std::vector<ReplicationPayload> Payloads;
    Payloads.reserve(Entities.size());
    for (const auto& Entity : Entities)
    {
        ReplicationPayload Payload;
        Payload.Entity = Entity;
        REQUIRE(ServerBridge.BuildSnapshot(Entity.EntityIdValue, Entity.TypeIdValue, Payload.Bytes));
        REQUIRE(DecodeReplicationHeader(ConstByteSpan(Payload.Bytes.data(), Payload.Bytes.size()), Payload));
        Payloads.push_back(std::move(Payload));
    }

    ServerWorld.Clear();

    World ClientWorld("ClientWorld");
    NetReplicationBridge ClientBridge(ClientWorld);

    const ReplicationPayload* ParentPayload = nullptr;
    const ReplicationPayload* ChildPayload = nullptr;
    const ReplicationPayload* ComponentPayload = nullptr;

    for (const auto& Payload : Payloads)
    {
        if (Payload.Kind == 1 && Payload.ObjectId == ComponentId)
        {
            ComponentPayload = &Payload;
            continue;
        }
        if (Payload.Kind == 0 && Payload.ObjectId == ChildId)
        {
            ChildPayload = &Payload;
            continue;
        }
        if (Payload.Kind == 0 && Payload.ObjectId == ParentId)
        {
            ParentPayload = &Payload;
        }
    }

    REQUIRE(ComponentPayload);
    REQUIRE(ChildPayload);
    REQUIRE(ParentPayload);

    ClientBridge.OnSpawn(NetConnectionHandle{0},
                         ComponentPayload->Entity.EntityIdValue,
                         ComponentPayload->Entity.TypeIdValue,
                         ConstByteSpan(ComponentPayload->Bytes.data(), ComponentPayload->Bytes.size()));

    ClientBridge.OnSpawn(NetConnectionHandle{0},
                         ChildPayload->Entity.EntityIdValue,
                         ChildPayload->Entity.TypeIdValue,
                         ConstByteSpan(ChildPayload->Bytes.data(), ChildPayload->Bytes.size()));

    ClientBridge.OnSpawn(NetConnectionHandle{0},
                         ParentPayload->Entity.EntityIdValue,
                         ParentPayload->Entity.TypeIdValue,
                         ConstByteSpan(ParentPayload->Bytes.data(), ParentPayload->Bytes.size()));

    auto* ClientParentBase = ObjectRegistry::Instance().Resolve<BaseNode>(ParentId);
    auto* ClientChildBase = ObjectRegistry::Instance().Resolve<BaseNode>(ChildId);
    REQUIRE(ClientParentBase != nullptr);
    REQUIRE(ClientChildBase != nullptr);

    auto* ClientChild = dynamic_cast<ReplicatedNode*>(ClientChildBase);
    REQUIRE(ClientChild != nullptr);
    REQUIRE(ClientChild->Parent().Id == ParentId);

    auto* ClientComponentBase = ObjectRegistry::Instance().Resolve<BaseComponent>(ComponentId);
    REQUIRE(ClientComponentBase != nullptr);
    auto* ClientComponent = static_cast<ReplicatedComponent*>(ClientComponentBase);
    REQUIRE(ClientComponent != nullptr);
    REQUIRE(ClientComponent->Owner().Id == ChildId);
    REQUIRE(ClientComponent->Value == 5);
}

TEST_CASE("ReplicationService replicates node/component snapshots over a session")
{
    RegisterReplicationTestTypes();

    NetConfig Config{};
    Config.Threading.UseInternalThreads = false;
    Config.Pacing.MaxBytesPerSecond = 1024 * 1024;
    Config.Pacing.BurstBytes = 1024 * 1024;
    Config.Pacing.MaxBytesPerPump = 1024 * 1024;

    NetSession Server(Config);
    NetSession Client(Config);
    Server.Role(ESessionRole::Server);
    Client.Role(ESessionRole::Client);

    const NetEndpoint ClientEndpoint{"client", 9101};
    const NetEndpoint ServerEndpoint{"server", 9102};
    auto ClientTransport = std::make_shared<TestDatagramTransport>(1, ClientEndpoint);
    auto ServerTransport = std::make_shared<TestDatagramTransport>(2, ServerEndpoint);
    ClientTransport->Link(ServerTransport.get());
    ServerTransport->Link(ClientTransport.get());

    Client.RegisterTransport(ClientTransport);
    Server.RegisterTransport(ServerTransport);

    const NetConnectionHandle Handle = 991;
    REQUIRE(Client.OpenConnection(ClientTransport->Handle(), ServerEndpoint, Handle) == Handle);
    REQUIRE(Server.OpenConnection(ServerTransport->Handle(), ClientEndpoint, Handle) == Handle);

    auto ServerReplication = ReplicationService::Create(Server);
    auto ClientReplication = ReplicationService::Create(Client);

    World ServerWorld("ServerWorld");
    World ClientWorld("ClientWorld");
    NetReplicationBridge ServerBridge(ServerWorld);
    NetReplicationBridge ClientBridge(ClientWorld);

    ServerReplication->EntityProvider(&ServerBridge);
    ServerReplication->InterestProvider(&ServerBridge);
    ServerReplication->PriorityProvider(&ServerBridge);
    ClientReplication->Receiver(&ClientBridge);

    auto NodeResult = ServerWorld.CreateNode<ReplicatedNode>("Actor");
    REQUIRE(NodeResult);
    auto* ServerNode = static_cast<ReplicatedNode*>(NodeResult->Borrowed());
    REQUIRE(ServerNode != nullptr);
    ServerNode->Replicated(true);
    ServerNode->Health = 3;

    auto ComponentResult = ServerNode->Add<ReplicatedComponent>();
    REQUIRE(ComponentResult);
    auto* ServerComponent = &*ComponentResult;
    ServerComponent->Replicated(true);
    ServerComponent->Value = 9;
    ServerComponent->Offset = Vec3(2.0f, 0.0f, 0.0f);

    const Uuid NodeId = ServerNode->Id();
    const Uuid ComponentId = ServerComponent->Id();

    TimePoint Now{};
    PumpPair(Client, Server, Now, 12);

    auto* ClientNodeBase = ObjectRegistry::Instance().Resolve<BaseNode>(NodeId);
    auto* ClientComponentBase = ObjectRegistry::Instance().Resolve<BaseComponent>(ComponentId);
    REQUIRE(ClientNodeBase != nullptr);
    REQUIRE(ClientComponentBase != nullptr);

    auto* ClientNode = dynamic_cast<ReplicatedNode*>(ClientNodeBase);
    auto* ClientComponent = static_cast<ReplicatedComponent*>(ClientComponentBase);
    REQUIRE(ClientNode != nullptr);
    REQUIRE(ClientComponent != nullptr);
    REQUIRE(ClientNode->Health == 3);
    REQUIRE(ClientComponent->Value == 9);

    ServerNode->Health = 11;
    ServerComponent->Value = 15;
    ServerComponent->Offset = Vec3(5.0f, 6.0f, 7.0f);

    PumpPair(Client, Server, Now, 12);

    REQUIRE(ClientNode->Health == 11);
    REQUIRE(ClientComponent->Value == 15);
    REQUIRE(ClientComponent->Offset.x() == 5.0f);
    REQUIRE(ClientComponent->Offset.y() == 6.0f);
    REQUIRE(ClientComponent->Offset.z() == 7.0f);
}
