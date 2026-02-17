#include "NetReplication.h"
#include "GameThreading.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)

#include "Profiling.h"

#include "ComponentStorage.h"
#include "IComponent.h"
#include "ObjectRegistry.h"
#include "Serialization.h"
#include "TypeRegistry.h"
#include "Uuid.h"

#include <cereal/archives/binary.hpp>

#include <NetHash.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <ostream>
#include <streambuf>
#include <unordered_map>
#include <unordered_set>

namespace SnAPI::GameFramework
{
namespace
{
using SnAPI::Networking::Byte;
using SnAPI::Networking::ByteSpan;
using SnAPI::Networking::ConstByteSpan;
using SnAPI::Networking::EntityId;
using SnAPI::Networking::NetByteReader;
using SnAPI::Networking::NetByteWriter;
using SnAPI::Networking::NetConnectionHandle;
using SnAPI::Networking::ReplicationEntityState;
using SnAPI::Networking::ReplicationDelta;

enum class ENetObjectKind : std::uint8_t
{
    Node = 0, /**< @brief Replicated entity represents a node object. */
    Component = 1, /**< @brief Replicated entity represents a component object. */
};

/**
 * @brief Replication payload header for node/component snapshots and updates.
 * @remarks Carries object identity/type and parent-owner linkage context.
 */
struct NetReplicationHeader
{
    ENetObjectKind Kind{}; /**< @brief Replicated object kind. */
    Uuid ObjectId{}; /**< @brief Replicated object identity UUID. */
    TypeId ObjectType{}; /**< @brief Reflected object type id. */
    Uuid OwnerId{}; /**< @brief Parent node UUID (for nodes) or owner node UUID (for components). */
};

/**
 * @brief Streambuf that writes archive bytes directly to vector storage.
 * @remarks Used to avoid extra copy layers during replication payload serialization.
 */
class VectorWriteStreambuf final : public std::streambuf
{
public:
    explicit VectorWriteStreambuf(std::vector<uint8_t>& Buffer)
        : m_buffer(Buffer)
    {
    }

protected:
    int_type overflow(int_type Ch) override
    {
        if (traits_type::eq_int_type(Ch, traits_type::eof()))
        {
            return traits_type::eof();
        }
        m_buffer.push_back(static_cast<uint8_t>(Ch));
        return Ch;
    }

    std::streamsize xsputn(const char* Data, std::streamsize Count) override
    {
        if (!Data || Count <= 0)
        {
            return 0;
        }
        const auto Size = static_cast<size_t>(Count);
        const size_t Offset = m_buffer.size();
        m_buffer.resize(Offset + Size);
        std::memcpy(m_buffer.data() + Offset, Data, Size);
        return Count;
    }

private:
    std::vector<uint8_t>& m_buffer; /**< @brief Destination byte vector reference. */
};

/**
 * @brief Streambuf view for reading from existing memory buffers.
 * @remarks Non-owning and used by cereal input archive decode paths.
 */
class MemoryReadStreambuf final : public std::streambuf
{
public:
    MemoryReadStreambuf(const uint8_t* Data, size_t Size)
    {
        if (!Data || Size == 0)
        {
            setg(nullptr, nullptr, nullptr);
            return;
        }
        char* Begin = const_cast<char*>(reinterpret_cast<const char*>(Data));
        setg(Begin, Begin, Begin + static_cast<std::streamsize>(Size));
    }
};

/**
 * @brief Cached descriptor for one reflected replicated field.
 * @remarks Stores direct codec pointer when available, or nested-type metadata fallback.
 */
struct ReplicatedField
{
    const FieldInfo* Field = nullptr; /**< @brief Reflected field metadata pointer. */
    const ValueCodecRegistry::CodecEntry* Codec = nullptr; /**< @brief Bound value codec when direct serialization is available. */
    TypeId NestedType{}; /**< @brief Nested type id used for recursive reflected traversal when no codec exists. */
    bool HasNested = false; /**< @brief True when nested traversal should be used for this field. */
};

/**
 * @brief Cached replicated-field plan per reflected type.
 * @remarks Invalidated when codec registry version changes.
 */
struct ReplicatedFieldCacheEntry
{
    uint32_t CodecVersion = 0; /**< @brief Value-codec registry version associated with this cache entry. */
    bool TypeFound = true; /**< @brief False when type metadata was unavailable during cache build. */
    std::vector<ReplicatedField> Fields{}; /**< @brief Flattened ordered replicated field descriptors (including bases). */
};

/**
 * @brief Helper guard for cycle-safe reflected type traversal.
 */
struct TypeVisitGuard
{
    std::unordered_map<TypeId, bool, UuidHash>& Visited; /**< @brief Shared visited-type set for current traversal. */
    const TypeId& Type; /**< @brief Type currently entering traversal. */
    bool Inserted = false; /**< @brief True when this guard inserted a fresh visited entry. */

