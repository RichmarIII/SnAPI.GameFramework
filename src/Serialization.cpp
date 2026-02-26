#include "Serialization.h"
#include "GameThreading.h"

#include <array>
#include <cstring>
#include <exception>
#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <streambuf>
#include <unordered_map>

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "Math.h"
#include "Level.h"
#include "NodeCast.h"
#include "PawnBase.h"
#include "PlayerStart.h"
#include "Relevance.h"
#include "ScriptComponent.h"
#include "TransformComponent.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"
#include "World.h"

namespace cereal
{

/**
 * @brief cereal save function for Uuid.
 * @param ArchiveRef Output archive.
 * @param Id UUID to serialize.
 * @remarks Stored as 16 raw bytes.
 */
template <class Archive>
void save(Archive& ArchiveRef, const SnAPI::GameFramework::Uuid& Id)
{
    std::array<uint8_t, 16> Data{};
    const auto& Bytes = Id.as_bytes();
    for (size_t i = 0; i < Data.size(); ++i)
    {
        Data[i] = static_cast<uint8_t>(std::to_integer<uint8_t>(Bytes[i]));
    }
    ArchiveRef(Data);
}

/**
 * @brief cereal load function for Uuid.
 * @param ArchiveRef Input archive.
 * @param Id UUID to deserialize into.
 * @remarks Reads 16 raw bytes.
 */
template <class Archive>
void load(Archive& ArchiveRef, SnAPI::GameFramework::Uuid& Id)
{
    std::array<uint8_t, 16> Data{};
    ArchiveRef(Data);
    Id = SnAPI::GameFramework::Uuid(Data);
}

} // namespace cereal

namespace SnAPI::GameFramework
{

namespace
{
/**
 * @brief Streambuf that appends cereal output bytes directly into a vector.
 * @remarks Eliminates intermediate stream copies for payload serialization.
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
 * @brief Streambuf adapter exposing immutable memory as input stream.
 * @remarks Non-owning view used by cereal binary input archive decode paths.
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
} // namespace

/**
 * @brief cereal serialize for NodeComponentPayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, NodeComponentPayload& Value)
{
    ArchiveRef(Value.ComponentId, Value.ComponentType, Value.Bytes);
}

/**
 * @brief cereal serialize for NodePayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, NodePayload& Value)
{
    ArchiveRef(Value.NodeId,
        Value.NodeType,
        Value.NodeTypeName,
        Value.Name,
        Value.Active,
        Value.HasNodeData,
        Value.NodeBytes,
        Value.Components,
        Value.Children);
}

/**
 * @brief cereal serialize for LevelPayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, LevelPayload& Value)
{
    ArchiveRef(Value.Name, Value.Nodes);
}

/**
 * @brief cereal serialize for WorldPayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, WorldPayload& Value)
{
    ArchiveRef(Value.Name, Value.Nodes);
}

namespace
{
struct SerializableField
{
    const FieldInfo* Field = nullptr; /**< @brief Reflected field metadata pointer. */
    const ValueCodecRegistry::CodecEntry* Codec = nullptr; /**< @brief Value codec pointer when direct codec serialization is available. */
    TypeId NestedType{}; /**< @brief Nested reflected type id for recursive traversal fallback. */
    bool HasNested = false; /**< @brief True when nested reflected traversal is required. */
};

struct SerializableFieldCacheEntry
{
    uint32_t CodecVersion = 0; /**< @brief Value-codec registry version used to build this cache. */
    bool TypeFound = true; /**< @brief False when type metadata was not registered at build time. */
    std::vector<SerializableField> Fields; /**< @brief Flattened ordered field plan for serializer traversal. */
};

struct TypeVisitGuard
{
    std::unordered_map<TypeId, bool, UuidHash>& Visited; /**< @brief Shared visited-type set for recursion/cycle detection. */
    TypeId Type{}; /**< @brief Type currently being traversed. */
    bool Inserted = false; /**< @brief True when this guard inserted new visited entry. */

    TypeVisitGuard(std::unordered_map<TypeId, bool, UuidHash>& InVisited, const TypeId& InType)
        : Visited(InVisited)
        , Type(InType)
    {
        Inserted = Visited.emplace(Type, true).second;
    }

    ~TypeVisitGuard()
    {
        if (Inserted)
        {
            Visited.erase(Type);
        }
    }
};

std::unordered_map<TypeId, std::shared_ptr<SerializableFieldCacheEntry>, UuidHash> g_serializableFieldCache; /**< @brief TypeId -> cached serializable field plan. */
GameMutex g_serializableFieldMutex; /**< @brief Guards serializable field cache map. */

void BuildSerializableFields(
    const TypeId& Type,
    std::vector<SerializableField>& Out,
    std::unordered_map<TypeId, bool, UuidHash>& Visited)
{
    auto [It, Inserted] = Visited.emplace(Type, true);
    if (!Inserted)
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
        BuildSerializableFields(Base, Out, Visited);
    }

