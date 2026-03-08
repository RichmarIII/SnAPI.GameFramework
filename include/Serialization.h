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
#include "Level.h"
#include "AssetRef.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

class Level;
class World;
class IWorld;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared context propagated through value codecs and reflection serializers.
 *
 * `TSerializationContext` carries the ambient lookup state needed to turn raw UUID-backed
 * payload data into live framework objects. It exists so low-level codecs can resolve
 * `NodeHandle`, `ComponentHandle`, and other graph-relative values without hard-coding one
 * global resolution path.
 *
 * Core semantics:
 * - `World` is the primary runtime lookup surface for live handles.
 * - `Graph` provides an optional level-scoped fallback when a serializer is operating
 *   against a `Level`.
 * - `NodeIdRemap` and `ComponentIdRemap` rewrite serialized source/template ids to fresh
 *   runtime ids when deserialization is configured to regenerate object identity.
 * - `UseLegacyFloatVectorDecode` enables compatibility decoding for older payloads that
 *   stored `Vec3` and `Quat` scalars as `float` even when the runtime scalar type is
 *   wider.
 *
 * Ownership and lifetime:
 * - All pointers are borrowed.
 * - The pointed-to objects and maps must outlive the encode/decode call using the
 *   context.
 *
 * @see TDeserializeOptions, TValueCodec, NodeSerializer
 */
struct TSerializationContext
{
    const IWorld* World = nullptr; /**< @brief Borrowed World used as the primary runtime lookup surface for Node and Component handles during decode. */
    const Level* Graph = nullptr; /**< @brief Optional borrowed Level used as a secondary graph-local lookup surface when a World lookup is unavailable or insufficient. */
    const std::unordered_map<Uuid, Uuid, UuidHash>* NodeIdRemap = nullptr; /**< @brief Optional borrowed source-node-id to runtime-node-id remap applied before handle resolution. */
    const std::unordered_map<Uuid, Uuid, UuidHash>* ComponentIdRemap = nullptr; /**< @brief Optional borrowed source-component-id to runtime-component-id remap applied before handle resolution. */
    bool UseLegacyFloatVectorDecode = false; /**< @brief Compatibility flag enabling legacy float32 decode for vector and quaternion payloads when the runtime scalar type is wider. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Policy flags that control how payload identity is materialized during decode.
 *
 * These options alter how serializers treat UUIDs embedded in payloads. The main use case
 * is the difference between:
 * - loading a save file and preserving object identity, and
 * - instantiating a template/prefab-like payload where every object must receive a fresh
 *   runtime id.
 */
struct TDeserializeOptions
{
    /**
     * @brief Regenerate Node and Component UUIDs while loading.
     *
     * When enabled, the payload's object ids are treated as source/template ids rather
     * than runtime-stable ids. Deserialization builds remap tables up front, assigns fresh
     * ids to newly created objects, and rewrites internal `NodeHandle` and
     * `ComponentHandle` references through those remaps while decoding fields.
     *
     * @note References that point outside the payload are not magically re-bound; only
     * ids present in the generated remap tables are rewritten.
     */
    bool RegenerateObjectIds = false;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time customization point for serializing one value type.
 *
 * `TValueCodec<T>` defines how a concrete C++ value is encoded into a binary archive and
 * decoded back out. `ValueCodecRegistry` binds these compile-time functions to runtime
 * `TypeId`s so reflection-based systems can serialize arbitrary reflected fields through a
 * common dispatch path.
 *
 * Default behavior covers:
 * - `std::string`
 * - `std::vector<uint8_t>`
 * - `Uuid`
 * - `Vec3`
 * - `Quat`
 * - `NodeHandle`
 * - `ComponentHandle`
 * - trivially copyable types as raw binary blobs
 *
 * Handle semantics during decode:
 * - `NodeHandle` first applies `NodeIdRemap`, then attempts runtime resolution through
 *   `World`, then `Graph`, then the global `ObjectRegistry`, and finally falls back to a
 *   UUID-only handle when no live object can be resolved.
 * - `ComponentHandle` first applies `ComponentIdRemap`, then attempts `ObjectRegistry`
 *   resolution, and finally falls back to a UUID-only handle.
 *
 * Specialize this template when a type needs:
 * - versioned or packed wire storage
 * - custom pointer or asset resolution rules
 * - a format that is more stable than raw memory layout
 *
 * Threading:
 * - The codec itself is stateless, but it may consult objects referenced by
 *   `TSerializationContext`. Any required synchronization is the caller's responsibility.
 *
 * @tparam T Value type to encode and decode.
 * @see ValueCodecRegistry
 */
template<typename T>
struct TValueCodec
{
    /**
     * @brief Encode one value into a cereal binary archive.
     * @param Value Value to serialize.
     * @param Archive Destination archive.
     * @param Context Borrowed serialization context used for handle-aware codecs.
     * @return `Ok()` on success or an error when the type has no supported default codec.
     */
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