    TypeVisitGuard(std::unordered_map<TypeId, bool, UuidHash>& InVisited, const TypeId& InType)
        : Visited(InVisited)
        , Type(InType)
    {
        Inserted = Visited.emplace(Type, true).second;
    }
};

GameMutex g_replicatedFieldMutex; /**< @brief Guards replicated-field cache map. */
std::unordered_map<TypeId, std::shared_ptr<ReplicatedFieldCacheEntry>, UuidHash> g_replicatedFieldCache; /**< @brief TypeId -> replicated-field cache entry. */

bool WriteUuid(NetByteWriter& Writer, const Uuid& Id)
{
    const auto& Bytes = Id.as_bytes();
    std::array<Byte, 16> Data{};
    for (size_t i = 0; i < Data.size(); ++i)
    {
        Data[i] = static_cast<Byte>(std::to_integer<uint8_t>(Bytes[i]));
    }
    return Writer.WriteBytes(ConstByteSpan(Data.data(), Data.size()));
}

bool ReadUuid(NetByteReader& Reader, Uuid& Out)
{
    std::array<Byte, 16> Data{};
    if (!Reader.ReadBytes(ByteSpan(Data.data(), Data.size())))
    {
        return false;
    }
    Out = Uuid(Data);
    return true;
}

bool EncodeHeader(NetByteWriter& Writer, const NetReplicationHeader& Header)
{
    return Writer.WriteU8(static_cast<std::uint8_t>(Header.Kind))
        && WriteUuid(Writer, Header.ObjectId)
        && WriteUuid(Writer, Header.ObjectType)
        && WriteUuid(Writer, Header.OwnerId);
}

bool DecodeHeader(NetByteReader& Reader, NetReplicationHeader& Header)
{
    std::uint8_t KindValue = 0;
    if (!Reader.ReadU8(KindValue))
    {
        return false;
    }
    Header.Kind = static_cast<ENetObjectKind>(KindValue);
    return ReadUuid(Reader, Header.ObjectId)
        && ReadUuid(Reader, Header.ObjectType)
        && ReadUuid(Reader, Header.OwnerId);
}

EntityId MakeEntityId(const Uuid& Id)
{
    const auto Parts = ToParts(Id);
    const uint64_t Mixed = Parts.High ^ (Parts.Low + 0x9e3779b97f4a7c15ULL + (Parts.High << 6) + (Parts.High >> 2));
    return static_cast<EntityId>(Mixed);
}

SnAPI::Networking::TypeId MakeNetTypeId(const TypeId& Type, ENetObjectKind Kind)
{
    const auto* Info = TypeRegistry::Instance().Find(Type);
    std::string Name = Info ? Info->Name : ToString(Type);
    const char* Prefix = (Kind == ENetObjectKind::Node) ? "Node:" : "Component:";
    std::string Key = std::string(Prefix) + Name;
    return SnAPI::Networking::NetHash::Hash32(Key);
}

bool HasReplicatedFields(const TypeId& Type);

void BuildReplicatedFields(const TypeId& Type, std::vector<ReplicatedField>& Out, std::unordered_map<TypeId, bool, UuidHash>& Visited)
{
    TypeVisitGuard Guard(Visited, Type);
    if (!Guard.Inserted)
    {
        return;
    }
    const auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return;
    }
    for (const auto& Base : Info->BaseTypes)
    {
        BuildReplicatedFields(Base, Out, Visited);
    }

