#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "IWorld.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include <Services/ReplicationService.h>
#endif

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_NETWORKING)

/**
 * @brief Reflection-driven replication bridge for world-owned graph objects.
 * @remarks
 * Adapts world-owned graph objects (nodes/components) to SnAPI.Networking replication interfaces.
 *
 * Key responsibilities:
 * - enumerate replicated entities from graph state
 * - build snapshot/delta payloads from reflected replicated fields
 * - apply incoming spawn/update/despawn payloads to local graph objects
 * - resolve parent/component ordering dependencies with pending queues
 *
 * Entity model:
 * - Node and Component entities are distinct kinds.
 * - Identity is carried by UUID + entity id mapping.
 * - Replication is gated both by field flags and runtime `Replicated(true)` object flags.
 */
class SNAPI_GAMEFRAMEWORK_API NetReplicationBridge final
    : public SnAPI::Networking::IReplicationEntityProvider
    , public SnAPI::Networking::IReplicationInterestProvider
    , public SnAPI::Networking::IReplicationPriorityProvider
    , public SnAPI::Networking::IReplicationReceiver
{
public:
    /**
 * @brief Construct a bridge for a world graph context.
 * @param WorldRef World to replicate.
 * @remarks World reference must outlive the bridge.
     */
    explicit NetReplicationBridge(IWorld& WorldRef);

    /**
     * @brief Access the replicated world context.
     */
    IWorld& World();

    /**
     * @brief Access the replicated world context (const).
     */
    const IWorld& World() const;

    // IReplicationEntityProvider
    /** @brief Enumerate currently replicated entities visible from graph state. */
    void GatherEntities(std::vector<SnAPI::Networking::ReplicationEntityState>& OutEntities) override;
    /** @brief Build full state snapshot payload for a single entity. */
    bool BuildSnapshot(SnAPI::Networking::EntityId EntityIdValue,
                       SnAPI::Networking::TypeId TypeIdValue,
                       std::vector<SnAPI::Networking::Byte>& OutSnapshot) override;
    /** @brief Build incremental delta payload from baseline to current state. */
    bool BuildDelta(SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue,
                    SnAPI::Networking::ConstByteSpan Baseline,
                    SnAPI::Networking::ReplicationDelta& OutDelta) override;

    // IReplicationInterestProvider
    /** @brief Determine whether a connection should receive updates for an entity. */
    bool Interested(SnAPI::Networking::NetConnectionHandle Handle,
                    SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue) override;

    // IReplicationPriorityProvider
    /** @brief Return replication priority score for scheduling/budgeting. */
    std::uint32_t Score(SnAPI::Networking::NetConnectionHandle Handle,
                        SnAPI::Networking::EntityId EntityIdValue,
                        SnAPI::Networking::TypeId TypeIdValue) override;

    // IReplicationReceiver
    /** @brief Apply spawn payload and create local entity if needed. */
    void OnSpawn(SnAPI::Networking::NetConnectionHandle Handle,
                 SnAPI::Networking::EntityId EntityIdValue,
                 SnAPI::Networking::TypeId TypeIdValue,
                 SnAPI::Networking::ConstByteSpan Payload) override;
    /** @brief Apply state update payload to an existing local entity. */
    void OnUpdate(SnAPI::Networking::NetConnectionHandle Handle,
                  SnAPI::Networking::EntityId EntityIdValue,
                  SnAPI::Networking::TypeId TypeIdValue,
                  SnAPI::Networking::ConstByteSpan Payload) override;
    /** @brief Despawn local entity mapped to remote entity id. */
    void OnDespawn(SnAPI::Networking::NetConnectionHandle Handle,
                   SnAPI::Networking::EntityId EntityIdValue) override;
    /** @brief Apply full snapshot payload to local entity state. */
    void OnSnapshot(SnAPI::Networking::NetConnectionHandle Handle,
                    SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue,
                    SnAPI::Networking::ConstByteSpan Payload) override;

private:
    struct EntityRef
    {
        std::uint8_t Kind = 0; /**< @brief Local object kind discriminator (node/component). */
        TypeId Type{}; /**< @brief Reflected type of mapped object. */
        BaseNode* Node = nullptr; /**< @brief Borrowed node pointer when kind is node. */
        BaseComponent* Component = nullptr; /**< @brief Borrowed component pointer when kind is component. */
    };

    struct EntityInfo
    {
        std::uint8_t Kind = 0; /**< @brief Object kind discriminator. */
        Uuid ObjectId{}; /**< @brief Stable object UUID for identity/linking. */
        TypeId Type{}; /**< @brief Reflected type id of object. */
    };

    bool ApplyPayload(SnAPI::Networking::EntityId EntityIdValue,
                      SnAPI::Networking::ConstByteSpan Payload);
    void ResolvePendingAttachments();
    void ResolvePendingComponents();

    IWorld* m_world = nullptr; /**< @brief Non-owning world context for replication operations. */
    std::unordered_map<SnAPI::Networking::EntityId, EntityRef> m_entityRefs{}; /**< @brief EntityId -> live local object references. */
    std::unordered_map<SnAPI::Networking::EntityId, EntityInfo> m_entityInfo{}; /**< @brief EntityId -> persisted identity/type metadata. */
    std::unordered_map<Uuid, Uuid, UuidHash> m_pendingParents{}; /**< @brief Child node id -> unresolved parent id map for out-of-order spawn handling. */

    struct PendingComponent
    {
        Uuid ComponentId{}; /**< @brief Component identity to instantiate once owner exists. */
        Uuid OwnerId{}; /**< @brief Owner node identity this component depends on. */
        TypeId ComponentType{}; /**< @brief Reflected component type to construct. */
        std::vector<uint8_t> FieldBytes{}; /**< @brief Serialized replicated field payload buffered until apply-time. */
    };

    std::vector<PendingComponent> m_pendingComponents{}; /**< @brief Buffered components awaiting owner-node availability. */
};

#endif // SNAPI_GF_ENABLE_NETWORKING

} // namespace SnAPI::GameFramework
