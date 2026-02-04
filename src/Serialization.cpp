#include "Serialization.h"

#include <array>
#include <exception>
#include <sstream>
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
/**
 * @brief Check if a type (or its bases) has serializable fields.
 * @param Type TypeId to check.
 * @param Cache Memoization cache.
 * @return True if any fields are registered for serialization.
 */
bool HasSerializableFields(const TypeId& Type, std::unordered_map<TypeId, bool, UuidHash>& Cache)
{
    auto It = Cache.find(Type);
    if (It != Cache.end())
    {
        return It->second;
    }

    const auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        Cache.emplace(Type, false);
        return false;
    }

    if (!Info->Fields.empty())
    {
        Cache.emplace(Type, true);
        return true;
    }

    for (const auto& Base : Info->BaseTypes)
    {
        if (HasSerializableFields(Base, Cache))
        {
            Cache.emplace(Type, true);
            return true;
        }
    }

    Cache.emplace(Type, false);
    return false;
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
    auto [It, Inserted] = Visited.emplace(Type, true);
    if (!Inserted)
    {
        return Ok();
    }

    auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    for (const auto& Base : Info->BaseTypes)
    {
        auto BaseResult = SerializeFieldsRecursive(Base, Instance, Archive, Context, Visited);
        if (!BaseResult)
        {
            return BaseResult;
        }
    }

    auto& ValueRegistry = ValueCodecRegistry::Instance();
    for (const auto& Field : Info->Fields)
    {
        auto FieldResult = Field.Getter(const_cast<void*>(Instance));
        if (!FieldResult)
        {
            return std::unexpected(FieldResult.error());
        }
        const Variant& FieldValue = FieldResult.value();
        if (ValueRegistry.Has(Field.FieldType))
        {
            auto EncodeResult = ValueRegistry.Encode(Field.FieldType, FieldValue.Borrowed(), Archive, Context);
            if (!EncodeResult)
            {
                return EncodeResult;
            }
        }
        else
        {
            auto* NestedInfo = TypeRegistry::Instance().Find(Field.FieldType);
            if (!NestedInfo || (NestedInfo->Fields.empty() && NestedInfo->BaseTypes.empty()))
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "No serializer for field type"));
            }
            const void* FieldPtr = FieldValue.Borrowed();
            if (!FieldPtr)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null field pointer"));
            }
            auto NestedResult = SerializeFieldsRecursive(Field.FieldType, FieldPtr, Archive, Context, Visited);
            if (!NestedResult)
            {
                return NestedResult;
            }
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
    auto [It, Inserted] = Visited.emplace(Type, true);
    if (!Inserted)
    {
        return Ok();
    }

    auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    for (const auto& Base : Info->BaseTypes)
    {
        auto BaseResult = DeserializeFieldsRecursive(Base, Instance, Archive, Context, Visited);
        if (!BaseResult)
        {
            return BaseResult;
        }
    }

    auto& ValueRegistry = ValueCodecRegistry::Instance();
    for (const auto& Field : Info->Fields)
    {
        if (ValueRegistry.Has(Field.FieldType))
        {
            auto ValueResult = ValueRegistry.Decode(Field.FieldType, Archive, Context);
            if (!ValueResult)
            {
                return std::unexpected(ValueResult.error());
            }
            auto SetResult = Field.Setter(Instance, ValueResult.value());
            if (!SetResult)
            {
                return std::unexpected(SetResult.error());
            }
        }
        else
        {
            auto* NestedInfo = TypeRegistry::Instance().Find(Field.FieldType);
            if (!NestedInfo || (NestedInfo->Fields.empty() && NestedInfo->BaseTypes.empty()))
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "No deserializer for field type"));
            }
            auto FieldResult = Field.Getter(Instance);
            if (!FieldResult)
            {
                return std::unexpected(FieldResult.error());
            }
            if (FieldResult->IsConst())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot mutate const field"));
            }
            void* FieldPtr = const_cast<void*>(FieldResult->Borrowed());
            if (!FieldPtr)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null field pointer"));
            }
            auto NestedResult = DeserializeFieldsRecursive(Field.FieldType, FieldPtr, Archive, Context, Visited);
            if (!NestedResult)
            {
                return NestedResult;
            }
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