    auto& ValueRegistry = ValueCodecRegistry::Instance();
    for (const auto& Field : Info->Fields)
    {
        if (!Field.Flags.Has(EFieldFlagBits::Replication))
        {
            continue;
        }
        ReplicatedField Entry;
        Entry.Field = &Field;
        Entry.Codec = ValueRegistry.FindEntry(Field.FieldType);
        if (!Entry.Codec)
        {
            const auto* NestedInfo = TypeRegistry::Instance().Find(Field.FieldType);
            if (NestedInfo && HasReplicatedFields(Field.FieldType))
            {
                Entry.NestedType = Field.FieldType;
                Entry.HasNested = true;
            }
        }
        Out.push_back(Entry);
    }
}

std::shared_ptr<const ReplicatedFieldCacheEntry> GetReplicatedFieldCache(const TypeId& Type)
{
    auto& ValueRegistry = ValueCodecRegistry::Instance();
    const uint32_t Version = ValueRegistry.Version();
    {
        GameLockGuard Lock(g_replicatedFieldMutex);
        auto It = g_replicatedFieldCache.find(Type);
        if (It != g_replicatedFieldCache.end() && It->second->CodecVersion == Version)
        {
            return It->second;
        }
    }

    auto Entry = std::make_shared<ReplicatedFieldCacheEntry>();
    Entry->CodecVersion = Version;
    if (TypeRegistry::Instance().Find(Type))
    {
        std::unordered_map<TypeId, bool, UuidHash> Visited;
        BuildReplicatedFields(Type, Entry->Fields, Visited);
    }
    else
    {
        Entry->TypeFound = false;
    }

    {
        GameLockGuard Lock(g_replicatedFieldMutex);
        g_replicatedFieldCache[Type] = Entry;
    }

    return Entry;
}

bool HasReplicatedFields(const TypeId& Type)
{
    auto Cache = GetReplicatedFieldCache(Type);
    return Cache && !Cache->Fields.empty();
}

TExpected<void> SerializeReplicatedFieldsRecursive(
    const TypeId& Type,
    const void* Instance,
    cereal::BinaryOutputArchive& Archive,
    const TSerializationContext& Context,
    std::unordered_map<TypeId, bool, UuidHash>& Visited)
{
    TypeVisitGuard Guard(Visited, Type);
    if (!Guard.Inserted)
    {
        return Ok();
    }

    auto Cache = GetReplicatedFieldCache(Type);
    if (!Cache || !Cache->TypeFound)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    for (const auto& Entry : Cache->Fields)
    {
        const auto* Field = Entry.Field;
        if (!Field)
        {
            continue;
        }
        const void* FieldPtr = nullptr;
        if (Field->ConstPointer)
        {
            FieldPtr = Field->ConstPointer(Instance);
        }
        if (!FieldPtr && Field->ViewGetter)
        {
            auto ViewResult = Field->ViewGetter(const_cast<void*>(Instance));
            if (!ViewResult)
            {
                return std::unexpected(ViewResult.error());
            }
            FieldPtr = ViewResult->Borrowed();
        }
        if (!FieldPtr && Field->Getter)
        {
            auto FieldResult = Field->Getter(const_cast<void*>(Instance));
            if (!FieldResult)
            {
                return std::unexpected(FieldResult.error());
            }
            FieldPtr = FieldResult->Borrowed();
        }
        if (!FieldPtr)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null field pointer"));
        }

        if (Entry.Codec)
        {
            auto EncodeResult = Entry.Codec->Encode(FieldPtr, Archive, Context);
            if (!EncodeResult)
            {
                return EncodeResult;
            }
        }
        else if (Entry.HasNested)
        {
            auto NestedResult = SerializeReplicatedFieldsRecursive(Entry.NestedType, FieldPtr, Archive, Context, Visited);
            if (!NestedResult)
            {
                return NestedResult;
            }
        }
        else
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "No serializer for replicated field"));
        }
    }

    return Ok();
}

