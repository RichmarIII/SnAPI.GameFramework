#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "NodeGraph.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include <Services/ReplicationService.h>
#endif

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_NETWORKING)

/**
 * @brief Reflection-driven replication bridge for NodeGraph objects.
 * @remarks Implements SnAPI.Networking replication interfaces.
 */
class SNAPI_GAMEFRAMEWORK_API NetReplicationBridge final
    : public SnAPI::Networking::IReplicationEntityProvider
    , public SnAPI::Networking::IReplicationInterestProvider
    , public SnAPI::Networking::IReplicationPriorityProvider
    , public SnAPI::Networking::IReplicationReceiver
{
public:
    /**
     * @brief Construct a bridge for a node graph.
     * @param Graph Graph to replicate.
     */
    explicit NetReplicationBridge(NodeGraph& Graph);

    /**
     * @brief Access the replicated graph.
     */
    NodeGraph& Graph();

    /**
     * @brief Access the replicated graph (const).
     */
    const NodeGraph& Graph() const;

    // IReplicationEntityProvider
    void GatherEntities(std::vector<SnAPI::Networking::ReplicationEntityState>& OutEntities) override;
    bool BuildSnapshot(SnAPI::Networking::EntityId EntityIdValue,
                       SnAPI::Networking::TypeId TypeIdValue,
                       std::vector<SnAPI::Networking::Byte>& OutSnapshot) override;
    bool BuildDelta(SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue,
                    SnAPI::Networking::ConstByteSpan Baseline,
                    SnAPI::Networking::ReplicationDelta& OutDelta) override;

    // IReplicationInterestProvider
    bool Interested(SnAPI::Networking::NetConnectionHandle Handle,
                    SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue) override;

    // IReplicationPriorityProvider
    std::uint32_t Score(SnAPI::Networking::NetConnectionHandle Handle,
                        SnAPI::Networking::EntityId EntityIdValue,
                        SnAPI::Networking::TypeId TypeIdValue) override;

    // IReplicationReceiver
    void OnSpawn(SnAPI::Networking::NetConnectionHandle Handle,
                 SnAPI::Networking::EntityId EntityIdValue,
                 SnAPI::Networking::TypeId TypeIdValue,
                 SnAPI::Networking::ConstByteSpan Payload) override;
    void OnUpdate(SnAPI::Networking::NetConnectionHandle Handle,
                  SnAPI::Networking::EntityId EntityIdValue,
                  SnAPI::Networking::TypeId TypeIdValue,
                  SnAPI::Networking::ConstByteSpan Payload) override;
    void OnDespawn(SnAPI::Networking::NetConnectionHandle Handle,
                   SnAPI::Networking::EntityId EntityIdValue) override;
    void OnSnapshot(SnAPI::Networking::NetConnectionHandle Handle,
                    SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue,
                    SnAPI::Networking::ConstByteSpan Payload) override;

private:
    struct EntityRef
    {
        std::uint8_t Kind = 0;
        TypeId Type{};
        BaseNode* Node = nullptr;
        IComponent* Component = nullptr;
    };

    struct EntityInfo
    {
        std::uint8_t Kind = 0;
        Uuid ObjectId{};
        TypeId Type{};
    };

    bool ApplyPayload(SnAPI::Networking::EntityId EntityIdValue,
                      SnAPI::Networking::ConstByteSpan Payload);
    void ResolvePendingAttachments();
    void ResolvePendingComponents();

    NodeGraph* m_graph = nullptr;
    std::unordered_map<SnAPI::Networking::EntityId, EntityRef> m_entityRefs{};
    std::unordered_map<SnAPI::Networking::EntityId, EntityInfo> m_entityInfo{};
    std::unordered_map<Uuid, Uuid, UuidHash> m_pendingParents{};

    struct PendingComponent
    {
        Uuid ComponentId{};
        Uuid OwnerId{};
        TypeId ComponentType{};
        std::vector<uint8_t> FieldBytes{};
    };

    std::vector<PendingComponent> m_pendingComponents{};
};

#endif // SNAPI_GF_ENABLE_NETWORKING

} // namespace SnAPI::GameFramework