    auto& ValueRegistry = ValueCodecRegistry::Instance();
    for (const auto& Field : Info->Fields)
    {
        SerializableField Entry;
        Entry.Field = &Field;
        Entry.Codec = ValueRegistry.FindEntry(Field.FieldType);
        if (!Entry.Codec)
        {
            const auto* NestedInfo = TypeRegistry::Instance().Find(Field.FieldType);
            if (NestedInfo && (!NestedInfo->Fields.empty() || !NestedInfo->BaseTypes.empty()))
            {
                Entry.NestedType = Field.FieldType;
                Entry.HasNested = true;
            }
        }
        Out.push_back(Entry);
    }
}

std::shared_ptr<const SerializableFieldCacheEntry> GetSerializableFieldCache(const TypeId& Type)
{
    auto& ValueRegistry = ValueCodecRegistry::Instance();
    const uint32_t Version = ValueRegistry.Version();
    {
        GameLockGuard Lock(g_serializableFieldMutex);
        auto It = g_serializableFieldCache.find(Type);
        if (It != g_serializableFieldCache.end() && It->second->CodecVersion == Version)
        {
            return It->second;
        }
    }

    auto Entry = std::make_shared<SerializableFieldCacheEntry>();
    Entry->CodecVersion = Version;
    if (TypeRegistry::Instance().Find(Type))
    {
        std::unordered_map<TypeId, bool, UuidHash> Visited;
        BuildSerializableFields(Type, Entry->Fields, Visited);
    }
    else
    {
        Entry->TypeFound = false;
    }

    {
        GameLockGuard Lock(g_serializableFieldMutex);
        g_serializableFieldCache[Type] = Entry;
    }

    return Entry;
}

bool HasSerializableFields(const TypeId& Type)
{
    auto Cache = GetSerializableFieldCache(Type);
    return Cache && !Cache->Fields.empty();
}

/**
 * @brief Serialize fields recursively for a type and its bases.
 * @param Type TypeId to serialize.
 * @param Instance Pointer to instance.
 * @param Archive Output archive.
 * @param Context Serialization context.
 * @param Visited Cycle guard for type traversal.
 * @return Success or error.
 */
TExpected<void> SerializeFieldsRecursive(
    const TypeId& Type,
    const void* Instance,
    cereal::BinaryOutputArchive& Archive,
    const TSerializationContext& Context,
    std::unordered_map<TypeId, bool, UuidHash>& Visited)
{
    TypeVisitGuard Visit(Visited, Type);
    if (!Visit.Inserted)
    {
        return Ok();
    }

    auto Cache = GetSerializableFieldCache(Type);
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
            auto NestedResult = SerializeFieldsRecursive(Entry.NestedType, FieldPtr, Archive, Context, Visited);
            if (!NestedResult)
            {
                return NestedResult;
            }
        }
        else
        {
            std::string FieldTypeName = "<unknown>";
            if (const auto* FieldTypeInfo = TypeRegistry::Instance().Find(Field->FieldType))
            {
                FieldTypeName = FieldTypeInfo->Name;
            }
            return std::unexpected(
                MakeError(EErrorCode::NotFound,
                          "No serializer for field '" + Field->Name + "' of type '" + FieldTypeName + "'"));
        }
    }

    return Ok();
}

/**
 * @brief Deserialize fields recursively for a type and its bases.
 * @param Type TypeId to deserialize.
 * @param Instance Pointer to instance.
 * @param Archive Input archive.
 * @param Context Serialization context.
 * @param Visited Cycle guard for type traversal.
 * @return Success or error.
 */
TExpected<void> DeserializeFieldsRecursive(
    const TypeId& Type,
    void* Instance,
    cereal::BinaryInputArchive& Archive,
    const TSerializationContext& Context,
    std::unordered_map<TypeId, bool, UuidHash>& Visited)
{
    TypeVisitGuard Visit(Visited, Type);
    if (!Visit.Inserted)
    {
        return Ok();
    }

    auto Cache = GetSerializableFieldCache(Type);
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
            auto NestedResult = DeserializeFieldsRecursive(Entry.NestedType, FieldPtr, Archive, Context, Visited);
            if (!NestedResult)
            {
                return NestedResult;
            }
        }
        else
        {
            std::string FieldTypeName = "<unknown>";
            if (const auto* FieldTypeInfo = TypeRegistry::Instance().Find(Field->FieldType))
            {
                FieldTypeName = FieldTypeInfo->Name;
            }
            return std::unexpected(
                MakeError(EErrorCode::NotFound,
                          "No deserializer for field '" + Field->Name + "' of type '" + FieldTypeName + "'"));
        }
    }

    return Ok();
}
} // namespace

