#pragma once

#include <array>
#include "GameThreading.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <mutex>
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
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

class Level;
class World;

/**
 * @brief Context used during serialization/deserialization.
 * @remarks
 * Context is propagated into value codecs and reflection walkers so serializers can
 * resolve graph-relative handles and object references deterministically.
 */
struct TSerializationContext
{
    const NodeGraph* Graph = nullptr; /**< @brief Graph being serialized/deserialized. */
};

/**
 * @brief Customization point for value serialization.
 * @remarks
 * Specialize `TValueCodec<T>` for packed/custom wire/storage formats.
 *
 * Default behavior includes:
 * - common framework scalar/value types
 * - UUID/handle binary encoding
 * - trivially-copyable fallback for POD-like values
 *
 * For high-performance hot paths, prefer custom codecs to avoid generic reflection walk costs.
 */
template<typename T>
struct TValueCodec
{
    static TExpected<void> Encode(const T& Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            Archive(Value);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
        {
            Archive(Value);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, Uuid>)
        {
            const auto& Bytes = Value.as_bytes();
            std::array<uint8_t, 16> Data{};
            std::memcpy(Data.data(), Bytes.data(), Data.size());
            Archive(Data);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, Vec3>)
        {
            const auto X = Value.x();
            const auto Y = Value.y();
            const auto Z = Value.z();
            Archive(X, Y, Z);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, Quat>)
        {
            const auto X = Value.x();
            const auto Y = Value.y();
            const auto Z = Value.z();
            const auto W = Value.w();
            Archive(X, Y, Z, W);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, NodeHandle>)
        {
            const auto& Bytes = Value.Id.as_bytes();
            std::array<uint8_t, 16> Data{};
            std::memcpy(Data.data(), Bytes.data(), Data.size());
            Archive(Data);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, ComponentHandle>)
        {
            const auto& Bytes = Value.Id.as_bytes();
            std::array<uint8_t, 16> Data{};
            std::memcpy(Data.data(), Bytes.data(), Data.size());
            Archive(Data);
            return Ok();
        }
        else if constexpr (std::is_trivially_copyable_v<T>)
        {
            Archive(cereal::binary_data(const_cast<T*>(&Value), sizeof(T)));
            return Ok();
        }
        else
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type not serializable"));
        }
    }

    static TExpected<T> Decode(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            std::string Value;
            Archive(Value);
            return Value;
        }
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
        {
            std::vector<uint8_t> Value;
            Archive(Value);
            return Value;
        }
        else if constexpr (std::is_same_v<T, Uuid>)
        {
            std::array<uint8_t, 16> Data{};
            Archive(Data);
            std::array<uint8_t, 16> Bytes{};
            std::memcpy(Bytes.data(), Data.data(), Bytes.size());
            return Uuid(Bytes);
        }
        else if constexpr (std::is_same_v<T, Vec3>)
        {
            using Scalar = typename Vec3::Scalar;
            Scalar X = Scalar(0);
            Scalar Y = Scalar(0);
            Scalar Z = Scalar(0);
            Archive(X, Y, Z);
            return Vec3(X, Y, Z);
        }
        else if constexpr (std::is_same_v<T, Quat>)
        {
            using Scalar = typename Quat::Scalar;
            Scalar X = Scalar(0);
            Scalar Y = Scalar(0);
            Scalar Z = Scalar(0);
            Scalar W = Scalar(1);
            Archive(X, Y, Z, W);
            Quat Rotation = Quat::Identity();
            Rotation.x() = X;
            Rotation.y() = Y;
            Rotation.z() = Z;
            Rotation.w() = W;
            return Rotation;
        }
        else if constexpr (std::is_same_v<T, NodeHandle>)
        {
            std::array<uint8_t, 16> Data{};
            Archive(Data);
            std::array<uint8_t, 16> Bytes{};
            std::memcpy(Bytes.data(), Data.data(), Bytes.size());
            const Uuid Id(Bytes);
            if (Context.Graph)
            {
                auto HandleResult = Context.Graph->NodeHandleByIdSlow(Id);
                if (HandleResult)
                {
                    return HandleResult.value();
                }
            }
            if (auto* Node = ObjectRegistry::Instance().Resolve<BaseNode>(Id))
            {
                return Node->Handle();
            }
            return NodeHandle(Id);
        }
        else if constexpr (std::is_same_v<T, ComponentHandle>)
        {
            std::array<uint8_t, 16> Data{};
            Archive(Data);
            std::array<uint8_t, 16> Bytes{};
            std::memcpy(Bytes.data(), Data.data(), Bytes.size());
            const Uuid Id(Bytes);
            if (auto* Component = ObjectRegistry::Instance().Resolve<IComponent>(Id))
            {
                return Component->Handle();
            }
            return ComponentHandle(Id);
        }
        else if constexpr (std::is_trivially_copyable_v<T>)
        {
            T Value{};
            Archive(cereal::binary_data(&Value, sizeof(T)));
            return Value;
        }
        else
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type not deserializable"));
        }
    }

    static TExpected<void> DecodeInto(T& Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            Archive(Value);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
        {
            Archive(Value);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, Uuid>)
        {
            std::array<uint8_t, 16> Data{};
            Archive(Data);
            std::array<uint8_t, 16> Bytes{};
            std::memcpy(Bytes.data(), Data.data(), Bytes.size());
            Value = Uuid(Bytes);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, Vec3>)
        {
            using Scalar = typename Vec3::Scalar;
            Scalar X = Value.x();
            Scalar Y = Value.y();
            Scalar Z = Value.z();
            Archive(X, Y, Z);
            Value = Vec3(X, Y, Z);
            return Ok();
        }
        else if constexpr (std::is_same_v<T, Quat>)
        {
            using Scalar = typename Quat::Scalar;
            Scalar X = Value.x();
            Scalar Y = Value.y();
            Scalar Z = Value.z();
            Scalar W = Value.w();
            Archive(X, Y, Z, W);
            Value.x() = X;
            Value.y() = Y;
            Value.z() = Z;
            Value.w() = W;
            return Ok();
        }
        else if constexpr (std::is_same_v<T, NodeHandle>)
        {
            std::array<uint8_t, 16> Data{};
            Archive(Data);
            std::array<uint8_t, 16> Bytes{};
            std::memcpy(Bytes.data(), Data.data(), Bytes.size());
            const Uuid Id(Bytes);
            if (Context.Graph)
            {
                auto HandleResult = Context.Graph->NodeHandleByIdSlow(Id);
                if (HandleResult)
                {
                    Value = HandleResult.value();
                    return Ok();
                }
            }
            if (auto* Node = ObjectRegistry::Instance().Resolve<BaseNode>(Id))
            {
                Value = Node->Handle();
            }
            else
            {
                Value = NodeHandle(Id);
            }
            return Ok();
        }
        else if constexpr (std::is_same_v<T, ComponentHandle>)
        {
            std::array<uint8_t, 16> Data{};
            Archive(Data);
            std::array<uint8_t, 16> Bytes{};
            std::memcpy(Bytes.data(), Data.data(), Bytes.size());
            const Uuid Id(Bytes);
            if (auto* Component = ObjectRegistry::Instance().Resolve<IComponent>(Id))
            {
                Value = Component->Handle();
            }
            else
            {
                Value = ComponentHandle(Id);
            }
            return Ok();
        }
        else if constexpr (std::is_trivially_copyable_v<T>)
        {
            Archive(cereal::binary_data(&Value, sizeof(T)));
            return Ok();
        }
        else
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type not deserializable"));
        }
    }
};