TExpected<void> DeserializeReplicatedFieldsRecursive(
    const TypeId& Type,
    void* Instance,
    cereal::BinaryInputArchive& Archive,
    const TSerializationContext& Context,
    std::unordered_map<TypeId, bool, UuidHash>& Visited)
{
    TypeVisitGuard Guard(Visited, Type);
    if (!Guard.Inserted)
    {
        return Ok();
    }

    auto Cache = GetReplicatedFieldCache(Type);
    if (!Cache || !Cache->TypeFound)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    for (const auto& Entry : Cache->Fields)
    {
        const auto* Field = Entry.Field;
        if (!Field)
        {
            continue;
        }
        void* FieldPtr = nullptr;
        if (Field->MutablePointer)
        {
            FieldPtr = Field->MutablePointer(Instance);
        }
        if (!FieldPtr && Field->ViewGetter)
        {
            auto ViewResult = Field->ViewGetter(Instance);
            if (!ViewResult)
            {
                return std::unexpected(ViewResult.error());
            }
            if (ViewResult->IsConst())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot mutate const field"));
            }
            FieldPtr = ViewResult->BorrowedMutable();
        }
        if (!FieldPtr && Field->Getter)
        {
            auto FieldResult = Field->Getter(Instance);
            if (!FieldResult)
            {
                return std::unexpected(FieldResult.error());
            }
            if (FieldResult->IsConst())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot mutate const field"));
            }
            FieldPtr = const_cast<void*>(FieldResult->Borrowed());
        }
        if (Field->IsConst)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot mutate const field"));
        }
        if (!FieldPtr)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null field pointer"));
        }

        if (Entry.Codec)
        {
            auto DecodeResult = Entry.Codec->DecodeInto
                ? Entry.Codec->DecodeInto(FieldPtr, Archive, Context)
                : ValueCodecRegistry::Instance().DecodeInto(Field->FieldType, FieldPtr, Archive, Context);
            if (!DecodeResult)
            {
                return DecodeResult;
            }
        }
        else if (Entry.HasNested)
        {
            auto NestedResult = DeserializeReplicatedFieldsRecursive(Entry.NestedType, FieldPtr, Archive, Context, Visited);
            if (!NestedResult)
            {
                return NestedResult;
            }
        }
        else
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "No deserializer for replicated field"));
        }
    }

    return Ok();
}

TExpected<void> SerializeReplicatedFields(const TypeId& Type,
                                          const void* Instance,
                                          std::vector<uint8_t>& OutBytes,
                                          const TSerializationContext& Context)
{
    if (!Instance)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
    }
    OutBytes.clear();
    VectorWriteStreambuf Buffer(OutBytes);
    std::ostream Os(&Buffer);
    cereal::BinaryOutputArchive Archive(Os);
    std::unordered_map<TypeId, bool, UuidHash> Visited;
    return SerializeReplicatedFieldsRecursive(Type, Instance, Archive, Context, Visited);
}

TExpected<void> DeserializeReplicatedFields(const TypeId& Type,
                                            void* Instance,
                                            const uint8_t* Data,
                                            size_t Size,
                                            const TSerializationContext& Context)
{
    if (!Instance)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
    }
    if (!Data && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null data"));
    }
    MemoryReadStreambuf Buffer(Data, Size);
    std::istream Is(&Buffer);
    cereal::BinaryInputArchive Archive(Is);
    std::unordered_map<TypeId, bool, UuidHash> Visited;
    return DeserializeReplicatedFieldsRecursive(Type, Instance, Archive, Context, Visited);
}

} // namespace

NetReplicationBridge::NetReplicationBridge(NodeGraph& Graph)
    : m_graph(&Graph)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
}

NodeGraph& NetReplicationBridge::Graph()
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    return *m_graph;
}

const NodeGraph& NetReplicationBridge::Graph() const
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    return *m_graph;
}