ValueCodecRegistry& ValueCodecRegistry::Instance()
{
    static ValueCodecRegistry Instance;
    return Instance;
}

const ValueCodecRegistry::CodecEntry* ValueCodecRegistry::FindEntry(const TypeId& Type) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end())
    {
        return nullptr;
    }
    return &It->second;
}

TExpected<void> ValueCodecRegistry::Encode(const TypeId& Type, const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) const
{
    auto* Entry = FindEntry(Type);
    if (!Entry || !Entry->Encode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No value codec registered"));
    }
    return Entry->Encode(Value, Archive, Context);
}

TExpected<Variant> ValueCodecRegistry::Decode(const TypeId& Type, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) const
{
    auto* Entry = FindEntry(Type);
    if (!Entry || !Entry->Decode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No value codec registered"));
    }
    return Entry->Decode(Archive, Context);
}

TExpected<void> ValueCodecRegistry::DecodeInto(
    const TypeId& Type,
    void* Value,
    cereal::BinaryInputArchive& Archive,
    const TSerializationContext& Context) const
{
    auto* Entry = FindEntry(Type);
    if (!Entry || !Entry->DecodeInto)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No value codec registered"));
    }
    return Entry->DecodeInto(Value, Archive, Context);
}

ComponentSerializationRegistry& ComponentSerializationRegistry::Instance()
{
    static ComponentSerializationRegistry Instance;
    return Instance;
}

TExpected<void*> ComponentSerializationRegistry::Create(IWorld& WorldRef, const NodeHandle& Owner, const TypeId& Type) const
{
    CreateFn CreateValue;
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            CreateValue = It->second.Create;
        }
    }
    if (!CreateValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            CreateValue = It->second.Create;
        }
    }
    if (!CreateValue)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component factory registered"));
    }
    return CreateValue(WorldRef, Owner);
}

TExpected<void*> ComponentSerializationRegistry::CreateWithId(IWorld& WorldRef, const NodeHandle& Owner, const TypeId& Type, const Uuid& Id) const
{
    CreateWithIdFn CreateValue;
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            CreateValue = It->second.CreateWithId;
        }
    }
    if (!CreateValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            CreateValue = It->second.CreateWithId;
        }
    }
    if (!CreateValue)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component factory registered"));
    }
    return CreateValue(WorldRef, Owner, Id);
}

TExpected<void> ComponentSerializationRegistry::Serialize(const TypeId& Type, const void* Instance, std::vector<uint8_t>& OutBytes, const TSerializationContext& Context) const
{
    SerializeFn SerializeValue;
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            SerializeValue = It->second.Serialize;
        }
    }
    if (!SerializeValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            SerializeValue = It->second.Serialize;
        }
    }
    if (!SerializeValue)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component serializer registered"));
    }
    try
    {
        OutBytes.clear();
        VectorWriteStreambuf Buffer(OutBytes);
        std::ostream Os(&Buffer);
        cereal::BinaryOutputArchive Archive(Os);
        auto Result = SerializeValue(Instance, Archive, Context);
        if (!Result)
        {
            return Result;
        }
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<void> ComponentSerializationRegistry::Deserialize(const TypeId& Type, void* Instance, const uint8_t* Bytes, size_t Size, const TSerializationContext& Context) const
{
    DeserializeFn DeserializeValue;
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            DeserializeValue = It->second.Deserialize;
        }
    }
    if (!DeserializeValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            DeserializeValue = It->second.Deserialize;
        }
    }
    if (!DeserializeValue)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component serializer registered"));
    }
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null component bytes"));
    }
    try
    {
        MemoryReadStreambuf Buffer(Bytes, Size);
        std::istream Is(&Buffer);
        cereal::BinaryInputArchive Archive(Is);
        return DeserializeValue(Instance, Archive, Context);
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<void> ComponentSerializationRegistry::SerializeByReflection(const TypeId& Type, const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)
{
    if (!Instance)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null component instance"));
    }
    std::unordered_map<TypeId, bool, UuidHash> Visited;
    return SerializeFieldsRecursive(Type, Instance, Archive, Context, Visited);
}

TExpected<void> ComponentSerializationRegistry::DeserializeByReflection(const TypeId& Type, void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)
{
    if (!Instance)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null component instance"));
    }
    std::unordered_map<TypeId, bool, UuidHash> Visited;
    return DeserializeFieldsRecursive(Type, Instance, Archive, Context, Visited);
}