TExpected<void> ValueCodecRegistry::Encode(const TypeId& Type, const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end() || !It->second.Encode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No value codec registered"));
    }
    return It->second.Encode(Value, Archive, Context);
}

TExpected<Variant> ValueCodecRegistry::Decode(const TypeId& Type, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end() || !It->second.Decode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No value codec registered"));
    }
    return It->second.Decode(Archive, Context);
}

ComponentSerializationRegistry& ComponentSerializationRegistry::Instance()
{
    static ComponentSerializationRegistry Instance;
    return Instance;
}

TExpected<void*> ComponentSerializationRegistry::Create(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end() || !It->second.Create)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component factory registered"));
    }
    return It->second.Create(Graph, Owner);
}

TExpected<void*> ComponentSerializationRegistry::CreateWithId(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type, const Uuid& Id) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end() || !It->second.CreateWithId)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component factory registered"));
    }
    return It->second.CreateWithId(Graph, Owner, Id);
}

TExpected<void> ComponentSerializationRegistry::Serialize(const TypeId& Type, const void* Instance, std::vector<uint8_t>& OutBytes, const TSerializationContext& Context) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end() || !It->second.Serialize)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component serializer registered"));
    }
    try
    {
        std::ostringstream Os(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Os);
        auto Result = It->second.Serialize(Instance, Archive, Context);
        if (!Result)
        {
            return Result;
        }
        const auto Data = Os.str();
        OutBytes.assign(Data.begin(), Data.end());
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

TExpected<void> ComponentSerializationRegistry::Deserialize(const TypeId& Type, void* Instance, const uint8_t* Bytes, size_t Size, const TSerializationContext& Context) const
{
    auto It = m_entries.find(Type);
    if (It == m_entries.end() || !It->second.Deserialize)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No component serializer registered"));
    }
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null component bytes"));
    }
    try
    {
        std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Is(Data, std::ios::binary);
        cereal::BinaryInputArchive Archive(Is);
        return It->second.Deserialize(Instance, Archive, Context);
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
    std::unordered_map<TypeId, bool, UuidHash> FieldCache;

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
        if (const auto* Info = TypeRegistry::Instance().Find(NodeData.NodeType))
        {
            NodeData.NodeTypeName = Info->Name;
        }
        NodeData.Name = Node->Name();
        NodeData.Active = Node->Active();
        if (!Node->Parent().IsNull())
        {
            NodeData.ParentId = Node->Parent().Id;
        }

        if (HasSerializableFields(NodeData.NodeType, FieldCache))
        {
            std::ostringstream Os(std::ios::binary);
            cereal::BinaryOutputArchive Archive(Os);
            auto NodeResult = ComponentSerializationRegistry::SerializeByReflection(NodeData.NodeType, Node, Archive, Context);
            if (!NodeResult)
            {
                return std::unexpected(NodeResult.error());
            }
            const auto Data = Os.str();
            NodeData.NodeBytes.assign(Data.begin(), Data.end());
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
            std::string Data(reinterpret_cast<const char*>(NodeData.NodeBytes.data()), NodeData.NodeBytes.size());
            std::istringstream Is(Data, std::ios::binary);
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
        std::ostringstream Os(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Os);
        Archive(Payload);
        const auto Data = Os.str();
        OutBytes.assign(Data.begin(), Data.end());
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
        std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Is(Data, std::ios::binary);
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
        std::ostringstream Os(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Os);
        Archive(Payload);
        const auto Data = Os.str();
        OutBytes.assign(Data.begin(), Data.end());
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
        std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Is(Data, std::ios::binary);
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
        std::ostringstream Os(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Os);
        Archive(Payload);
        const auto Data = Os.str();
        OutBytes.assign(Data.begin(), Data.end());
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
        std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Is(Data, std::ios::binary);
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
    ValueRegistry.Register<Uuid>();
    ValueRegistry.Register<Vec3>();
    ValueRegistry.Register<NodeHandle>();
    ValueRegistry.Register<ComponentHandle>();

    auto& ComponentRegistry = ComponentSerializationRegistry::Instance();
    ComponentRegistry.Register<TransformComponent>();
    ComponentRegistry.Register<ScriptComponent>();

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