void NetReplicationBridge::GatherEntities(std::vector<ReplicationEntityState>& OutEntities)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    OutEntities.clear();
    m_entityRefs.clear();

    if (!m_graph)
    {
        return;
    }

    std::unordered_set<EntityId> AddedNodes;
    std::vector<BaseNode*> ReplicatedNodes;

    auto AddNodeEntity = [&](BaseNode& Node) {
        const EntityId NodeEntityId = MakeEntityId(Node.Id());
        if (!AddedNodes.insert(NodeEntityId).second)
        {
            return;
        }
        const SnAPI::Networking::TypeId NodeTypeId = MakeNetTypeId(Node.TypeKey(), ENetObjectKind::Node);
        OutEntities.push_back({NodeEntityId, NodeTypeId});
        m_entityRefs.emplace(NodeEntityId,
                             EntityRef{static_cast<std::uint8_t>(ENetObjectKind::Node), Node.TypeKey(), &Node, nullptr});
    };

    auto AddNodeWithParents = [&](BaseNode& Node) {
        BaseNode* Current = &Node;
        while (Current)
        {
            AddNodeEntity(*Current);
            if (Current->Parent().IsNull())
            {
                break;
            }
            Current = Current->Parent().Borrowed();
        }
    };

    m_graph->NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        bool ShouldReplicateNode = Node.Replicated();
        bool HasReplicatedComponent = false;
        for (const auto& Type : Node.ComponentTypes())
        {
            void* ComponentPtr = m_graph->BorrowedComponent(Handle, Type);
            auto* Component = static_cast<IComponent*>(ComponentPtr);
            if (Component && Component->Replicated())
            {
                HasReplicatedComponent = true;
                break;
            }
        }
        if (!ShouldReplicateNode && !HasReplicatedComponent)
        {
            return;
        }
        AddNodeWithParents(Node);
        ReplicatedNodes.push_back(&Node);
    });

    for (auto* Node : ReplicatedNodes)
    {
        if (!Node)
        {
            continue;
        }
        NodeHandle Handle = Node->Handle();
        for (const auto& Type : Node->ComponentTypes())
        {
            void* ComponentPtr = m_graph->BorrowedComponent(Handle, Type);
            auto* Component = static_cast<IComponent*>(ComponentPtr);
            if (!Component || !Component->Replicated())
            {
                continue;
            }
            const EntityId ComponentEntityId = MakeEntityId(Component->Id());
            const SnAPI::Networking::TypeId ComponentTypeId = MakeNetTypeId(Type, ENetObjectKind::Component);
            OutEntities.push_back({ComponentEntityId, ComponentTypeId});
            m_entityRefs.emplace(ComponentEntityId,
                                 EntityRef{static_cast<std::uint8_t>(ENetObjectKind::Component), Type, nullptr, Component});
        }
    }
}

bool NetReplicationBridge::BuildSnapshot(EntityId EntityIdValue,
                                         SnAPI::Networking::TypeId,
                                         std::vector<Byte>& OutSnapshot)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    auto It = m_entityRefs.find(EntityIdValue);
    if (It == m_entityRefs.end())
    {
        return false;
    }

    const auto& Ref = It->second;
    TSerializationContext Context;
    Context.Graph = m_graph;

    std::vector<uint8_t> FieldBytes;
    NetReplicationHeader Header{};

    if (Ref.Kind == static_cast<std::uint8_t>(ENetObjectKind::Node))
    {
        if (!Ref.Node)
        {
            return false;
        }
        Header.Kind = ENetObjectKind::Node;
        Header.ObjectId = Ref.Node->Id();
        Header.ObjectType = Ref.Type;
        Header.OwnerId = Ref.Node->Parent().Id;
        if (Ref.Node->Replicated())
        {
            auto Result = SerializeReplicatedFields(Ref.Type, Ref.Node, FieldBytes, Context);
            if (!Result)
            {
                return false;
            }
        }
    }
    else
    {
        if (!Ref.Component)
        {
            return false;
        }
        Header.Kind = ENetObjectKind::Component;
        Header.ObjectId = Ref.Component->Id();
        Header.ObjectType = Ref.Type;
        Header.OwnerId = Ref.Component->Owner().Id;
        if (Ref.Component->Replicated())
        {
            auto Result = SerializeReplicatedFields(Ref.Type, Ref.Component, FieldBytes, Context);
            if (!Result)
            {
                return false;
            }
        }
    }

    OutSnapshot.clear();
    NetByteWriter Writer(OutSnapshot);
    if (!EncodeHeader(Writer, Header))
    {
        return false;
    }
    if (!FieldBytes.empty())
    {
        OutSnapshot.insert(OutSnapshot.end(), FieldBytes.begin(), FieldBytes.end());
    }
    return true;
}

bool NetReplicationBridge::BuildDelta(EntityId EntityIdValue,
                                      SnAPI::Networking::TypeId TypeIdValue,
                                      ConstByteSpan Baseline,
                                      ReplicationDelta& OutDelta)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    std::vector<Byte> Snapshot;
    if (!BuildSnapshot(EntityIdValue, TypeIdValue, Snapshot))
    {
        return false;
    }
    if (Snapshot.size() == Baseline.size() &&
        (Snapshot.empty() || std::memcmp(Snapshot.data(), Baseline.data(), Snapshot.size()) == 0))
    {
        return false;
    }
    OutDelta.Delta = Snapshot;
    OutDelta.Baseline = std::move(Snapshot);
    return true;
}