namespace
{
struct PendingNodeDeserialize
{
    const NodePayload* Payload = nullptr;
    NodeHandle Handle{};
    TypeId NodeType{};
};

struct TObjectIdRemap
{
    std::unordered_map<Uuid, Uuid, UuidHash> NodeIds{};
    std::unordered_map<Uuid, Uuid, UuidHash> ComponentIds{};
};

[[nodiscard]] BaseNode* ResolveNodeForPayload(const NodeHandle& Handle, const IWorld* WorldRef)
{
    if (BaseNode* Node = Handle.Borrowed())
    {
        return Node;
    }

    if (WorldRef && !Handle.Id.is_nil())
    {
        auto WorldHandle = WorldRef->NodeHandleById(Handle.Id);
        if (WorldHandle)
        {
            return WorldHandle->Borrowed();
        }
    }

    if (!Handle.Id.is_nil())
    {
        if (BaseNode* Node = Handle.BorrowedSlowByUuid())
        {
            return Node;
        }
    }

    return nullptr;
}

[[nodiscard]] std::size_t CountPayloadNodes(const NodePayload& Payload)
{
    std::size_t Count = 1;
    for (const NodePayload& Child : Payload.Children)
    {
        Count += CountPayloadNodes(Child);
    }
    return Count;
}

void BuildNodePayloadObjectIdRemapRecursive(const NodePayload& Payload, TObjectIdRemap& OutRemap)
{
    if (!Payload.NodeId.is_nil())
    {
        OutRemap.NodeIds.try_emplace(Payload.NodeId, NewUuid());
    }

    for (const NodeComponentPayload& ComponentPayload : Payload.Components)
    {
        if (!ComponentPayload.ComponentId.is_nil())
        {
            OutRemap.ComponentIds.try_emplace(ComponentPayload.ComponentId, NewUuid());
        }
    }

    for (const NodePayload& Child : Payload.Children)
    {
        BuildNodePayloadObjectIdRemapRecursive(Child, OutRemap);
    }
}

TExpected<TypeId> ResolveNodeTypeFromPayload(const NodePayload& Payload)
{
    TypeId ResolvedType = Payload.NodeType;
    if (ResolvedType == TypeId{} && !Payload.NodeTypeName.empty())
    {
        ResolvedType = TypeIdFromName(Payload.NodeTypeName);
    }

    if (ResolvedType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node payload does not include a valid type"));
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(ResolvedType);
    if (!Type)
    {
        std::string Message = "Node type is not registered";
        if (!Payload.NodeTypeName.empty())
        {
            Message += ": " + Payload.NodeTypeName;
        }
        return std::unexpected(MakeError(EErrorCode::NotFound, std::move(Message)));
    }

    if (!TypeRegistry::Instance().IsA(ResolvedType, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Serialized type is not a node type"));
    }

    return ResolvedType;
}

TExpected<NodePayload> SerializeNodePayloadRecursive(const BaseNode& NodeRef, const TSerializationContext& Context)
{
    NodePayload NodeData{};
    NodeData.NodeId = NodeRef.Id();
    NodeData.NodeType = NodeRef.TypeKey();
    if (const TypeInfo* Type = TypeRegistry::Instance().Find(NodeData.NodeType))
    {
        NodeData.NodeTypeName = Type->Name;
    }
    NodeData.Name = NodeRef.Name();
    NodeData.Active = NodeRef.Active();

    if (NodeData.NodeType != TypeId{} && HasSerializableFields(NodeData.NodeType))
    {
        NodeData.NodeBytes.clear();
        VectorWriteStreambuf Buffer(NodeData.NodeBytes);
        std::ostream Os(&Buffer);
        cereal::BinaryOutputArchive Archive(Os);
        std::unordered_map<TypeId, bool, UuidHash> Visited{};
        auto NodeResult = SerializeFieldsRecursive(NodeData.NodeType, &NodeRef, Archive, Context, Visited);
        if (!NodeResult)
        {
            return std::unexpected(NodeResult.error());
        }
        NodeData.HasNodeData = true;
    }

    const NodeHandle NodeSelfHandle = NodeRef.Handle();
    NodeData.Components.reserve(NodeRef.ComponentTypes().size());
    for (const auto& Type : NodeRef.ComponentTypes())
    {
        const void* ComponentPtr = nullptr;
        if (Context.World && !NodeSelfHandle.IsNull())
        {
            ComponentPtr = Context.World->BorrowedComponent(NodeSelfHandle, Type);
            if (!ComponentPtr && !NodeSelfHandle.Id.is_nil())
            {
                ComponentPtr = Context.World->BorrowedComponent(NodeHandle{NodeSelfHandle.Id}, Type);
            }
        }
        if (!ComponentPtr && Context.Graph && !NodeSelfHandle.IsNull())
        {
            ComponentPtr = Context.Graph->BorrowedComponent(NodeSelfHandle, Type);
        }

        if (!ComponentPtr)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Component instance missing while serializing node payload"));
        }

        NodeComponentPayload ComponentPayload{};
        ComponentPayload.ComponentType = Type;
        if (const auto* Component = static_cast<const BaseComponent*>(ComponentPtr))
        {
            ComponentPayload.ComponentId = Component->Id();
        }
        auto SerializeResult = ComponentSerializationRegistry::Instance().Serialize(Type, ComponentPtr, ComponentPayload.Bytes, Context);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }
        NodeData.Components.push_back(std::move(ComponentPayload));
    }

    NodeData.Children.reserve(NodeRef.Children().size());
    for (const NodeHandle& ChildHandle : NodeRef.Children())
    {
        BaseNode* ChildNode = ResolveNodeForPayload(ChildHandle, Context.World);
        if (!ChildNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child node could not be resolved during serialization"));
        }

        if (ChildNode->EditorTransient())
        {
            continue;
        }

        auto ChildPayloadResult = SerializeNodePayloadRecursive(*ChildNode, Context);
        if (!ChildPayloadResult)
        {
            return std::unexpected(ChildPayloadResult.error());
        }
        NodeData.Children.push_back(std::move(ChildPayloadResult.value()));
    }

    return NodeData;
}

TExpected<NodeHandle> CreateNodePayloadRecursive(const NodePayload& Payload,
                                                 IWorld& WorldRef,
                                                 const NodeHandle& Parent,
                                                 std::vector<PendingNodeDeserialize>& OutPending,
                                                 const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap)
{
    auto TypeResult = ResolveNodeTypeFromPayload(Payload);
    if (!TypeResult)
    {
        return std::unexpected(TypeResult.error());
    }

    const TypeId NodeType = TypeResult.value();
    Uuid RuntimeNodeId = Payload.NodeId;
    if (NodeIdRemap && !RuntimeNodeId.is_nil())
    {
        if (const auto It = NodeIdRemap->find(RuntimeNodeId); It != NodeIdRemap->end())
        {
            RuntimeNodeId = It->second;
        }
    }

    TExpected<NodeHandle> CreateResult = RuntimeNodeId.is_nil()
        ? WorldRef.CreateNode(NodeType, Payload.Name)
        : WorldRef.CreateNodeWithId(NodeType, Payload.Name, RuntimeNodeId);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    NodeHandle CreatedNode = CreateResult.value();
    if (!Parent.IsNull())
    {
        auto AttachResult = WorldRef.AttachChild(Parent, CreatedNode);
        if (!AttachResult)
        {
            (void)WorldRef.DestroyNode(CreatedNode);
            return std::unexpected(AttachResult.error());
        }
    }

    if (BaseNode* Node = CreatedNode.Borrowed())
    {
        Node->Active(Payload.Active);
    }

    OutPending.push_back(PendingNodeDeserialize{
        .Payload = &Payload,
        .Handle = CreatedNode,
        .NodeType = NodeType,
    });

    for (const NodePayload& Child : Payload.Children)
    {
        auto ChildResult = CreateNodePayloadRecursive(Child, WorldRef, CreatedNode, OutPending, NodeIdRemap);
        if (!ChildResult)
        {
            return std::unexpected(ChildResult.error());
        }
    }

    return CreatedNode;
}

TExpected<void> DeserializeNodePayloadData(const PendingNodeDeserialize& PendingData,
                                           IWorld& WorldRef,
                                           const TSerializationContext& Context,
                                           const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap)
{
    if (!PendingData.Payload)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Invalid pending node payload"));
    }