/**
 * @brief Registry for value codecs used by reflection serialization.
 * @remarks
 * Dynamic registry that maps reflected `TypeId` to encode/decode callbacks.
 * Consumers include:
 * - payload serializers
 * - replication field encode/decode
 * - reflected RPC argument marshaling (indirectly through Variant/value conversions)
 */
class ValueCodecRegistry
{
public:
    /**
     * @brief Encoder function signature.
     * @param Value Pointer to value.
     * @param Archive Output archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    using EncodeFn = TExpected<void>(*)(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);
    /**
     * @brief Decoder function signature.
     * @param Archive Input archive.
     * @param Context Serialization context.
     * @return Variant containing decoded value or error.
     */
    using DecodeFn = TExpected<Variant>(*)(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);
    /**
     * @brief Decode-into function signature.
     * @param Value Output pointer to write into.
     * @param Archive Input archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    using DecodeIntoFn = TExpected<void>(*)(void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    /**
     * @brief Entry storing encode/decode callbacks.
     */
    struct CodecEntry
    {
        EncodeFn Encode = nullptr; /**< @brief Encode callback. */
        DecodeFn Decode = nullptr; /**< @brief Decode callback. */
        DecodeIntoFn DecodeInto = nullptr; /**< @brief Decode-into callback. */
    };