    /**
     * @brief Decode one value from a cereal binary archive.
     * @param Archive Source archive positioned at the value payload.
     * @param Context Borrowed serialization context used for handle remap and lookup.
     * @return Decoded value on success or an error when the type has no supported default
     *         codec.
     */
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
            if constexpr (sizeof(Scalar) > sizeof(float))
            {
                if (Context.UseLegacyFloatVectorDecode)
                {
                    float LegacyX = 0.0f;
                    float LegacyY = 0.0f;
                    float LegacyZ = 0.0f;
                    Archive(LegacyX, LegacyY, LegacyZ);
                    return Vec3(
                        static_cast<Scalar>(LegacyX),
                        static_cast<Scalar>(LegacyY),
                        static_cast<Scalar>(LegacyZ));
                }
            }
            Scalar X = Scalar(0);
            Scalar Y = Scalar(0);
            Scalar Z = Scalar(0);
            Archive(X, Y, Z);
            return Vec3(X, Y, Z);
        }
        else if constexpr (std::is_same_v<T, Quat>)
        {
            using Scalar = typename Quat::Scalar;
            if constexpr (sizeof(Scalar) > sizeof(float))
            {
                if (Context.UseLegacyFloatVectorDecode)
                {
                    float LegacyX = 0.0f;
                    float LegacyY = 0.0f;
                    float LegacyZ = 0.0f;
                    float LegacyW = 1.0f;
                    Archive(LegacyX, LegacyY, LegacyZ, LegacyW);
                    Quat Rotation = Quat::Identity();
                    Rotation.x() = static_cast<Scalar>(LegacyX);
                    Rotation.y() = static_cast<Scalar>(LegacyY);
                    Rotation.z() = static_cast<Scalar>(LegacyZ);
                    Rotation.w() = static_cast<Scalar>(LegacyW);
                    return Rotation;
                }
            }
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
            Uuid Id(Bytes);
            if (Context.NodeIdRemap)
            {
                if (const auto It = Context.NodeIdRemap->find(Id); It != Context.NodeIdRemap->end())
                {
                    Id = It->second;
                }
            }
            if (Context.World)
            {
                auto HandleResult = Context.World->NodeHandleById(Id);
                if (HandleResult)
                {
                    return HandleResult.value();
                }
            }
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
            Uuid Id(Bytes);
            if (Context.ComponentIdRemap)
            {
                if (const auto It = Context.ComponentIdRemap->find(Id); It != Context.ComponentIdRemap->end())
                {
                    Id = It->second;
                }
            }
            if (auto* Component = ObjectRegistry::Instance().Resolve<BaseComponent>(Id))
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

    /**
     * @brief Decode one value directly into existing storage.
     * @param Value Destination object to overwrite.
     * @param Archive Source archive positioned at the value payload.
     * @param Context Borrowed serialization context used for handle remap and lookup.
     * @return `Ok()` on success or an error when the type has no supported default codec.
     */
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
            if constexpr (sizeof(Scalar) > sizeof(float))
            {
                if (Context.UseLegacyFloatVectorDecode)
                {
                    float LegacyX = static_cast<float>(Value.x());
                    float LegacyY = static_cast<float>(Value.y());
                    float LegacyZ = static_cast<float>(Value.z());
                    Archive(LegacyX, LegacyY, LegacyZ);
                    Value = Vec3(
                        static_cast<Scalar>(LegacyX),
                        static_cast<Scalar>(LegacyY),
                        static_cast<Scalar>(LegacyZ));
                    return Ok();
                }
            }
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
            if constexpr (sizeof(Scalar) > sizeof(float))
            {
                if (Context.UseLegacyFloatVectorDecode)
                {
                    float LegacyX = static_cast<float>(Value.x());
                    float LegacyY = static_cast<float>(Value.y());
                    float LegacyZ = static_cast<float>(Value.z());
                    float LegacyW = static_cast<float>(Value.w());
                    Archive(LegacyX, LegacyY, LegacyZ, LegacyW);
                    Value.x() = static_cast<Scalar>(LegacyX);
                    Value.y() = static_cast<Scalar>(LegacyY);
                    Value.z() = static_cast<Scalar>(LegacyZ);
                    Value.w() = static_cast<Scalar>(LegacyW);
                    return Ok();
                }
            }
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
            Uuid Id(Bytes);
            if (Context.NodeIdRemap)
            {
                if (const auto It = Context.NodeIdRemap->find(Id); It != Context.NodeIdRemap->end())
                {
                    Id = It->second;
                }
            }
            if (Context.World)
            {
                auto HandleResult = Context.World->NodeHandleById(Id);
                if (HandleResult)
                {
                    Value = HandleResult.value();
                    return Ok();
                }
            }
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
            Uuid Id(Bytes);
            if (Context.ComponentIdRemap)
            {
                if (const auto It = Context.ComponentIdRemap->find(Id); It != Context.ComponentIdRemap->end())
                {
                    Id = It->second;
                }
            }
            if (auto* Component = ObjectRegistry::Instance().Resolve<BaseComponent>(Id))
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
 * @ingroup SnAPI_GameFramework
 * @brief `TValueCodec` specialization for asset references.
 *
 * Asset references serialize by logical asset identity rather than by any loaded runtime
 * object state. The wire format stores both asset name and asset id so loaders can choose
 * whichever identifier is most useful in the current environment.
 *
 * @tparam TBase Asset base type.
 * @tparam TNameTag Asset name-tag type used by `TAssetRef`.
 */
template<typename TBase, typename TNameTag>
struct TValueCodec<TAssetRef<TBase, TNameTag>>
{
    /** @brief Serialize asset reference identity fields into the archive. */
    static TExpected<void> Encode(const TAssetRef<TBase, TNameTag>& Value,
                                  cereal::BinaryOutputArchive& Archive,
                                  const TSerializationContext&)
    {
        const std::string AssetName = Value.GetAssetName();
        const std::string AssetId = Value.GetAssetId();
        Archive(AssetName, AssetId);
        return Ok();
    }

    /** @brief Decode an asset reference from serialized asset name and asset id fields. */
    static TExpected<TAssetRef<TBase, TNameTag>> Decode(cereal::BinaryInputArchive& Archive,
                                                        const TSerializationContext&)
    {
        std::string AssetName{};
        std::string AssetId{};
        Archive(AssetName, AssetId);
        return TAssetRef<TBase, TNameTag>(std::move(AssetName), std::move(AssetId));
    }

    /** @brief Decode an asset reference directly into an existing `TAssetRef` instance. */
    static TExpected<void> DecodeInto(TAssetRef<TBase, TNameTag>& Value,
                                      cereal::BinaryInputArchive& Archive,
                                      const TSerializationContext&)
    {
        std::string AssetName{};
        std::string AssetId{};
        Archive(AssetName, AssetId);
        Value = TAssetRef<TBase, TNameTag>(std::move(AssetName), std::move(AssetId));
        return Ok();
    }
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief `TValueCodec` specialization for vectors of codec-supported element types.
 *
 * The vector wire format stores a 64-bit element count followed by each element encoded
 * through `TValueCodec<T>`.
 *
 * @tparam T Element type. Must itself be serializable through `TValueCodec<T>`.
 */
template<typename T>
struct TValueCodec<std::vector<T>>
{
    /** @brief Encode a vector length followed by each element in order. */
    static TExpected<void> Encode(const std::vector<T>& Value,
                                  cereal::BinaryOutputArchive& Archive,
                                  const TSerializationContext& Context)
    {
        const std::uint64_t Count = static_cast<std::uint64_t>(Value.size());
        Archive(Count);
        for (const T& Element : Value)
        {
            auto Result = TValueCodec<T>::Encode(Element, Archive, Context);
            if (!Result)
            {
                return Result;
            }
        }
        return Ok();
    }

    /** @brief Decode a vector by reading its stored element count and each serialized element. */
    static TExpected<std::vector<T>> Decode(cereal::BinaryInputArchive& Archive,
                                            const TSerializationContext& Context)
    {
        std::uint64_t Count = 0;
        Archive(Count);

        std::vector<T> Value{};
        Value.reserve(static_cast<std::size_t>(Count));
        for (std::uint64_t Index = 0; Index < Count; ++Index)
        {
            auto ElementResult = TValueCodec<T>::Decode(Archive, Context);
            if (!ElementResult)
            {
                return std::unexpected(ElementResult.error());
            }
            Value.emplace_back(std::move(*ElementResult));
        }
        return Value;
    }

    /** @brief Replace an existing vector with decoded contents from the archive. */
    static TExpected<void> DecodeInto(std::vector<T>& Value,
                                      cereal::BinaryInputArchive& Archive,
                                      const TSerializationContext& Context)
    {
        std::uint64_t Count = 0;
        Archive(Count);

        Value.clear();
        Value.reserve(static_cast<std::size_t>(Count));
        for (std::uint64_t Index = 0; Index < Count; ++Index)
        {
            auto ElementResult = TValueCodec<T>::Decode(Archive, Context);
            if (!ElementResult)
            {
                return std::unexpected(ElementResult.error());
            }
            Value.emplace_back(std::move(*ElementResult));
        }
        return Ok();
    }
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime registry that binds reflected `TypeId`s to concrete value codecs.
 *
 * Reflection-based systems such as payload serialization, replication, and reflected RPC
 * need to encode values when they only know the runtime `TypeId`. `ValueCodecRegistry`
 * bridges that gap by turning `TValueCodec<T>` specializations into runtime dispatch
 * entries.
 *
 * Core semantics:
 * - Registration stores function pointers for encode, decode, and decode-into operations.
 * - `Version()` increments on every registration and can be used by higher-level caches
 *   to invalidate any memoized codec lookup state.
 * - Missing codecs fail at runtime with `EErrorCode::NotFound`.
 *
 * Ownership and lifetime:
 * - The registry is a process-wide singleton.
 * - Registered callbacks are static function pointers derived from `TValueCodec<T>` and
 *   therefore do not capture user state.
 *
 * Threading:
 * - Not internally synchronized.
 * - Registration and lookup must not race. In practice, register codecs during startup
 *   before multiple threads begin using the registry.
 *
 * @see TValueCodec, RegisterSerializationDefaults()
 */
class ValueCodecRegistry
{
public:
    /** @brief Runtime function signature used to encode one type-erased value. */
    using EncodeFn = TExpected<void>(*)(const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context);
    /** @brief Runtime function signature used to decode one value into a `Variant`. */
    using DecodeFn = TExpected<Variant>(*)(cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);
    /** @brief Runtime function signature used to decode one value directly into caller-provided storage. */
    using DecodeIntoFn = TExpected<void>(*)(void* Value, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context);

    /**
     * @brief Runtime dispatch entry for one reflected type.
     */
    struct CodecEntry
    {
        EncodeFn Encode = nullptr; /**< @brief Encode callback for the registered type. */
        DecodeFn Decode = nullptr; /**< @brief Decode-to-Variant callback for the registered type. */
        DecodeIntoFn DecodeInto = nullptr; /**< @brief Decode-into-existing-storage callback for the registered type. */
    };

    /**
     * @brief Access the process-wide value codec registry.
     * @return Singleton registry instance.
     */
    static ValueCodecRegistry& Instance();

    /**
     * @brief Register the default `TValueCodec<T>` under `StaticTypeId<T>()`.
     * @tparam T Value type to expose through runtime dispatch.
     *
     * Re-registering the same type replaces the existing callbacks and increments the
     * registry version.
     */
    template<typename T>
    void Register()
    {
        const TypeId Type = StaticTypeId<T>();
        m_entries[Type] = {&EncodeImpl<T>, &DecodeImpl<T>, &DecodeIntoImpl<T>};
        ++m_version;
    }

    /**
     * @brief Register the default `TValueCodec<T>` under an explicit reflected type id.
     * @tparam T Value type whose codec should be used.
     * @param Type Reflected type id to bind. `TypeId{}` is ignored.
     *
     * This is primarily used when the reflected field type is not expressed directly as
     * `StaticTypeId<T>()`, such as generated or aliased reflected container types.
     */
    template<typename T>
    void RegisterAs(const TypeId& Type)
    {
        if (Type == TypeId{})
        {
            return;
        }
        m_entries[Type] = {&EncodeImpl<T>, &DecodeImpl<T>, &DecodeIntoImpl<T>};
        ++m_version;
    }

    /**
     * @brief Check whether a runtime codec exists for one reflected type.
     * @param Type Reflected type id to query.
     * @return `true` when the registry currently has a dispatch entry for @p Type.
     */
    bool Has(const TypeId& Type) const
    {
        return FindEntry(Type) != nullptr;
    }

    /**
     * @brief Look up the raw runtime dispatch entry for one reflected type.
     * @param Type Reflected type id to query.
     * @return Pointer to the registry entry, or `nullptr` when the type is not registered.
     *
     * @note The returned pointer is borrowed and becomes invalid if the registry storage
     * rehashes due to later registrations.
     */
    const CodecEntry* FindEntry(const TypeId& Type) const;

    /**
     * @brief Return the registry mutation version.
     * @return Monotonic counter incremented on each successful registration call.
     *
     * Higher-level caches use this value to detect stale `TypeId` to codec-entry bindings.
     */
    uint32_t Version() const
    {
        return m_version;
    }

    /**
     * @brief Encode a type-erased value using its reflected type id.
     * @param Type Reflected type id of the value.
     * @param Value Pointer to the value storage. Must match @p Type.
     * @param Archive Destination archive.
     * @param Context Borrowed serialization context.
     * @return `Ok()` on success or an error when no codec is registered or the codec
     *         rejects the value.
     */
    TExpected<void> Encode(const TypeId& Type, const void* Value, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) const;
    /**
     * @brief Decode a value by reflected type id into a `Variant`.
     * @param Type Reflected type id of the value to decode.
     * @param Archive Source archive.
     * @param Context Borrowed serialization context.
     * @return `Variant` containing the decoded value on success, or an error when no codec
     *         is registered or decode fails.
     */
    TExpected<Variant> Decode(const TypeId& Type, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) const;
    /**
     * @brief Decode a value by reflected type id directly into existing storage.
     * @param Type Reflected type id of the value to decode.
     * @param Value Destination storage. Must point to an object compatible with @p Type.
     * @param Archive Source archive.
     * @param Context Borrowed serialization context.
     * @return `Ok()` on success or an error when no codec is registered or decode fails.
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

    std::unordered_map<TypeId, CodecEntry, UuidHash> m_entries{}; /**< @brief Runtime dispatch table keyed by reflected `TypeId`. */
    uint32_t m_version = 0; /**< @brief Monotonic mutation version used by higher-level caches. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Registry that knows how to construct and serialize reflected Component types.
 *
 * `ComponentSerializationRegistry` complements `ValueCodecRegistry`. Instead of handling
 * plain values, it handles Component instances that must be created inside a World, filled
 * from bytes, and then optionally receive deferred `OnCreate` lifecycle callbacks.
 *
 * Core semantics:
 * - Registration can install either reflection-based or custom byte serialization.
 * - Creation is routed by reflected `TypeId`.
 * - On lookup misses, the registry asks `TypeAutoRegistry` to ensure the reflected type is
 *   auto-registered before failing.
 * - `Deserialize()` only populates fields; `InvokeOnCreate()` is a separate explicit step.
 *
 * Threading:
 * - Registry map access is protected by `m_mutex`.
 * - The callbacks themselves usually mutate World or Component state and therefore are not
 *   generally safe to invoke from arbitrary threads. Treat create/deserialize/on-create
 *   operations as main-thread/world-thread work.
 *
 * Ownership:
 * - The registry is a process-wide singleton.
 * - It does not own created Components; ownership remains with the World/Node that
 *   created them.
 *
 * @see NodeSerializer, RegisterSerializationDefaults()
 */
class ComponentSerializationRegistry
{
public:
    /** @brief Callback signature used to create one Component instance inside a World. */
    using CreateFn = std::function<TExpected<void*>(IWorld& WorldRef, const NodeHandle& Owner)>;
    /** @brief Callback signature used to create one Component with an explicit UUID. */
    using CreateWithIdFn = std::function<TExpected<void*>(IWorld& WorldRef, const NodeHandle& Owner, const Uuid& Id)>;
    /** @brief Callback signature used to serialize one type-erased Component instance. */
    using SerializeFn = std::function<TExpected<void>(const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context)>;
    /** @brief Callback signature used to deserialize bytes into one existing type-erased Component instance. */
    using DeserializeFn = std::function<TExpected<void>(void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context)>;
    /** @brief Callback signature used to deliver deferred post-deserialize component lifecycle. */
    using OnCreateFn = std::function<TExpected<void>(void* Instance)>;

    /**
     * @brief Access the process-wide Component serialization registry.
     * @return Singleton registry instance.
     */
    static ComponentSerializationRegistry& Instance();

    /**
     * @brief Register one Component type using default reflection-based serialization.
     * @tparam T Component type to register.
     *
     * The default registration installs:
     * - runtime-Component creation callbacks
     * - creation-with-explicit-id callbacks
     * - reflection-based serialize and deserialize callbacks
     * - a deferred `OnCreate` adapter that calls `T::OnCreate()` or `T::OnCreate(IWorld&)`
     *   when available
     *
     * The current implementation only supports ECS/runtime components that are move
     * constructible and can be added through `AddRuntimeComponent*`.
     *
     * @note If the type is already registered, this function is a no-op.
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
        EntryValue.Create = [](IWorld& WorldRef, const NodeHandle& Owner) -> TExpected<void*> {
            (void)WorldRef;
            if constexpr (RuntimeTickType<T> && std::is_move_constructible_v<T>)
            {
                BaseNode* OwnerNode = Owner.Borrowed();
                if (OwnerNode)
                {
                    auto AddResult = OwnerNode->AddRuntimeComponent<T>();
                    if (AddResult)
                    {
                        auto ComponentResult = OwnerNode->RuntimeComponent<T>();
                        if (!ComponentResult)
                        {
                            return std::unexpected(ComponentResult.error());
                        }
                        return static_cast<void*>(&*ComponentResult);
                    }
                }
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "ECS-only components must be runtime-compatible and move constructible"));
        };
        EntryValue.CreateWithId = [](IWorld& WorldRef, const NodeHandle& Owner, const Uuid& Id) -> TExpected<void*> {
            (void)WorldRef;
            if constexpr (RuntimeTickType<T> && std::is_move_constructible_v<T>)
            {
                BaseNode* OwnerNode = Owner.Borrowed();
                if (OwnerNode)
                {
                    auto AddResult = OwnerNode->AddRuntimeComponentWithId<T>(Id);
                    if (AddResult)
                    {
                        auto ComponentResult = OwnerNode->RuntimeComponent<T>();
                        if (!ComponentResult)
                        {
                            return std::unexpected(ComponentResult.error());
                        }
                        return static_cast<void*>(&*ComponentResult);
                    }
                }
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "ECS-only components must be runtime-compatible and move constructible"));
        };
        EntryValue.Serialize = [Type](const void* Instance, cereal::BinaryOutputArchive& Archive, const TSerializationContext& Context) -> TExpected<void> {
            return SerializeByReflection(Type, Instance, Archive, Context);
        };
        EntryValue.Deserialize = [Type](void* Instance, cereal::BinaryInputArchive& Archive, const TSerializationContext& Context) -> TExpected<void> {
            return DeserializeByReflection(Type, Instance, Archive, Context);
        };
        EntryValue.OnCreate = [](void* Instance) -> TExpected<void> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null component instance"));
            }
            auto& TypedInstance = *static_cast<T*>(Instance);
            if constexpr (requires(T& Value) { Value.OnCreate(); })
            {
                TypedInstance.OnCreate();
            }
            else if constexpr (requires(T& Value, IWorld & WorldRef) { Value.OnCreate(WorldRef); })
            {
                IWorld* WorldPtr = TypedInstance.World();
                if (!WorldPtr)
                {
                    return std::unexpected(MakeError(EErrorCode::NotReady, "Component world is unavailable during OnCreate"));
                }
                TypedInstance.OnCreate(*WorldPtr);
            }
            else
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "Component type does not expose OnCreate lifecycle"));
            }
            return Ok();
        };
        GameLockGuard Lock(m_mutex);
        m_entries[Type] = std::move(EntryValue);
    }

    /**
     * @brief Register one Component type with custom byte serialization callbacks.
     * @tparam T Component type to register.
     * @param Serialize Custom serializer for the Component's payload bytes.
     * @param Deserialize Custom deserializer for the Component's payload bytes.
     *
     * Creation and deferred `OnCreate` behavior still use the registry's default runtime
     * component construction logic; only the field byte format is customized.
     */
    template<typename T>
    void RegisterCustom(SerializeFn Serialize, DeserializeFn Deserialize)
    {
        const TypeId Type = StaticTypeId<T>();
        Entry EntryValue;
        EntryValue.Create = [](IWorld& WorldRef, const NodeHandle& Owner) -> TExpected<void*> {
            (void)WorldRef;
            if constexpr (RuntimeTickType<T> && std::is_move_constructible_v<T>)
            {
                BaseNode* OwnerNode = Owner.Borrowed();
                if (OwnerNode)
                {
                    auto AddResult = OwnerNode->AddRuntimeComponent<T>();
                    if (AddResult)
                    {
                        auto ComponentResult = OwnerNode->RuntimeComponent<T>();
                        if (!ComponentResult)
                        {
                            return std::unexpected(ComponentResult.error());
                        }
                        return static_cast<void*>(&*ComponentResult);
                    }
                }
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "ECS-only components must be runtime-compatible and move constructible"));
        };
        EntryValue.CreateWithId = [](IWorld& WorldRef, const NodeHandle& Owner, const Uuid& Id) -> TExpected<void*> {
            (void)WorldRef;
            if constexpr (RuntimeTickType<T> && std::is_move_constructible_v<T>)
            {
                BaseNode* OwnerNode = Owner.Borrowed();
                if (OwnerNode)
                {
                    auto AddResult = OwnerNode->AddRuntimeComponentWithId<T>(Id);
                    if (AddResult)
                    {
                        auto ComponentResult = OwnerNode->RuntimeComponent<T>();
                        if (!ComponentResult)
                        {
                            return std::unexpected(ComponentResult.error());
                        }
                        return static_cast<void*>(&*ComponentResult);
                    }
                }
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "ECS-only components must be runtime-compatible and move constructible"));
        };
        EntryValue.Serialize = std::move(Serialize);
        EntryValue.Deserialize = std::move(Deserialize);
        EntryValue.OnCreate = [](void* Instance) -> TExpected<void> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null component instance"));
            }
            auto& TypedInstance = *static_cast<T*>(Instance);
            if constexpr (requires(T& Value) { Value.OnCreate(); })
            {
                TypedInstance.OnCreate();
            }
            else if constexpr (requires(T& Value, IWorld & WorldRef) { Value.OnCreate(WorldRef); })
            {
                IWorld* WorldPtr = TypedInstance.World();
                if (!WorldPtr)
                {
                    return std::unexpected(MakeError(EErrorCode::NotReady, "Component world is unavailable during OnCreate"));
                }
                TypedInstance.OnCreate(*WorldPtr);
            }
            else
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "Component type does not expose OnCreate lifecycle"));
            }
            return Ok();
        };
        GameLockGuard Lock(m_mutex);
        m_entries[Type] = std::move(EntryValue);
    }

    /**
     * @brief Check whether one Component type is registered.
     * @param Type Reflected Component type id.
     * @return `true` when the registry has an entry for @p Type.
     */
    bool Has(const TypeId& Type) const
    {
        GameLockGuard Lock(m_mutex);
        return m_entries.find(Type) != m_entries.end();
    }

    /**
     * @brief Return a snapshot of all currently registered Component type ids.
     * @return Copy of the registry's current type-id set.
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
     * @brief Create a Component instance by reflected type id.
     * @param WorldRef Destination World that will own the new Component.
     * @param Owner Owner Node handle that should receive the new Component.
     * @param Type Reflected Component type id.
     * @return Borrowed raw pointer to the newly created Component on success, or an error
     *         when the type is unknown or cannot be created with the default runtime
     *         component path.
     */
    TExpected<void*> Create(IWorld& WorldRef, const NodeHandle& Owner, const TypeId& Type) const;
    /**
     * @brief Create a Component instance by reflected type id with an explicit UUID.
     * @param WorldRef Destination World that will own the new Component.
     * @param Owner Owner Node handle that should receive the new Component.
     * @param Type Reflected Component type id.
     * @param Id Explicit runtime UUID to assign to the created Component.
     * @return Borrowed raw pointer to the newly created Component on success, or an error
     *         when the type is unknown or cannot be created with the default runtime
     *         component path.
     */
    TExpected<void*> CreateWithId(IWorld& WorldRef, const NodeHandle& Owner, const TypeId& Type, const Uuid& Id) const;
    /**
     * @brief Serialize one Component instance into its raw payload byte form.
     * @param Type Reflected Component type id.
     * @param Instance Pointer to the Component instance to serialize.
     * @param OutBytes Destination byte vector. Existing contents are replaced.
     * @param Context Borrowed serialization context.
     * @return `Ok()` on success or an error when no serializer is registered, the
     *         instance is invalid, or archive serialization throws.
     */
    TExpected<void> Serialize(const TypeId& Type, const void* Instance, std::vector<uint8_t>& OutBytes, const TSerializationContext& Context) const;
    /**
     * @brief Deserialize raw payload bytes into an existing Component instance.
     * @param Type Reflected Component type id.
     * @param Instance Destination Component instance. Must already exist.
     * @param Bytes Serialized payload bytes. May be null only when @p Size is zero.
     * @param Size Number of bytes in @p Bytes.
     * @param Context Borrowed serialization context.
     * @return `Ok()` on success or an error when no deserializer is registered, the input
     *         is invalid, or archive deserialization throws.
     *
     * @note The implementation automatically retries with
     * `UseLegacyFloatVectorDecode=true` when a decode error indicates one of the legacy
     * float-vector compatibility cases.
     */
    TExpected<void> Deserialize(const TypeId& Type, void* Instance, const uint8_t* Bytes, size_t Size, const TSerializationContext& Context) const;
    /**
     * @brief Invoke the deferred `OnCreate` lifecycle hook for one deserialized Component.
     * @param Type Reflected Component type id.
     * @param Instance Destination Component instance.
     * @return `Ok()` on success or an error when no callback is registered, the instance
     *         is invalid, or the callback throws.
     */
    TExpected<void> InvokeOnCreate(const TypeId& Type, void* Instance) const;