    const NodePayload& NodeData = *PendingData.Payload;
    NodeHandle Owner = PendingData.Handle;

    if (NodeData.HasNodeData && !NodeData.NodeBytes.empty())
    {
        auto* Node = Owner.Borrowed();
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
        }
        MemoryReadStreambuf Buffer(NodeData.NodeBytes.data(), NodeData.NodeBytes.size());
        std::istream Is(&Buffer);
        cereal::BinaryInputArchive Archive(Is);
        std::unordered_map<TypeId, bool, UuidHash> Visited{};
        auto NodeResult = DeserializeFieldsRecursive(
            PendingData.NodeType == TypeId{} ? Node->TypeKey() : PendingData.NodeType,
            Node,
            Archive,
            Context,
            Visited);
        if (!NodeResult)
        {
            return std::unexpected(NodeResult.error());
        }
    }

    for (const auto& ComponentPayload : NodeData.Components)
    {
        Uuid RuntimeComponentId = ComponentPayload.ComponentId;
        if (ComponentIdRemap && !RuntimeComponentId.is_nil())
        {
            if (const auto It = ComponentIdRemap->find(RuntimeComponentId); It != ComponentIdRemap->end())
            {
                RuntimeComponentId = It->second;
            }
        }

        auto CreateResult = RuntimeComponentId.is_nil()
            ? ComponentSerializationRegistry::Instance().Create(WorldRef, Owner, ComponentPayload.ComponentType)
            : ComponentSerializationRegistry::Instance().CreateWithId(
                WorldRef,
                Owner,
                ComponentPayload.ComponentType,
                RuntimeComponentId);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }
        void* ComponentPtr = CreateResult.value();
        auto DeserializeResult = ComponentSerializationRegistry::Instance().Deserialize(
            ComponentPayload.ComponentType,
            ComponentPtr,
            ComponentPayload.Bytes.data(),
            ComponentPayload.Bytes.size(),
            Context);
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error());
        }
    }

    if (BaseNode* Node = Owner.Borrowed())
    {
        if (auto* Pawn = NodeCast<PawnBase>(Node))
        {
            Pawn->OnCreateImpl(WorldRef);
        }

        if (auto* Start = NodeCast<PlayerStart>(Node))
        {
            Start->OnCreateImpl(WorldRef);
        }
    }

    return Ok();
}