    /**
     * @brief Access the singleton registry.
     * @return Registry instance.
     */
    static ValueCodecRegistry& Instance();

    /**
     * @brief Register a codec for type T.
     * @tparam T Type to register.
     * @remarks Binds static `TValueCodec<T>` implementations into runtime type-id keyed map.
     */
    template<typename T>
    void Register()
    {
        const TypeId Type = StaticTypeId<T>();
        m_entries[Type] = {&EncodeImpl<T>, &DecodeImpl<T>, &DecodeIntoImpl<T>};
        ++m_version;
    }

    /**
     * @brief Check if a codec exists for a type.
     * @param Type TypeId to query.
     * @return True if registered.
     */
    bool Has(const TypeId& Type) const
    {
        return FindEntry(Type) != nullptr;
    }

    /**
     * @brief Lookup the codec entry for a type.
     * @param Type TypeId to query.
     * @return Pointer to codec entry or nullptr if not found.
     */
    const CodecEntry* FindEntry(const TypeId& Type) const;

    /**
     * @brief Get the codec registry version.
     * @return Version counter incremented on registration.
     * @remarks Used by caches to invalidate field-codec binding snapshots.
     */
    uint32_t Version() const
    {
        return m_version;
    }

    /**
     * @brief Encode a value by type id.
     * @param Type TypeId of the value.
     * @param Value Pointer to the value.
     * @param Archive Output archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    TExpected<void> Encode(const TypeId& Type, const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) const;
    /**
     * @brief Decode a value by type id.
     * @param Type TypeId of the value.
     * @param Archive Input archive.
     * @param Context Serialization context.
     * @return Variant containing decoded value or error.
     */
    TExpected<Variant> Decode(const TypeId& Type, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) const;
    /**
     * @brief Decode a value by type id directly into memory.
     * @param Type TypeId of the value.
     * @param Value Output pointer.
     * @param Archive Input archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    TExpected<void> DecodeInto(const TypeId& Type, void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) const;

private:
    /**
     * @brief Template encoder implementation.
     * @tparam T Value type.
     */
    template<typename T>
    static TExpected<void> EncodeImpl(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);

    /**
     * @brief Template decoder implementation.
     * @tparam T Value type.
     */
    template<typename T>
    static TExpected<Variant> DecodeImpl(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);
    /**
     * @brief Template decode-into implementation.
     * @tparam T Value type.
     */
    template<typename T>
    static TExpected<void> DecodeIntoImpl(void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    std::unordered_map<TypeId, CodecEntry, UuidHash> m_entries{}; /**< @brief Runtime codec dispatch table keyed by reflected TypeId. */
    uint32_t m_version = 0; /**< @brief Monotonic cache-invalidation version incremented on each registration. */
};

class ComponentSerializationRegistry
{
public:
    /**
     * @brief Callback to create a component on a graph.
     * @param Graph Owning graph.
     * @param Owner Owner node handle.
     * @return Pointer to created component (type-erased).
     */
    using CreateFn = std::function<TExpected<void*>(NodeGraph& Graph, NodeHandle Owner)>;
    /**
     * @brief Callback to create a component with explicit UUID.
     * @param Graph Owning graph.
     * @param Owner Owner node handle.
     * @param Id Component UUID.
     * @return Pointer to created component (type-erased).
     */
    using CreateWithIdFn = std::function<TExpected<void*>(NodeGraph& Graph, NodeHandle Owner, const Uuid& Id)>;
    /**
     * @brief Callback to serialize a component instance.
     * @param Instance Pointer to component instance.
     * @param Archive Output archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    using SerializeFn = std::function<TExpected<void>(const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)>;
    /**
     * @brief Callback to deserialize a component instance.
     * @param Instance Pointer to component instance.
     * @param Archive Input archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    using DeserializeFn = std::function<TExpected<void>(void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)>;

    /**
     * @brief Access the singleton registry.
     * @return Registry instance.
     */
    static ComponentSerializationRegistry& Instance();