bool NetReplicationBridge::Interested(NetConnectionHandle,
                                      EntityId,
                                      SnAPI::Networking::TypeId)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    return true;
}

std::uint32_t NetReplicationBridge::Score(NetConnectionHandle,
                                          EntityId,
                                          SnAPI::Networking::TypeId)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    return 0;
}

void NetReplicationBridge::OnSpawn(NetConnectionHandle,
                                   EntityId EntityIdValue,
                                   SnAPI::Networking::TypeId,
                                   ConstByteSpan Payload)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    (void)EntityIdValue;
    (void)ApplyPayload(EntityIdValue, Payload);
}

void NetReplicationBridge::OnUpdate(NetConnectionHandle,
                                    EntityId EntityIdValue,
                                    SnAPI::Networking::TypeId,
                                    ConstByteSpan Payload)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    (void)EntityIdValue;
    (void)ApplyPayload(EntityIdValue, Payload);
}

void NetReplicationBridge::OnDespawn(NetConnectionHandle,
                                     EntityId EntityIdValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    if (!m_graph)
    {
        return;
    }
    auto It = m_entityInfo.find(EntityIdValue);
    if (It == m_entityInfo.end())
    {
        return;
    }

    const auto& Info = It->second;
    if (Info.Kind == static_cast<std::uint8_t>(ENetObjectKind::Node))
    {
        auto* Node = ObjectRegistry::Instance().Resolve<BaseNode>(Info.ObjectId);
        if (Node)
        {
            m_graph->DestroyNode(Node->Handle());
        }
    }
    else
    {
        auto* Component = ObjectRegistry::Instance().Resolve<IComponent>(Info.ObjectId);
        if (Component)
        {
            NodeHandle Owner = Component->Owner();
            if (auto* Storage = m_graph->Storage(Info.Type))
            {
                Storage->Remove(Owner);
                if (auto* OwnerNode = Owner.Borrowed())
                {
                    m_graph->UnregisterComponentOnNode(*OwnerNode, Info.Type);
                }
            }
        }
    }

    m_entityInfo.erase(It);
}

void NetReplicationBridge::OnSnapshot(NetConnectionHandle,
                                      EntityId EntityIdValue,
                                      SnAPI::Networking::TypeId,
                                      ConstByteSpan Payload)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    (void)EntityIdValue;
    (void)ApplyPayload(EntityIdValue, Payload);
}