std::vector<NodeHandle> LevelRootNodes(const Level& LevelRef)
{
    std::vector<NodeHandle> Roots{};
    if (!LevelRef.Handle().IsNull())
    {
        Roots = LevelRef.Children();
        return Roots;
    }

    LevelRef.NodePool().ForEach([&Roots](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Parent().IsNull())
        {
            Roots.push_back(Handle);
        }
    });

    return Roots;
}

std::vector<NodeHandle> WorldRootNodes(const World& WorldRef)
{
    std::vector<NodeHandle> Roots{};
    WorldRef.NodePool().ForEach([&Roots](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.Parent().IsNull())
        {
            Roots.push_back(Handle);
        }
    });
    return Roots;
}

TExpected<void> DestroyChildrenAndFlush(Level& LevelRef)
{
    IWorld* WorldRef = LevelRef.World();
    if (!WorldRef)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
    }

    std::vector<NodeHandle> Children = LevelRef.Children();
    for (const NodeHandle& Child : Children)
    {
        auto DestroyResult = WorldRef->DestroyNode(Child);
        if (!DestroyResult)
        {
            return std::unexpected(DestroyResult.error());
        }
    }

    if (!Children.empty())
    {
        WorldRef->EndFrame();
    }

    return Ok();
}

TExpected<NodeHandle> DeserializeNodePayloadImpl(const NodePayload& Payload,
                                                 IWorld& WorldRef,
                                                 const NodeHandle& Parent,
                                                 const Level* GraphContext,
                                                 const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap,
                                                 const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap)
{
    std::vector<PendingNodeDeserialize> PendingNodes{};
    PendingNodes.reserve(CountPayloadNodes(Payload));
    auto RootCreateResult = CreateNodePayloadRecursive(Payload, WorldRef, Parent, PendingNodes, NodeIdRemap);
    if (!RootCreateResult)
    {
        return std::unexpected(RootCreateResult.error());
    }

    TSerializationContext Context{};
    Context.World = &WorldRef;
    Context.Graph = GraphContext;
    Context.NodeIdRemap = NodeIdRemap;
    Context.ComponentIdRemap = ComponentIdRemap;
    for (const PendingNodeDeserialize& PendingData : PendingNodes)
    {
        auto ApplyResult = DeserializeNodePayloadData(PendingData, WorldRef, Context, ComponentIdRemap);
        if (!ApplyResult)
        {
            return std::unexpected(ApplyResult.error());
        }
    }

    return RootCreateResult.value();
}
} // namespace

TExpected<NodePayload> NodeSerializer::Serialize(const BaseNode& NodeRef)
{
    TSerializationContext Context{};
    Context.World = NodeRef.World();
    if (const auto* OwningLevel = NodeCast<Level>(&NodeRef))
    {
        Context.Graph = OwningLevel;
    }
    return SerializeNodePayloadRecursive(NodeRef, Context);
}

TExpected<NodeHandle> NodeSerializer::Deserialize(const NodePayload& Payload,
                                                  IWorld& WorldRef,
                                                  const NodeHandle& Parent,
                                                  const TDeserializeOptions& Options)
{
    TObjectIdRemap IdRemap{};
    const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap = nullptr;
    const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap = nullptr;
    if (Options.RegenerateObjectIds)
    {
        BuildNodePayloadObjectIdRemapRecursive(Payload, IdRemap);
        NodeIdRemap = &IdRemap.NodeIds;
        ComponentIdRemap = &IdRemap.ComponentIds;
    }

    return DeserializeNodePayloadImpl(Payload, WorldRef, Parent, nullptr, NodeIdRemap, ComponentIdRemap);
}