    /**
     * @brief Register a component type using reflection serialization.
     * @tparam T Component type.
     * @remarks
     * Installs create/create-with-id callbacks and reflection-driven (de)serialization callbacks.
     */
    template<typename T>
    void Register()
    {
        const TypeId Type = StaticTypeId<T>();
        {
            GameLockGuard Lock(m_mutex);
            if (m_entries.find(Type) != m_entries.end())
            {
                return;
            }
        }
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
        GameLockGuard Lock(m_mutex);
        m_entries[Type] = std::move(EntryValue);
    }

    /**
     * @brief Register a component type with custom serialization.
     * @tparam T Component type.
     * @param Serialize Serialize callback.
     * @param Deserialize Deserialize callback.
     * @remarks
     * Useful when component wants binary-optimized layout or versioned custom format while
     * keeping default component construction semantics.
     */
    template<typename T>
    void RegisterCustom(SerializeFn Serialize, DeserializeFn Deserialize)
    {
        const TypeId Type = StaticTypeId<T>();
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
        GameLockGuard Lock(m_mutex);
        m_entries[Type] = std::move(EntryValue);
    }

    /**
     * @brief Check if a component type is registered.
     * @param Type Component TypeId.
     * @return True if registered.
     */
    bool Has(const TypeId& Type) const
    {
        GameLockGuard Lock(m_mutex);
        return m_entries.find(Type) != m_entries.end();
    }

    /**
     * @brief Enumerate registered component types.
     * @return Snapshot of registered component type ids.
     */
    std::vector<TypeId> Types() const
    {
        GameLockGuard Lock(m_mutex);
        std::vector<TypeId> Result{};
        Result.reserve(m_entries.size());
        for (const auto& [Type, _] : m_entries)
        {
            Result.push_back(Type);
        }
        return Result;
    }

    /**
     * @brief Create a component by type id.
     * @param Graph Owning graph.
     * @param Owner Owner node handle.
     * @param Type Component TypeId.
     * @return Pointer to created component or error.
     */
    TExpected<void*> Create(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type) const;
    /**
     * @brief Create a component by type id with explicit UUID.
     * @param Graph Owning graph.
     * @param Owner Owner node handle.
     * @param Type Component TypeId.
     * @param Id Component UUID.
     * @return Pointer to created component or error.
     */
    TExpected<void*> CreateWithId(NodeGraph& Graph, NodeHandle Owner, const TypeId& Type, const Uuid& Id) const;
    /**
     * @brief Serialize a component instance to bytes.
     * @param Type Component TypeId.
     * @param Instance Pointer to instance.
     * @param OutBytes Output byte vector.
     * @param Context Serialization context.
     * @return Success or error.
     * @remarks Produces raw component payload bytes suitable for graph payload embedding.
     */
    TExpected<void> Serialize(const TypeId& Type, const void* Instance, std::vector<uint8_t>& OutBytes, const TSerializationContext& Context) const;
    /**
     * @brief Deserialize a component instance from bytes.
     * @param Type Component TypeId.
     * @param Instance Pointer to instance.
     * @param Bytes Serialized bytes.
     * @param Size Byte count.
     * @param Context Serialization context.
     * @return Success or error.
     * @remarks Caller is responsible for having instantiated the destination component first.
     */
    TExpected<void> Deserialize(const TypeId& Type, void* Instance, const uint8_t* Bytes, size_t Size, const TSerializationContext& Context) const;

private:
    friend class NodeGraphSerializer;

