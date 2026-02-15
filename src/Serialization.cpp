#include "Serialization.h"

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
#include "NodeGraph.h"
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
        Value.ParentId,
        Value.HasNodeData,
        Value.NodeBytes,
        Value.Components,
        Value.HasGraph,
        Value.GraphBytes);
}

/**
 * @brief cereal serialize for NodeGraphPayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, NodeGraphPayload& Value)
{
    ArchiveRef(Value.Name, Value.Nodes);
}

/**
 * @brief cereal serialize for LevelPayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, LevelPayload& Value)
{
    ArchiveRef(Value.Name, Value.Graph);
}

/**
 * @brief cereal serialize for WorldPayload.
 * @param ArchiveRef Archive.
 * @param Value Payload to serialize.
 */
template <class Archive>
void serialize(Archive& ArchiveRef, WorldPayload& Value)
{
    ArchiveRef(Value.Graph);
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
std::mutex g_serializableFieldMutex; /**< @brief Guards serializable field cache map. */

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
        std::lock_guard<std::mutex> Lock(g_serializableFieldMutex);
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
        std::lock_guard<std::mutex> Lock(g_serializableFieldMutex);
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
            return std::unexpected(MakeError(EErrorCode::NotFound, "No serializer for field type"));
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
            return std::unexpected(MakeError(EErrorCode::NotFound, "No deserializer for field type"));
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

TExpected<void*> ComponentSerializationRegistry::Create(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type) const
{
    CreateFn CreateValue;
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            CreateValue = It->second.Create;
        }
    }
    if (!CreateValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        std::lock_guard<std::mutex> Lock(m_mutex);
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
    return CreateValue(Graph, Owner);
}

TExpected<void*> ComponentSerializationRegistry::CreateWithId(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type, const Uuid& Id) const
{
    CreateWithIdFn CreateValue;
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            CreateValue = It->second.CreateWithId;
        }
    }
    if (!CreateValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        std::lock_guard<std::mutex> Lock(m_mutex);
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
    return CreateValue(Graph, Owner, Id);
}

TExpected<void> ComponentSerializationRegistry::Serialize(const TypeId& Type, const void* Instance, std::vector<uint8_t>& OutBytes, const TSerializationContext& Context) const
{
    SerializeFn SerializeValue;
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            SerializeValue = It->second.Serialize;
        }
    }
    if (!SerializeValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        std::lock_guard<std::mutex> Lock(m_mutex);
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
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_entries.find(Type);
        if (It != m_entries.end())
        {
            DeserializeValue = It->second.Deserialize;
        }
    }
    if (!DeserializeValue)
    {
        (void)TypeAutoRegistry::Instance().Ensure(Type);
        std::lock_guard<std::mutex> Lock(m_mutex);
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

TExpected<NodeGraphPayload> NodeGraphSerializer::Serialize(const NodeGraph& Graph)
{
    NodeGraphPayload Payload;
    Payload.Name = Graph.Name();

    TSerializationContext Context;
    Context.Graph = &Graph;

    std::vector<NodeHandle> Handles;
    Graph.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode&) {
        Handles.push_back(Handle);
    });

    Payload.Nodes.reserve(Handles.size());
    for (const NodeHandle& Handle : Handles)
    {
        auto* Node = Handle.Borrowed();
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
        }

        NodePayload NodeData;
        NodeData.NodeId = Node->Id();
        NodeData.NodeType = Node->TypeKey();
        if (NodeData.NodeType == TypeId{})
        {
            if (const auto* Info = TypeRegistry::Instance().Find(NodeData.NodeType))
            {
                NodeData.NodeTypeName = Info->Name;
            }
        }
        NodeData.Name = Node->Name();
        NodeData.Active = Node->Active();
        if (!Node->Parent().IsNull())
        {
            NodeData.ParentId = Node->Parent().Id;
        }

        if (HasSerializableFields(NodeData.NodeType))
        {
            NodeData.NodeBytes.clear();
            VectorWriteStreambuf Buffer(NodeData.NodeBytes);
            std::ostream Os(&Buffer);
            cereal::BinaryOutputArchive Archive(Os);
            auto NodeResult = ComponentSerializationRegistry::SerializeByReflection(NodeData.NodeType, Node, Archive, Context);
            if (!NodeResult)
            {
                return std::unexpected(NodeResult.error());
            }
            NodeData.HasNodeData = true;
        }

        NodeData.Components.reserve(Node->ComponentTypes().size());
        for (const auto& Type : Node->ComponentTypes())
        {
            const void* ComponentPtr = Graph.BorrowedComponent(Handle, Type);
            if (!ComponentPtr)
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "Component instance missing"));
            }
            NodeComponentPayload ComponentPayload;
            const auto* Component = static_cast<const IComponent*>(ComponentPtr);
            ComponentPayload.ComponentId = Component ? Component->Id() : Uuid{};
            ComponentPayload.ComponentType = Type;
            auto SerializeResult = ComponentSerializationRegistry::Instance().Serialize(Type, ComponentPtr, ComponentPayload.Bytes, Context);
            if (!SerializeResult)
            {
                return std::unexpected(SerializeResult.error());
            }
            NodeData.Components.push_back(std::move(ComponentPayload));
        }

        if (const auto* GraphNode = dynamic_cast<const NodeGraph*>(Node))
        {
            auto GraphPayloadResult = NodeGraphSerializer::Serialize(*GraphNode);
            if (!GraphPayloadResult)
            {
                return std::unexpected(GraphPayloadResult.error());
            }
            NodeData.HasGraph = true;
            auto BytesResult = SerializeNodeGraphPayload(GraphPayloadResult.value(), NodeData.GraphBytes);
            if (!BytesResult)
            {
                return std::unexpected(BytesResult.error());
            }
        }

        Payload.Nodes.push_back(std::move(NodeData));
    }

    return Payload;
}

