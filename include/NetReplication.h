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
 * @ingroup SnAPI_GameFramework
 * @brief Adapts a live World graph to SnAPI.Networking's replication interfaces.
 *
 * The bridge exposes replicated Nodes and Components as transport-level replication
 * entities. It is reflection-driven: users opt fields into replication through reflected
 * metadata, and this type discovers those fields at runtime and serializes them into
 * snapshot payloads understood by the networking layer.
 *
 * Design intent:
 * - keep the gameplay object model authoritative instead of introducing a second
 *   replication-only state model
 * - preserve stable object identity through UUIDs across save/load, replication, and RPC
 * - tolerate out-of-order network delivery by buffering unresolved parent-child and
 *   component-owner relationships until the missing objects arrive
 *
 * Core semantics:
 * - Nodes and Components replicate as distinct entity kinds.
 * - An object is emitted when the object itself is marked `Replicated(true)`, or when a
 *   Node owns at least one replicated Component that requires the Node to exist remotely.
 * - Parent Nodes are included as structural dependencies even when those parents are not
 *   themselves flagged for replication.
 * - `GatherEntities()` rebuilds the bridge's transient live-object table. Snapshot and
 *   delta construction rely on that table and therefore assume they are called after a
 *   fresh gather pass.
 *
 * Ownership and lifetime:
 * - The bridge does not own the bound World.
 * - Any object pointers cached internally are borrowed and only valid until the next
 *   gather pass or until the World mutates.
 *
 * Threading:
 * - Not thread-safe. Callers must serialize access.
 * - Apply callbacks mutate the bound World and therefore must only run on a thread that
 *   is allowed to create, destroy, attach, and deserialize graph objects. In normal
 *   engine usage that is the main/world thread.
 *
 * Performance:
 * - `GatherEntities()` walks the entire World graph and rebuilds transient maps.
 * - Snapshot building allocates payload buffers and serializes all replicated fields.
 * - Delta building currently rebuilds a full snapshot and compares bytes. It is not a
 *   structural field-by-field delta encoder.
 *
 * @warning The bridge keeps only transient live-object references. If the caller asks for
 * snapshots without first calling `GatherEntities()`, entity lookup may fail.
 * @see NetRpcBridge, IWorld, BaseNode, BaseComponent
 */