    /**
     * @brief Registry entry storing creation and serialization callbacks.
     */
    struct Entry
    {
        CreateFn Create{}; /**< @brief Creation callback. */
        CreateWithIdFn CreateWithId{}; /**< @brief Creation-with-id callback. */
        SerializeFn Serialize{}; /**< @brief Serialization callback. */
        DeserializeFn Deserialize{}; /**< @brief Deserialization callback. */
    };

    /**
     * @brief Reflection-based serialization for a component instance.
     * @param Type Component TypeId.
     * @param Instance Pointer to component.
     * @param Archive Output archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    static TExpected<void> SerializeByReflection(const TypeId& Type, const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);
    /**
     * @brief Reflection-based deserialization for a component instance.
     * @param Type Component TypeId.
     * @param Instance Pointer to component.
     * @param Archive Input archive.
     * @param Context Serialization context.
     * @return Success or error.
     */
    static TExpected<void> DeserializeByReflection(const TypeId& Type, void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    mutable GameMutex m_mutex{}; /**< @brief Guards component serializer registry map updates/lookups. */
    std::unordered_map<TypeId, Entry, UuidHash> m_entries{}; /**< @brief Registry map by TypeId. */
};

/**
 * @brief Serialized component data attached to a node.
 * @remarks Atomic component payload unit embedded inside `NodePayload`.
 */
struct NodeComponentPayload
{
    Uuid ComponentId{}; /**< @brief Component UUID. */
    TypeId ComponentType{}; /**< @brief Component type id. */
    std::vector<uint8_t> Bytes{}; /**< @brief Serialized component bytes. */
};

/**
 * @brief Serialized node data within a graph.
 * @remarks
 * Represents full node snapshot:
 * - identity/type/name/state
 * - optional node reflected field bytes
 * - attached component payloads
 * - optional nested graph payload
 */
struct NodePayload
{
    Uuid NodeId{}; /**< @brief Node UUID. */
    TypeId NodeType{}; /**< @brief Node type id. */
    std::string NodeTypeName{}; /**< @brief Type name fallback when TypeId is missing. */
    std::string Name{}; /**< @brief Node name. */
    bool Active = true; /**< @brief Active state. */
    Uuid ParentId{}; /**< @brief Parent node UUID (nil if root). */
    bool HasNodeData = false; /**< @brief True if node fields were serialized. */
    std::vector<uint8_t> NodeBytes{}; /**< @brief Serialized node field bytes. */
    std::vector<NodeComponentPayload> Components{}; /**< @brief Component payloads. */
    bool HasGraph = false; /**< @brief True if node contains a nested graph. */
    std::vector<uint8_t> GraphBytes{}; /**< @brief Serialized nested graph bytes. */
};

/**
 * @brief Serialized node graph payload.
 * @remarks Graph-level serialized representation consumed by NodeGraphSerializer.
 */
struct NodeGraphPayload
{
    std::string Name{}; /**< @brief Graph name. */
    std::vector<NodePayload> Nodes{}; /**< @brief Node payloads. */
};

/**
 * @brief Serialized level payload.
 * @remarks Level envelope around graph payload for explicit level identity.
 */
struct LevelPayload
{
    std::string Name{}; /**< @brief Level name. */
    NodeGraphPayload Graph{}; /**< @brief Level root graph payload. */
};

/**
 * @brief Serialized world payload.
 * @remarks World envelope around root graph payload.
 */
struct WorldPayload
{
    NodeGraphPayload Graph{}; /**< @brief World root graph payload. */
};

/**
 * @brief Serializer for NodeGraph to/from NodeGraphPayload.
 * @remarks Reflection-driven serializer preserving UUID identity and hierarchy structure.
 */
class NodeGraphSerializer
{
public:
    /** @brief Current schema version for NodeGraph payloads. */
    static constexpr uint32_t kSchemaVersion = 4;