TExpected<NodeHandle> NodeSerializer::Deserialize(const NodePayload& Payload,
                                                  Level& LevelRef,
                                                  const NodeHandle& Parent,
                                                  const TDeserializeOptions& Options)
{
    IWorld* WorldRef = LevelRef.World();
    if (!WorldRef)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Destination level is not bound to a world"));
    }

    NodeHandle AttachParent = Parent;
    if (AttachParent.IsNull() && !LevelRef.Handle().IsNull())
    {
        AttachParent = LevelRef.Handle();
    }

    TObjectIdRemap IdRemap{};
    const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap = nullptr;
    const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap = nullptr;
    if (Options.RegenerateObjectIds)
    {
        BuildNodePayloadObjectIdRemapRecursive(Payload, IdRemap);
        NodeIdRemap = &IdRemap.NodeIds;
        ComponentIdRemap = &IdRemap.ComponentIds;
    }

    return DeserializeNodePayloadImpl(Payload, *WorldRef, AttachParent, &LevelRef, NodeIdRemap, ComponentIdRemap);
}

TExpected<LevelPayload> LevelSerializer::Serialize(const Level& LevelRef)
{
    LevelPayload Payload;
    Payload.Name = LevelRef.Name();

    const std::vector<NodeHandle> Roots = LevelRootNodes(LevelRef);
    Payload.Nodes.reserve(Roots.size());
    for (const NodeHandle& Root : Roots)
    {
        BaseNode* Node = ResolveNodeForPayload(Root, LevelRef.World());
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Level root node could not be resolved"));
        }

        if (Node->EditorTransient())
        {
            continue;
        }

        auto NodePayloadResult = NodeSerializer::Serialize(*Node);
        if (!NodePayloadResult)
        {
            return std::unexpected(NodePayloadResult.error());
        }
        Payload.Nodes.push_back(std::move(NodePayloadResult.value()));
    }

    return Payload;
}

TExpected<void> LevelSerializer::Deserialize(const LevelPayload& Payload,
                                             Level& LevelRef,
                                             const TDeserializeOptions& Options)
{
    auto ClearResult = DestroyChildrenAndFlush(LevelRef);
    if (!ClearResult)
    {
        return std::unexpected(ClearResult.error());
    }

    IWorld* WorldRef = LevelRef.World();
    if (!WorldRef)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Destination level is not bound to a world"));
    }

    NodeHandle AttachParent{};
    if (!LevelRef.Handle().IsNull())
    {
        AttachParent = LevelRef.Handle();
    }

    TObjectIdRemap IdRemap{};
    const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap = nullptr;
    const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap = nullptr;
    if (Options.RegenerateObjectIds)
    {
        for (const NodePayload& NodeData : Payload.Nodes)
        {
            BuildNodePayloadObjectIdRemapRecursive(NodeData, IdRemap);
        }
        NodeIdRemap = &IdRemap.NodeIds;
        ComponentIdRemap = &IdRemap.ComponentIds;
    }

    LevelRef.Name(Payload.Name);
    for (const NodePayload& NodeData : Payload.Nodes)
    {
        auto NodeResult = DeserializeNodePayloadImpl(
            NodeData,
            *WorldRef,
            AttachParent,
            &LevelRef,
            NodeIdRemap,
            ComponentIdRemap);
        if (!NodeResult)
        {
            return std::unexpected(NodeResult.error());
        }
    }
    return Ok();
}

TExpected<WorldPayload> WorldSerializer::Serialize(const World& WorldRef)
{
    WorldPayload Payload;
    Payload.Name = WorldRef.Name();

    const std::vector<NodeHandle> Roots = WorldRootNodes(WorldRef);
    Payload.Nodes.reserve(Roots.size());
    for (const NodeHandle& Root : Roots)
    {
        BaseNode* Node = ResolveNodeForPayload(Root, &WorldRef);
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "World root node could not be resolved"));
        }

        if (Node->EditorTransient())
        {
            continue;
        }

        auto NodePayloadResult = NodeSerializer::Serialize(*Node);
        if (!NodePayloadResult)
        {
            return std::unexpected(NodePayloadResult.error());
        }
        Payload.Nodes.push_back(std::move(NodePayloadResult.value()));
    }

    return Payload;
}