class SNAPI_GAMEFRAMEWORK_API NetReplicationBridge final
    : public SnAPI::Networking::IReplicationEntityProvider
    , public SnAPI::Networking::IReplicationInterestProvider
    , public SnAPI::Networking::IReplicationPriorityProvider
    , public SnAPI::Networking::IReplicationReceiver
{
public:
    /**
     * @brief Construct a replication bridge over an existing World.
     *
     * The bridge stores a non-owning pointer to @p WorldRef and immediately becomes able
     * to enumerate and mutate that World through the replication interfaces.
     *
     * @param WorldRef World instance whose Node and Component graph should be exposed to
     *        the replication layer. The caller retains ownership.
     *
     * @pre @p WorldRef must outlive this bridge.
     * @post `World()` returns @p WorldRef.
     */
    explicit NetReplicationBridge(IWorld& WorldRef);

    /**
     * @brief Access the bound World.
     * @return Non-owning reference to the World supplied at construction time.
     * @warning Undefined behavior if the bridge outlives the World.
     */
    IWorld& World();

    /**
     * @brief Access the bound World through a const view.
     * @return Non-owning reference to the World supplied at construction time.
     */
    const IWorld& World() const;

    // IReplicationEntityProvider
    /**
     * @brief Rebuild the current replication entity list from World state.
     *
     * This call scans the World graph, emits every replicated Node and Component, and
     * populates the bridge's transient `EntityId -> live object` table used by
     * `BuildSnapshot()` and `BuildDelta()`.
     *
     * Enumeration rules:
     * - Replicated Nodes are emitted directly.
     * - Non-replicated Nodes are still emitted when they are required as parents of a
     *   replicated descendant.
     * - Replicated Components are emitted after their owner Nodes.
     *
     * @param OutEntities Destination array. Existing contents are discarded.
     *
     * @post `OutEntities` contains the complete set of currently visible replication
     *       entities for this World.
     * @post Internal live-object caches reflect the same gather pass.
     */
    void GatherEntities(std::vector<SnAPI::Networking::ReplicationEntityState>& OutEntities) override;

    /**
     * @brief Serialize the current full replicated state for one entity.
     *
     * The bridge looks up the entity in the transient cache produced by the most recent
     * `GatherEntities()` call, then writes a header plus all currently replicated fields.
     * For Components, the owner Node id is included so the receiver can recreate the
     * ownership relationship.
     *
     * @param EntityIdValue Transport entity id to snapshot.
     * @param TypeIdValue Transport type id supplied by the replication service. The
     *        current implementation does not trust this value for lookup; it serializes
     *        the object resolved from the bridge's internal cache.
     * @param OutSnapshot Destination byte buffer. Existing contents are replaced.
     * @return `true` when the entity exists in the current gather cache and its replicated
     *         payload could be serialized; otherwise `false`.
     *
     * @warning This method expects `GatherEntities()` to have run for the same live
     * object set. Entities not present in the internal cache will fail to snapshot.
     */
    bool BuildSnapshot(SnAPI::Networking::EntityId EntityIdValue,
                       SnAPI::Networking::TypeId TypeIdValue,
                       std::vector<SnAPI::Networking::Byte>& OutSnapshot) override;

    /**
     * @brief Build an update payload relative to a previous baseline snapshot.
     *
     * The current implementation is byte-oriented rather than structural: it rebuilds a
     * fresh full snapshot and compares it against @p Baseline. If the bytes are
     * identical, no delta is produced. If they differ, the new full snapshot is returned
     * as both the outgoing delta payload and the new baseline.
     *
     * @param EntityIdValue Transport entity id to evaluate.
     * @param TypeIdValue Transport type id supplied by the replication service.
     * @param Baseline Previous serialized bytes for this entity.
     * @param OutDelta Filled with the outgoing payload and replacement baseline when a
     *        change is detected.
     * @return `true` when a new payload was produced, `false` when snapshot generation
     *         failed or when no byte-level change was detected.
     */
    bool BuildDelta(SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue,
                    SnAPI::Networking::ConstByteSpan Baseline,
                    SnAPI::Networking::ReplicationDelta& OutDelta) override;

    // IReplicationInterestProvider
    /**
     * @brief Decide whether a connection is interested in an entity.
     *
     * @param Handle Remote connection being evaluated.
     * @param EntityIdValue Candidate replication entity.
     * @param TypeIdValue Candidate entity type.
     * @return Always `true` in the current implementation.
     *
     * @note Interest culling is not implemented yet. Every replicated entity is treated
     * as visible to every connection.
     */
    bool Interested(SnAPI::Networking::NetConnectionHandle Handle,
                    SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue) override;

    // IReplicationPriorityProvider
    /**
     * @brief Return a scheduling score for one entity/connection pair.
     *
     * @param Handle Remote connection being evaluated.
     * @param EntityIdValue Candidate replication entity.
     * @param TypeIdValue Candidate entity type.
     * @return Always `0` in the current implementation.
     *
     * @note Priority weighting is not implemented yet. Callers should treat the current
     * score as a placeholder rather than a meaningful gameplay priority signal.
     */
    std::uint32_t Score(SnAPI::Networking::NetConnectionHandle Handle,
                        SnAPI::Networking::EntityId EntityIdValue,
                        SnAPI::Networking::TypeId TypeIdValue) override;

    // IReplicationReceiver
    /**
     * @brief Apply a spawn payload from the network.
     *
     * Spawn and update handling share the same payload application path. If the payload
     * refers to a missing Node or Component, the bridge creates it on demand. If a
     * referenced parent Node or owner Node has not arrived yet, the bridge buffers that
     * relationship and resolves it later when the dependency becomes available.
     *
     * @param Handle Source connection.
     * @param EntityIdValue Transport entity id associated with the payload.
     * @param TypeIdValue Transport type id supplied by the replication service.
     * @param Payload Serialized replication bytes previously built by a peer bridge.
     */
    void OnSpawn(SnAPI::Networking::NetConnectionHandle Handle,
                 SnAPI::Networking::EntityId EntityIdValue,
                 SnAPI::Networking::TypeId TypeIdValue,
                 SnAPI::Networking::ConstByteSpan Payload) override;

    /**
     * @brief Apply an update payload from the network.
     *
     * The bridge treats updates similarly to spawns: missing objects may be created, and
     * unresolved attachments are buffered until their dependencies exist.
     *
     * @param Handle Source connection.
     * @param EntityIdValue Transport entity id associated with the payload.
     * @param TypeIdValue Transport type id supplied by the replication service.
     * @param Payload Serialized replication bytes previously built by a peer bridge.
     */
    void OnUpdate(SnAPI::Networking::NetConnectionHandle Handle,
                  SnAPI::Networking::EntityId EntityIdValue,
                  SnAPI::Networking::TypeId TypeIdValue,
                  SnAPI::Networking::ConstByteSpan Payload) override;

    /**
     * @brief Remove the local object mapped to a remote entity id.
     *
     * For Nodes this destroys the entire Node via the World. For Components this removes
     * the Component by reflected type from its owner Node. If no mapping exists, the call
     * is ignored.
     *
     * @param Handle Source connection.
     * @param EntityIdValue Transport entity id to despawn locally.
     */
    void OnDespawn(SnAPI::Networking::NetConnectionHandle Handle,
                   SnAPI::Networking::EntityId EntityIdValue) override;

    /**
     * @brief Apply a full snapshot payload from the network.
     *
     * Snapshot handling reuses the same object-creation and field-application path as
     * spawns and updates.
     *
     * @param Handle Source connection.
     * @param EntityIdValue Transport entity id associated with the payload.
     * @param TypeIdValue Transport type id supplied by the replication service.
     * @param Payload Serialized full state payload.
     */
    void OnSnapshot(SnAPI::Networking::NetConnectionHandle Handle,
                    SnAPI::Networking::EntityId EntityIdValue,
                    SnAPI::Networking::TypeId TypeIdValue,
                    SnAPI::Networking::ConstByteSpan Payload) override;

private:
    struct EntityRef
    {
        std::uint8_t Kind = 0; /**< @brief Local object kind discriminator: node or component. */
        TypeId Type{}; /**< @brief Reflected runtime type used for snapshot serialization. */
        BaseNode* Node = nullptr; /**< @brief Borrowed Node pointer. Valid only while the cached gather pass remains current. */
        BaseComponent* Component = nullptr; /**< @brief Borrowed Component pointer. Valid only while the cached gather pass remains current. */
    };

    struct EntityInfo
    {
        std::uint8_t Kind = 0; /**< @brief Replicated object kind persisted across receive callbacks. */
        Uuid ObjectId{}; /**< @brief Stable World object UUID used to resolve or recreate the object later. */
        TypeId Type{}; /**< @brief Reflected concrete type used when recreating or removing the object. */
    };

    bool ApplyPayload(SnAPI::Networking::EntityId EntityIdValue,
                      SnAPI::Networking::ConstByteSpan Payload);
    void ResolvePendingAttachments();
    void ResolvePendingComponents();

    IWorld* m_world = nullptr; /**< @brief Non-owning World used for enumeration, creation, destruction, and deserialization. */
    std::unordered_map<SnAPI::Networking::EntityId, EntityRef> m_entityRefs{}; /**< @brief Transient transport id to live object map rebuilt on each gather pass. */
    std::unordered_map<SnAPI::Networking::EntityId, EntityInfo> m_entityInfo{}; /**< @brief Stable transport id metadata used by despawn and receive paths. */
    std::unordered_map<Uuid, Uuid, UuidHash> m_pendingParents{}; /**< @brief Child Node id to unresolved parent Node id map for out-of-order attachment replay. */

    struct PendingComponent
    {
        Uuid ComponentId{}; /**< @brief Component UUID to create once the owner Node exists. */
        Uuid OwnerId{}; /**< @brief Owner Node UUID required before the Component can be instantiated. */
        TypeId ComponentType{}; /**< @brief Reflected concrete Component type to create. */
        std::vector<uint8_t> FieldBytes{}; /**< @brief Serialized replicated fields captured until owner resolution succeeds. */
    };

    std::vector<PendingComponent> m_pendingComponents{}; /**< @brief Buffered Components waiting for owner-Node creation or arrival. */
};

#endif // SNAPI_GF_ENABLE_NETWORKING

} // namespace SnAPI::GameFramework