    /**
     * @brief Serialize a graph to a payload.
     * @param Graph Source graph.
     * @return Payload or error.
     */
    static TExpected<NodeGraphPayload> Serialize(const NodeGraph& Graph);
    /**
     * @brief Deserialize a graph from a payload.
     * @param Payload Payload to read.
     * @param Graph Destination graph.
     * @return Success or error.
     */
    static TExpected<void> Deserialize(const NodeGraphPayload& Payload, NodeGraph& Graph);
};

/**
 * @brief Serializer for Level to/from LevelPayload.
 * @remarks Delegates graph serialization to NodeGraphSerializer with level envelope semantics.
 */
class LevelSerializer
{
public:
    /** @brief Current schema version for Level payloads. */
    static constexpr uint32_t kSchemaVersion = 4;

    /**
     * @brief Serialize a level to a payload.
     * @param LevelRef Source level.
     * @return Payload or error.
     */
    static TExpected<LevelPayload> Serialize(const Level& LevelRef);
    /**
     * @brief Deserialize a level from a payload.
     * @param Payload Payload to read.
     * @param LevelRef Destination level.
     * @return Success or error.
     */
    static TExpected<void> Deserialize(const LevelPayload& Payload, Level& LevelRef);
};

/**
 * @brief Serializer for World to/from WorldPayload.
 * @remarks Delegates graph serialization to NodeGraphSerializer with world envelope semantics.
 */
class WorldSerializer
{
public:
    /** @brief Current schema version for World payloads. */
    static constexpr uint32_t kSchemaVersion = 4;

    /**
     * @brief Serialize a world to a payload.
     * @param WorldRef Source world.
     * @return Payload or error.
     */
    static TExpected<WorldPayload> Serialize(const World& WorldRef);
    /**
     * @brief Deserialize a world from a payload.
     * @param Payload Payload to read.
     * @param WorldRef Destination world.
     * @return Success or error.
     */
    static TExpected<void> Deserialize(const WorldPayload& Payload, World& WorldRef);
};

/**
 * @brief Serialize a NodeGraphPayload to bytes.
 * @param Payload Payload to serialize.
 * @param OutBytes Output byte vector.
 * @return Success or error.
 */
TExpected<void> SerializeNodeGraphPayload(const NodeGraphPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @brief Deserialize a NodeGraphPayload from bytes.
 * @param Bytes Byte buffer.
 * @param Size Byte count.
 * @return Payload or error.
 */
TExpected<NodeGraphPayload> DeserializeNodeGraphPayload(const uint8_t* Bytes, size_t Size);
/**
 * @brief Serialize a LevelPayload to bytes.
 * @param Payload Payload to serialize.
 * @param OutBytes Output byte vector.
 * @return Success or error.
 */
TExpected<void> SerializeLevelPayload(const LevelPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @brief Deserialize a LevelPayload from bytes.
 * @param Bytes Byte buffer.
 * @param Size Byte count.
 * @return Payload or error.
 */
TExpected<LevelPayload> DeserializeLevelPayload(const uint8_t* Bytes, size_t Size);
/**
 * @brief Serialize a WorldPayload to bytes.
 * @param Payload Payload to serialize.
 * @param OutBytes Output byte vector.
 * @return Success or error.
 */
TExpected<void> SerializeWorldPayload(const WorldPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @brief Deserialize a WorldPayload from bytes.
 * @param Bytes Byte buffer.
 * @param Size Byte count.
 * @return Payload or error.
 */
TExpected<WorldPayload> DeserializeWorldPayload(const uint8_t* Bytes, size_t Size);

/**
 * @brief Register default serialization codecs and component serializers.
 * @remarks Call after RegisterBuiltinTypes.
 */
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
    return TValueCodec<T>::Encode(*static_cast<const T*>(Value), Archive, Context);
}

template<typename T>
TExpected<Variant> ValueCodecRegistry::DecodeImpl(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)
{
    auto Result = TValueCodec<T>::Decode(Archive, Context);
    if (!Result)
    {
        return std::unexpected(Result.error());
    }
    return Variant::FromValue(std::move(Result.value()));
}

template<typename T>
TExpected<void> ValueCodecRegistry::DecodeIntoImpl(void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)
{
    if (!Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null value"));
    }
    return TValueCodec<T>::DecodeInto(*static_cast<T*>(Value), Archive, Context);
}

} // namespace SnAPI::GameFramework