private:
    friend class NodeSerializer;

    /** @brief Registry entry bundling create, serialize, deserialize, and deferred-lifecycle callbacks for one Component type. */
    struct Entry
    {
        CreateFn Create{}; /**< @brief Creation callback for the registered Component type. */
        CreateWithIdFn CreateWithId{}; /**< @brief Creation callback that preserves an explicit Component UUID. */
        SerializeFn Serialize{}; /**< @brief Serializer that emits the Component's raw payload bytes. */
        DeserializeFn Deserialize{}; /**< @brief Deserializer that populates an existing Component from raw payload bytes. */
        OnCreateFn OnCreate{}; /**< @brief Deferred lifecycle callback invoked after deserialize has populated the Component's fields. */
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

    mutable GameMutex m_mutex{}; /**< @brief Guards registry entry map access. Callback execution happens outside the lock. */
    std::unordered_map<TypeId, Entry, UuidHash> m_entries{}; /**< @brief Reflected Component type id to registry-entry map. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialized representation of one Component attached to a Node.
 *
 * This is the smallest Component-level payload unit carried inside a `NodePayload`.
 * `Bytes` contains the Component's type-specific field data; construction policy and
 * ownership still come from the surrounding Node and World deserializer.
 */
struct NodeComponentPayload
{
    Uuid ComponentId{}; /**< @brief Serialized Component UUID. May be remapped during deserialization when `RegenerateObjectIds` is enabled. */
    TypeId ComponentType{}; /**< @brief Reflected concrete Component type used to recreate and deserialize the instance. */
    std::vector<uint8_t> Bytes{}; /**< @brief Raw serialized Component payload bytes produced by `ComponentSerializationRegistry`. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Recursive serialized representation of one Node subtree.
 *
 * A `NodePayload` carries everything needed to recreate one Node and its descendants:
 * - Node identity and reflected type
 * - name and active state
 * - optional reflected field bytes for the Node itself
 * - serialized attached Components
 * - recursive child payloads
 *
 * Deserialization semantics:
 * - The subtree structure is created first.
 * - Node field bytes and Component payloads are then applied in a second pass.
 * - Deferred node and component `OnCreate` hooks are requested only after field
 *   population.
 */
struct NodePayload
{
    Uuid NodeId{}; /**< @brief Serialized Node UUID. May be remapped during deserialization when `RegenerateObjectIds` is enabled. */
    TypeId NodeType{}; /**< @brief Reflected concrete Node type id. */
    std::string NodeTypeName{}; /**< @brief Reflected type name fallback used when `NodeType` is missing or unresolved in the receiving runtime. */
    std::string Name{}; /**< @brief Node name to assign after creation. */
    bool Active = true; /**< @brief Serialized active-state flag restored after the Node is created. */
    bool HasNodeData = false; /**< @brief Indicates whether `NodeBytes` contains serialized reflected Node fields. */
    std::vector<uint8_t> NodeBytes{}; /**< @brief Raw serialized Node field bytes emitted by reflection-based field walking. */
    std::vector<NodeComponentPayload> Components{}; /**< @brief Serialized Components that should be attached to this Node. */
    std::vector<NodePayload> Children{}; /**< @brief Serialized child subtrees. Editor-transient children are intentionally omitted from serializer output. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialized envelope for one Level's root-node set.
 *
 * `LevelPayload` stores the Level name plus the serialized subtrees rooted directly under
 * the Level. Existing destination Level contents are destroyed before deserialization
 * populates the new roots.
 */
struct LevelPayload
{
    std::string Name{}; /**< @brief Serialized Level name. */
    std::vector<NodePayload> Nodes{}; /**< @brief Serialized payloads for the Level's effective root Nodes. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Serialized envelope for one World's root-node set.
 *
 * `WorldPayload` stores the World name plus the serialized root Node subtrees. Existing
 * destination World contents are cleared before deserialization populates the new roots.
 */
struct WorldPayload
{
    std::string Name{}; /**< @brief Serialized World name. */
    std::vector<NodePayload> Nodes{}; /**< @brief Serialized payloads for the World's root Nodes. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Converts one Node subtree between live objects and `NodePayload`.
 *
 * `NodeSerializer` is the core graph serializer. It preserves subtree hierarchy, Node and
 * Component identity, and reflected field data while remaining agnostic to the specific
 * gameplay types present in the tree.
 *
 * Core semantics:
 * - Serialization skips editor-transient child Nodes.
 * - Deserialization creates the full Node tree first and applies fields in a second pass.
 * - Component creation is separated from Component `OnCreate`, which is explicitly
 *   deferred until deserialization has populated the Component fields.
 * - When `RegenerateObjectIds` is enabled, internal Node and Component handles are
 *   rewritten through remap tables built from the payload before any objects are created.
 *
 * @see LevelSerializer, WorldSerializer, TDeserializeOptions
 */
class NodeSerializer
{
public:
    /** @brief Current schema version for `NodePayload`. Consumers can use this for out-of-band format compatibility checks. */
    static constexpr uint32_t kSchemaVersion = 2;

    /**
     * @brief Serialize one live Node subtree into a `NodePayload`.
     * @param NodeRef Source Node whose subtree should be captured.
     * @return Serialized payload on success or an error when the subtree contains
     *         unresolved children or Components, or when a field serializer fails.
     */
    static TExpected<NodePayload> Serialize(const BaseNode& NodeRef);
    /**
     * @brief Deserialize one Node subtree into a World.
     * @param Payload Serialized subtree to materialize.
     * @param WorldRef Destination World used for Node and Component creation.
     * @param Parent Optional parent handle to attach the created root under. A null handle
     *        means "spawn as a World root".
     * @param Options Identity-remap behavior applied during decode.
     * @return Handle to the created root Node on success or an error when type resolution,
     *         creation, field deserialization, or attachment fails.
     */
    static TExpected<NodeHandle> Deserialize(const NodePayload& Payload,
                                             IWorld& WorldRef,
                                             const NodeHandle& Parent = {},
                                             const TDeserializeOptions& Options = {});
    /**
     * @brief Deserialize one Node subtree into a Level context.
     * @param Payload Serialized subtree to materialize.
     * @param LevelRef Destination Level. The Level must already be bound to a World.
     * @param Parent Optional parent handle to attach the created root under. A null handle
     *        means "attach under the Level handle when available, otherwise as a level
     *        root".
     * @param Options Identity-remap behavior applied during decode.
     * @return Handle to the created root Node on success or an error when the Level is not
     *         bound to a World, or when creation/deserialization fails.
     */
    static TExpected<NodeHandle> Deserialize(const NodePayload& Payload,
                                             Level& LevelRef,
                                             const NodeHandle& Parent = {},
                                             const TDeserializeOptions& Options = {});
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Converts one live Level to and from `LevelPayload`.
 *
 * `LevelSerializer` treats a Level as an envelope around its effective root Nodes. During
 * serialization it collects those roots and delegates each subtree to `NodeSerializer`.
 * During deserialization it destroys existing Level children, flushes the World once, and
 * then recreates the payload's roots.
 */
class LevelSerializer
{
public:
    /** @brief Current schema version for `LevelPayload`. Consumers can use this for out-of-band format compatibility checks. */
    static constexpr uint32_t kSchemaVersion = 6;

    /**
     * @brief Serialize one Level into a `LevelPayload`.
     * @param LevelRef Source Level.
     * @return Serialized payload on success or an error when any root subtree fails to
     *         serialize.
     */
    static TExpected<LevelPayload> Serialize(const Level& LevelRef);
    /**
     * @brief Replace a Level's current contents with a serialized payload.
     * @param Payload Serialized Level payload to load.
     * @param LevelRef Destination Level to overwrite.
     * @param Options Identity-remap behavior applied during decode.
     * @return `Ok()` on success or an error when the Level is not bound to a World, when
     *         existing children cannot be destroyed, or when any root subtree fails to
     *         deserialize.
     */
    static TExpected<void> Deserialize(const LevelPayload& Payload,
                                       Level& LevelRef,
                                       const TDeserializeOptions& Options = {});
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Converts one live World to and from `WorldPayload`.
 *
 * `WorldSerializer` is the top-level graph serializer. It captures the World's effective
 * root Nodes and delegates subtree work to `NodeSerializer`. During deserialization it
 * clears the destination World before recreating the payload roots.
 */
class WorldSerializer
{
public:
    /** @brief Current schema version for `WorldPayload`. Consumers can use this for out-of-band format compatibility checks. */
    static constexpr uint32_t kSchemaVersion = 6;

    /**
     * @brief Serialize one World into a `WorldPayload`.
     * @param WorldRef Source World.
     * @return Serialized payload on success or an error when any root subtree fails to
     *         serialize.
     */
    static TExpected<WorldPayload> Serialize(const World& WorldRef);
    /**
     * @brief Replace a World's current contents with a serialized payload.
     * @param Payload Serialized World payload to load.
     * @param WorldRef Destination World to overwrite.
     * @param Options Identity-remap behavior applied during decode.
     * @return `Ok()` on success or an error when root recreation or subtree deserialization
     *         fails.
     *
     * @post The destination World's existing contents are cleared before any payload Nodes
     *       are created.
     */
    static TExpected<void> Deserialize(const WorldPayload& Payload,
                                       World& WorldRef,
                                       const TDeserializeOptions& Options = {});
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Encode a `NodePayload` envelope into raw bytes.
 * @param Payload Payload to serialize.
 * @param OutBytes Destination byte vector. Existing contents are replaced.
 * @return `Ok()` on success or an error when archive encoding throws.
 */
TExpected<void> SerializeNodePayload(const NodePayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Decode a `NodePayload` envelope from raw bytes.
 * @param Bytes Source byte buffer. Must not be null when @p Size is non-zero.
 * @param Size Byte count.
 * @return Decoded payload on success or an error when the input is empty or archive
 *         decoding throws.
 */
TExpected<NodePayload> DeserializeNodePayload(const uint8_t* Bytes, size_t Size);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Encode a `LevelPayload` envelope into raw bytes.
 * @param Payload Payload to serialize.
 * @param OutBytes Destination byte vector. Existing contents are replaced.
 * @return `Ok()` on success or an error when archive encoding throws.
 */
TExpected<void> SerializeLevelPayload(const LevelPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Decode a `LevelPayload` envelope from raw bytes.
 * @param Bytes Source byte buffer. Must not be null when @p Size is non-zero.
 * @param Size Byte count.
 * @return Decoded payload on success or an error when the input is empty or archive
 *         decoding throws.
 */
TExpected<LevelPayload> DeserializeLevelPayload(const uint8_t* Bytes, size_t Size);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Encode a `WorldPayload` envelope into raw bytes.
 * @param Payload Payload to serialize.
 * @param OutBytes Destination byte vector. Existing contents are replaced.
 * @return `Ok()` on success or an error when archive encoding throws.
 */
TExpected<void> SerializeWorldPayload(const WorldPayload& Payload, std::vector<uint8_t>& OutBytes);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Decode a `WorldPayload` envelope from raw bytes.
 * @param Bytes Source byte buffer. Must not be null when @p Size is non-zero.
 * @param Size Byte count.
 * @return Decoded payload on success or an error when the input is empty or archive
 *         decoding throws.
 */
TExpected<WorldPayload> DeserializeWorldPayload(const uint8_t* Bytes, size_t Size);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register the framework's built-in value codecs and component serializers.
 *
 * This function wires the serialization layer to the framework's default reflected and
 * runtime component types. It is intended to run during startup after builtin reflected
 * types are available.
 *
 * Current behavior:
 * - registers scalar, string, UUID, math, and handle value codecs
 * - installs special runtime bindings for reflected container types that need explicit
 *   codec aliasing
 * - registers built-in Component serializers, including custom serializers where the
 *   default reflection format is not sufficient
 *
 * @note The function is not documented as idempotent. Repeated calls will re-register some
 * codec entries and advance the `ValueCodecRegistry` version counter.
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