bool NetReplicationBridge::ApplyPayload(EntityId EntityIdValue,
                                        ConstByteSpan Payload)
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    if (!m_graph)
    {
        return false;
    }

    NetByteReader Reader(Payload);
    NetReplicationHeader Header{};
    if (!DecodeHeader(Reader, Header))
    {
        return false;
    }

    const size_t PayloadOffset = Reader.Position();
    if (PayloadOffset > Payload.size())
    {
        return false;
    }
    const uint8_t* FieldData = Payload.data() + PayloadOffset;
    const size_t FieldSize = Payload.size() - PayloadOffset;

    TSerializationContext Context;
    Context.Graph = m_graph;

    if (Header.Kind == ENetObjectKind::Node)
    {
        BaseNode* Node = ObjectRegistry::Instance().Resolve<BaseNode>(Header.ObjectId);
        if (!Node)
        {
            auto CreateResult = m_graph->CreateNode(Header.ObjectType, "Node", Header.ObjectId);
            if (!CreateResult)
            {
                return false;
            }
            Node = ObjectRegistry::Instance().Resolve<BaseNode>(Header.ObjectId);
        }

        if (Node && Header.ObjectType != TypeId{} && Node->TypeKey() != Header.ObjectType)
        {
            Node->TypeKey(Header.ObjectType);
        }

        if (!Header.OwnerId.is_nil())
        {
            auto* Parent = ObjectRegistry::Instance().Resolve<BaseNode>(Header.OwnerId);
            if (Parent)
            {
                if (Node && Node->Parent() != Parent->Handle())
                {
                    if (!Node->Parent().IsNull())
                    {
                        m_graph->DetachChild(Node->Handle());
                    }
                    m_graph->AttachChild(Parent->Handle(), Node->Handle());
                }
            }
            else
            {
                m_pendingParents[Header.ObjectId] = Header.OwnerId;
            }
        }
        else if (Node && !Node->Parent().IsNull())
        {
            m_graph->DetachChild(Node->Handle());
        }

        if (Node && FieldSize > 0)
        {
            auto Result = DeserializeReplicatedFields(Header.ObjectType, Node, FieldData, FieldSize, Context);
            if (!Result)
            {
                return false;
            }
        }
        m_entityInfo[EntityIdValue] = EntityInfo{static_cast<std::uint8_t>(ENetObjectKind::Node),
                                                 Header.ObjectId,
                                                 Header.ObjectType};
        ResolvePendingAttachments();
        ResolvePendingComponents();
        return true;
    }

    if (Header.Kind == ENetObjectKind::Component)
    {
        IComponent* Component = ObjectRegistry::Instance().Resolve<IComponent>(Header.ObjectId);
        if (!Component)
        {
            auto* OwnerNode = ObjectRegistry::Instance().Resolve<BaseNode>(Header.OwnerId);
            if (!OwnerNode)
            {
                PendingComponent Pending{};
                Pending.ComponentId = Header.ObjectId;
                Pending.OwnerId = Header.OwnerId;
                Pending.ComponentType = Header.ObjectType;
                Pending.FieldBytes.assign(FieldData, FieldData + FieldSize);
                m_pendingComponents.push_back(std::move(Pending));
                return true;
            }

            auto CreateResult = ComponentSerializationRegistry::Instance()
                .CreateWithId(*m_graph, OwnerNode->Handle(), Header.ObjectType, Header.ObjectId);
            if (!CreateResult)
            {
                return false;
            }
            Component = ObjectRegistry::Instance().Resolve<IComponent>(Header.ObjectId);
        }

        if (Component && FieldSize > 0)
        {
            auto Result = DeserializeReplicatedFields(Header.ObjectType, Component, FieldData, FieldSize, Context);
            if (!Result)
            {
                return false;
            }
        }
        m_entityInfo[EntityIdValue] = EntityInfo{static_cast<std::uint8_t>(ENetObjectKind::Component),
                                                 Header.ObjectId,
                                                 Header.ObjectType};
        return true;
    }

    return false;
}

void NetReplicationBridge::ResolvePendingAttachments()
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    if (!m_graph || m_pendingParents.empty())
    {
        return;
    }
    for (auto It = m_pendingParents.begin(); It != m_pendingParents.end();)
    {
        const Uuid ChildId = It->first;
        const Uuid ParentId = It->second;
        auto* Child = ObjectRegistry::Instance().Resolve<BaseNode>(ChildId);
        auto* Parent = ObjectRegistry::Instance().Resolve<BaseNode>(ParentId);
        if (!Child || !Parent)
        {
            ++It;
            continue;
        }
        if (Child->Parent() != Parent->Handle())
        {
            if (!Child->Parent().IsNull())
            {
                m_graph->DetachChild(Child->Handle());
            }
            m_graph->AttachChild(Parent->Handle(), Child->Handle());
        }
        It = m_pendingParents.erase(It);
    }
}

void NetReplicationBridge::ResolvePendingComponents()
{
    SNAPI_GF_PROFILE_FUNCTION("Networking");
    if (!m_graph || m_pendingComponents.empty())
    {
        return;
    }

    auto It = m_pendingComponents.begin();
    while (It != m_pendingComponents.end())
    {
        auto* OwnerNode = ObjectRegistry::Instance().Resolve<BaseNode>(It->OwnerId);
        if (!OwnerNode)
        {
            ++It;
            continue;
        }

        auto CreateResult = ComponentSerializationRegistry::Instance()
            .CreateWithId(*m_graph, OwnerNode->Handle(), It->ComponentType, It->ComponentId);
        if (!CreateResult)
        {
            ++It;
            continue;
        }

        auto* Component = ObjectRegistry::Instance().Resolve<IComponent>(It->ComponentId);
        if (Component)
        {
            TSerializationContext Context;
            Context.Graph = m_graph;
            if (!It->FieldBytes.empty())
            {
                auto Result = DeserializeReplicatedFields(It->ComponentType,
                                                          Component,
                                                          It->FieldBytes.data(),
                                                          It->FieldBytes.size(),
                                                          Context);
                if (!Result)
                {
                    ++It;
                    continue;
                }
            }
        }

        It = m_pendingComponents.erase(It);
    }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_NETWORKING