TExpected<void> NodeGraphSerializer::Deserialize(const NodeGraphPayload& Payload, NodeGraph& Graph)
{
    Graph.Clear();
    Graph.Name(Payload.Name);

    TSerializationContext Context;
    Context.Graph = &Graph;
    std::vector<NodeHandle> CreatedHandles;
    CreatedHandles.reserve(Payload.Nodes.size());
    for (const auto& NodeData : Payload.Nodes)
    {
        TypeId Type = NodeData.NodeType;
        if (Type == TypeId{})
        {
            Type = TypeIdFromName(NodeData.NodeTypeName);
        }
        TExpected<NodeHandle> HandleResult = NodeData.NodeId.is_nil()
            ? Graph.CreateNode(Type, NodeData.Name)
            : Graph.CreateNode(Type, NodeData.Name, NodeData.NodeId);
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }
        NodeHandle Handle = HandleResult.value();
        CreatedHandles.push_back(Handle);
        if (auto* Node = Handle.Borrowed())
        {
            Node->Active(NodeData.Active);
        }
    }

    for (size_t Index = 0; Index < Payload.Nodes.size(); ++Index)
    {
        const auto& NodeData = Payload.Nodes[Index];
        if (!NodeData.ParentId.is_nil())
        {
            NodeHandle Parent(NodeData.ParentId);
            NodeHandle Child = NodeData.NodeId.is_nil() ? CreatedHandles[Index] : NodeHandle(NodeData.NodeId);
            auto AttachResult = Graph.AttachChild(Parent, Child);
            if (!AttachResult)
            {
                return std::unexpected(AttachResult.error());
            }
        }
    }

    for (size_t Index = 0; Index < Payload.Nodes.size(); ++Index)
    {
        const auto& NodeData = Payload.Nodes[Index];
        NodeHandle Owner = NodeData.NodeId.is_nil() ? CreatedHandles[Index] : NodeHandle(NodeData.NodeId);

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
            auto NodeResult = ComponentSerializationRegistry::DeserializeByReflection(NodeData.NodeType, Node, Archive, Context);
            if (!NodeResult)
            {
                return std::unexpected(NodeResult.error());
            }
        }

        for (const auto& ComponentPayload : NodeData.Components)
        {
            auto CreateResult = ComponentPayload.ComponentId.is_nil()
                ? ComponentSerializationRegistry::Instance().Create(Graph, Owner, ComponentPayload.ComponentType)
                : ComponentSerializationRegistry::Instance().CreateWithId(Graph, Owner, ComponentPayload.ComponentType, ComponentPayload.ComponentId);
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

        if (NodeData.HasGraph)
        {
            auto* Node = Owner.Borrowed();
            if (!Node)
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "Graph node missing"));
            }
            auto* GraphNode = dynamic_cast<NodeGraph*>(Node);
            if (!GraphNode)
            {
                return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Node is not a graph"));
            }
            auto GraphPayloadResult = DeserializeNodeGraphPayload(NodeData.GraphBytes.data(), NodeData.GraphBytes.size());
            if (!GraphPayloadResult)
            {
                return std::unexpected(GraphPayloadResult.error());
            }
            auto GraphResult = NodeGraphSerializer::Deserialize(GraphPayloadResult.value(), *GraphNode);
            if (!GraphResult)
            {
                return std::unexpected(GraphResult.error());
            }
        }
    }

    return Ok();
}

TExpected<LevelPayload> LevelSerializer::Serialize(const Level& LevelRef)
{
    LevelPayload Payload;
    Payload.Name = LevelRef.Name();

    auto GraphResult = NodeGraphSerializer::Serialize(LevelRef);
    if (!GraphResult)
    {
        return std::unexpected(GraphResult.error());
    }
    Payload.Graph = std::move(GraphResult.value());
    return Payload;
}

TExpected<void> LevelSerializer::Deserialize(const LevelPayload& Payload, Level& LevelRef)
{
    LevelRef.Name(Payload.Name);
    return NodeGraphSerializer::Deserialize(Payload.Graph, LevelRef);
}

TExpected<WorldPayload> WorldSerializer::Serialize(const World& WorldRef)
{
    WorldPayload Payload;
    auto GraphResult = NodeGraphSerializer::Serialize(WorldRef);
    if (!GraphResult)
    {
        return std::unexpected(GraphResult.error());
    }
    Payload.Graph = std::move(GraphResult.value());
    return Payload;
}

TExpected<void> WorldSerializer::Deserialize(const WorldPayload& Payload, World& WorldRef)
{
    return NodeGraphSerializer::Deserialize(Payload.Graph, WorldRef);
}

TExpected<void> SerializeNodeGraphPayload(const NodeGraphPayload& Payload, std::vector<uint8_t>& OutBytes)
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

TExpected<NodeGraphPayload> DeserializeNodeGraphPayload(const uint8_t* Bytes, size_t Size)
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
        NodeGraphPayload Payload;
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
