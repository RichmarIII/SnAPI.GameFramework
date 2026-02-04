#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

#include "Expected.h"
#include "Handle.h"
#include "Math.h"
#include "NodeGraph.h"
#include "TypeName.h"
#include "Uuid.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

class Level;
class World;

struct TSerializationContext
{
    const NodeGraph* Graph = nullptr;
};

class ValueCodecRegistry
{
public:
    using EncodeFn = TExpected<void>(*)(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);
    using DecodeFn = TExpected<Variant>(*)(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    static ValueCodecRegistry& Instance();

    template<typename T>
    void Register()
    {
        const TypeId Type = TypeIdFromName(TTypeNameV<T>);
        m_entries[Type] = {&EncodeImpl<T>, &DecodeImpl<T>};
    }

    bool Has(const TypeId& Type) const
    {
        return m_entries.find(Type) != m_entries.end();
    }

    TExpected<void> Encode(const TypeId& Type, const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) const;
    TExpected<Variant> Decode(const TypeId& Type, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) const;

private:
    template<typename T>
    static TExpected<void> EncodeImpl(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);

    template<typename T>
    static TExpected<Variant> DecodeImpl(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    struct Entry
    {
        EncodeFn Encode = nullptr;
        DecodeFn Decode = nullptr;
    };

    std::unordered_map<TypeId, Entry, UuidHash> m_entries{};
};

class ComponentSerializationRegistry
{
public:
    using CreateFn = std::function<TExpected<void*>(NodeGraph& Graph, NodeHandle Owner)>;
    using CreateWithIdFn = std::function<TExpected<void*>(NodeGraph& Graph, NodeHandle Owner, const Uuid& Id)>;
    using SerializeFn = std::function<TExpected<void>(const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)>;
    using DeserializeFn = std::function<TExpected<void>(void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)>;

    static ComponentSerializationRegistry& Instance();

    template<typename T>
    void Register()
    {
        const TypeId Type = TypeIdFromName(TTypeNameV<T>);
        Entry EntryValue;
        EntryValue.Create = [](NodeGraph& Graph, NodeHandle Owner) -> TExpected<void*> {
            auto Result = Graph.template AddComponent<T>(Owner);
            if (!Result)
            {
                return std::unexpected(Result.error());
            }
            return static_cast<void*>(&*Result);
        };
        EntryValue.CreateWithId = [](NodeGraph& Graph, NodeHandle Owner, const Uuid& Id) -> TExpected<void*> {
            auto Result = Graph.template AddComponentWithId<T>(Owner, Id);
            if (!Result)
            {
                return std::unexpected(Result.error());
            }
            return static_cast<void*>(&*Result);
        };
        EntryValue.Serialize = [Type](const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) -> TExpected<void> {
            return SerializeByReflection(Type, Instance, Archive, Context);
        };
        EntryValue.Deserialize = [Type](void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) -> TExpected<void> {
            return DeserializeByReflection(Type, Instance, Archive, Context);
        };
        m_entries[Type] = std::move(EntryValue);
    }

    template<typename T>
    void RegisterCustom(SerializeFn Serialize, DeserializeFn Deserialize)
    {
        const TypeId Type = TypeIdFromName(TTypeNameV<T>);
        Entry EntryValue;
        EntryValue.Create = [](NodeGraph& Graph, NodeHandle Owner) -> TExpected<void*> {
            auto Result = Graph.template AddComponent<T>(Owner);
            if (!Result)
            {
                return std::unexpected(Result.error());
            }
            return static_cast<void*>(&*Result);
        };
        EntryValue.CreateWithId = [](NodeGraph& Graph, NodeHandle Owner, const Uuid& Id) -> TExpected<void*> {
            auto Result = Graph.template AddComponentWithId<T>(Owner, Id);
            if (!Result)
            {
                return std::unexpected(Result.error());
            }
            return static_cast<void*>(&*Result);
        };
        EntryValue.Serialize = std::move(Serialize);
        EntryValue.Deserialize = std::move(Deserialize);
        m_entries[Type] = std::move(EntryValue);
    }

    bool Has(const TypeId& Type) const
    {
        return m_entries.find(Type) != m_entries.end();
    }

    TExpected<void*> Create(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type) const;
    TExpected<void*> CreateWithId(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type, const Uuid& Id) const;
    TExpected<void> Serialize(const TypeId& Type, const void* Instance, std::vector<uint8_t>& OutBytes, const TSerializationContext& Context) const;
    TExpected<void> Deserialize(const TypeId& Type, void* Instance, const uint8_t* Bytes, size_t Size, const TSerializationContext& Context) const;

private:
    friend class NodeGraphSerializer;

    struct Entry
    {
        CreateFn Create{};
        CreateWithIdFn CreateWithId{};
        SerializeFn Serialize{};
        DeserializeFn Deserialize{};
    };

    static TExpected<void> SerializeByReflection(const TypeId& Type, const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);
    static TExpected<void> DeserializeByReflection(const TypeId& Type, void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    std::unordered_map<TypeId, Entry, UuidHash> m_entries{};
};

struct NodeComponentPayload
{
    Uuid ComponentId{};
    TypeId ComponentType{};
    std::vector<uint8_t> Bytes{};
};

struct NodePayload
{
    Uuid NodeId{};
    TypeId NodeType{};
    std::string NodeTypeName{};
    std::string Name{};
    bool Active = true;
    Uuid ParentId{};
    bool HasNodeData = false;
    std::vector<uint8_t> NodeBytes{};
    std::vector<NodeComponentPayload> Components{};
    bool HasGraph = false;
    std::vector<uint8_t> GraphBytes{};
};

struct NodeGraphPayload
{
    std::string Name{};
    std::vector<NodePayload> Nodes{};
};

struct LevelPayload
{
    std::string Name{};
    NodeGraphPayload Graph{};
};

struct WorldPayload
{
    NodeGraphPayload Graph{};
};

class NodeGraphSerializer
{
public:
    static constexpr uint32_t kSchemaVersion = 4;

    static TExpected<NodeGraphPayload> Serialize(const NodeGraph& Graph);
    static TExpected<void> Deserialize(const NodeGraphPayload& Payload, NodeGraph& Graph);
};

class LevelSerializer
{
public:
    static constexpr uint32_t kSchemaVersion = 4;

    static TExpected<LevelPayload> Serialize(const Level& LevelRef);
    static TExpected<void> Deserialize(const LevelPayload& Payload, Level& LevelRef);
};

class WorldSerializer
{
public:
    static constexpr uint32_t kSchemaVersion = 4;

    static TExpected<WorldPayload> Serialize(const World& WorldRef);
    static TExpected<void> Deserialize(const WorldPayload& Payload, World& WorldRef);
};

TExpected<void> SerializeNodeGraphPayload(const NodeGraphPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<NodeGraphPayload> DeserializeNodeGraphPayload(const uint8_t* Bytes, size_t Size);
TExpected<void> SerializeLevelPayload(const LevelPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<LevelPayload> DeserializeLevelPayload(const uint8_t* Bytes, size_t Size);
TExpected<void> SerializeWorldPayload(const WorldPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<WorldPayload> DeserializeWorldPayload(const uint8_t* Bytes, size_t Size);

void RegisterSerializationDefaults();

} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework
{

template<typename T>
TExpected<void> ValueCodecRegistry::EncodeImpl(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)
{
    if (!Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null value"));
    }
    if constexpr (std::is_same_v<T, std::string>)
    {
        Archive(*static_cast<const std::string*>(Value));
        return Ok();
    }
    else if constexpr (std::is_same_v<T, Uuid>)
    {
        const auto& Bytes = static_cast<const Uuid*>(Value)->as_bytes();
        std::array<uint8_t, 16> Data{};
        for (size_t i = 0; i < Data.size(); ++i)
        {
            Data[i] = static_cast<uint8_t>(std::to_integer<uint8_t>(Bytes[i]));
        }
        Archive(Data);
        return Ok();
    }
    else if constexpr (std::is_same_v<T, Vec3>)
    {
        const auto& Vec = *static_cast<const Vec3*>(Value);
        Archive(Vec.X, Vec.Y, Vec.Z);
        return Ok();
    }
    else if constexpr (std::is_same_v<T, NodeHandle>)
    {
        const auto& Handle = *static_cast<const NodeHandle*>(Value);
        const auto& Bytes = Handle.Id.as_bytes();
        std::array<uint8_t, 16> Data{};
        for (size_t i = 0; i < Data.size(); ++i)
        {
            Data[i] = static_cast<uint8_t>(std::to_integer<uint8_t>(Bytes[i]));
        }
        Archive(Data);
        return Ok();
    }
    else if constexpr (std::is_same_v<T, ComponentHandle>)
    {
        const auto& Handle = *static_cast<const ComponentHandle*>(Value);
        const auto& Bytes = Handle.Id.as_bytes();
        std::array<uint8_t, 16> Data{};
        for (size_t i = 0; i < Data.size(); ++i)
        {
            Data[i] = static_cast<uint8_t>(std::to_integer<uint8_t>(Bytes[i]));
        }
        Archive(Data);
        return Ok();
    }
    else if constexpr (std::is_trivially_copyable_v<T>)
    {
        Archive(cereal::binary_data(const_cast<T*>(static_cast<const T*>(Value)), sizeof(T)));
        return Ok();
    }
    else
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type not serializable"));
    }
}

template<typename T>
TExpected<Variant> ValueCodecRegistry::DecodeImpl(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::string Value;
        Archive(Value);
        return Variant::FromValue(std::move(Value));
    }
    else if constexpr (std::is_same_v<T, Uuid>)
    {
        std::array<uint8_t, 16> Data{};
        Archive(Data);
        std::array<uint8_t, 16> Bytes{};
        for (size_t i = 0; i < Bytes.size(); ++i)
        {
            Bytes[i] = Data[i];
        }
        return Variant::FromValue(Uuid(Bytes));
    }
    else if constexpr (std::is_same_v<T, Vec3>)
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        Archive(X, Y, Z);
        return Variant::FromValue(Vec3{X, Y, Z});
    }
    else if constexpr (std::is_same_v<T, NodeHandle>)
    {
        std::array<uint8_t, 16> Data{};
        Archive(Data);
        std::array<uint8_t, 16> Bytes{};
        for (size_t i = 0; i < Bytes.size(); ++i)
        {
            Bytes[i] = Data[i];
        }
        return Variant::FromValue(NodeHandle(Uuid(Bytes)));
    }
    else if constexpr (std::is_same_v<T, ComponentHandle>)
    {
        std::array<uint8_t, 16> Data{};
        Archive(Data);
        std::array<uint8_t, 16> Bytes{};
        for (size_t i = 0; i < Bytes.size(); ++i)
        {
            Bytes[i] = Data[i];
        }
        return Variant::FromValue(ComponentHandle(Uuid(Bytes)));
    }
    else if constexpr (std::is_trivially_copyable_v<T>)
    {
        T Value{};
        Archive(cereal::binary_data(&Value, sizeof(T)));
        return Variant::FromValue(Value);
    }
    else
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type not deserializable"));
    }
}

} // namespace SnAPI::GameFramework