TExpected<void> WorldSerializer::Deserialize(const WorldPayload& Payload,
                                             World& WorldRef,
                                             const TDeserializeOptions& Options)
{
    WorldRef.Clear();
    WorldRef.Name(Payload.Name);

    TObjectIdRemap IdRemap{};
    const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap = nullptr;
    const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap = nullptr;
    if (Options.RegenerateObjectIds)
    {
        for (const NodePayload& NodeData : Payload.Nodes)
        {
            BuildNodePayloadObjectIdRemapRecursive(NodeData, IdRemap);
        }
        NodeIdRemap = &IdRemap.NodeIds;
        ComponentIdRemap = &IdRemap.ComponentIds;
    }

    for (const NodePayload& NodeData : Payload.Nodes)
    {
        auto NodeResult = DeserializeNodePayloadImpl(
            NodeData,
            static_cast<IWorld&>(WorldRef),
            {},
            nullptr,
            NodeIdRemap,
            ComponentIdRemap);
        if (!NodeResult)
        {
            return std::unexpected(NodeResult.error());
        }
    }

    return Ok();
}

TExpected<void> SerializeNodePayload(const NodePayload& Payload, std::vector<uint8_t>& OutBytes)
{
    try
    {
        OutBytes.clear();
        VectorWriteStreambuf Buffer(OutBytes);
        std::ostream Os(&Buffer);
        cereal::BinaryOutputArchive Archive(Os);
        Archive(Payload);
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<NodePayload> DeserializeNodePayload(const uint8_t* Bytes, size_t Size)
{
    if (!Bytes || Size == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Empty payload"));
    }
    try
    {
        MemoryReadStreambuf Buffer(Bytes, Size);
        std::istream Is(&Buffer);
        cereal::BinaryInputArchive Archive(Is);
        NodePayload Payload;
        Archive(Payload);
        return Payload;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<void> SerializeLevelPayload(const LevelPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    try
    {
        OutBytes.clear();
        VectorWriteStreambuf Buffer(OutBytes);
        std::ostream Os(&Buffer);
        cereal::BinaryOutputArchive Archive(Os);
        Archive(Payload);
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<LevelPayload> DeserializeLevelPayload(const uint8_t* Bytes, size_t Size)
{
    if (!Bytes || Size == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Empty payload"));
    }
    try
    {
        MemoryReadStreambuf Buffer(Bytes, Size);
        std::istream Is(&Buffer);
        cereal::BinaryInputArchive Archive(Is);
        LevelPayload Payload;
        Archive(Payload);
        return Payload;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<void> SerializeWorldPayload(const WorldPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    try
    {
        OutBytes.clear();
        VectorWriteStreambuf Buffer(OutBytes);
        std::ostream Os(&Buffer);
        cereal::BinaryOutputArchive Archive(Os);
        Archive(Payload);
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<WorldPayload> DeserializeWorldPayload(const uint8_t* Bytes, size_t Size)
{
    if (!Bytes || Size == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Empty payload"));
    }
    try
    {
        MemoryReadStreambuf Buffer(Bytes, Size);
        std::istream Is(&Buffer);
        cereal::BinaryInputArchive Archive(Is);
        WorldPayload Payload;
        Archive(Payload);
        return Payload;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

void RegisterSerializationDefaults()
{
    auto& ValueRegistry = ValueCodecRegistry::Instance();
    ValueRegistry.Register<bool>();
    ValueRegistry.Register<int>();
    ValueRegistry.Register<unsigned int>();
    ValueRegistry.Register<std::uint64_t>();
    ValueRegistry.Register<float>();
    ValueRegistry.Register<double>();
    ValueRegistry.Register<std::string>();
    ValueRegistry.Register<std::vector<uint8_t>>();
    ValueRegistry.Register<Uuid>();
    ValueRegistry.Register<Vec3>();
    ValueRegistry.Register<Quat>();
    ValueRegistry.Register<NodeHandle>();
    ValueRegistry.Register<ComponentHandle>();

    auto& ComponentRegistry = ComponentSerializationRegistry::Instance();
    ComponentRegistry.RegisterCustom<RelevanceComponent>(
        [](const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext&) -> TExpected<void> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null relevance component"));
            }
            const auto* Component = static_cast<const RelevanceComponent*>(Instance);
            const bool Active = Component->Active();
            const float Score = Component->LastScore();
            Archive(Active, Score);
            return Ok();
        },
        [](void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext&) -> TExpected<void> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null relevance component"));
            }
            bool Active = true;
            float Score = 0.0f;
            Archive(Active, Score);
            auto* Component = static_cast<RelevanceComponent*>(Instance);
            Component->Active(Active);
            Component->LastScore(Score);
            return Ok();
        });
}

} // namespace SnAPI::GameFramework
