#pragma once

#include <atomic>
#include "GameThreading.h"
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Expected.h"
#include "Flags.h"
#include "Invoker.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Heterogeneous hash functor for string-key lookups.
 * @remarks Supports `std::string` and `std::string_view` without transient allocations.
 */
struct TransparentStringHash
{
    using is_transparent = void;

    size_t operator()(std::string_view Value) const noexcept
    {
        return std::hash<std::string_view>{}(Value);
    }

    size_t operator()(const std::string& Value) const noexcept
    {
        return std::hash<std::string_view>{}(Value);
    }
};

/**
 * @brief Heterogeneous equality functor for string-key lookups.
 * @remarks Paired with TransparentStringHash for transparent unordered-map lookup.
 */
struct TransparentStringEqual
{
    using is_transparent = void;

    bool operator()(std::string_view Left, std::string_view Right) const noexcept
    {
        return Left == Right;
    }

    bool operator()(std::string_view Left, const std::string& Right) const noexcept
    {
        return Left == std::string_view(Right);
    }

    bool operator()(const std::string& Left, std::string_view Right) const noexcept
    {
        return std::string_view(Left) == Right;
    }

    bool operator()(const std::string& Left, const std::string& Right) const noexcept
    {
        return Left == Right;
    }
};

/**
 * @brief Field-level flags for reflection metadata.
 */
enum class EFieldFlagBits : uint32_t
{
    None = 0, /**< @brief No special field behavior flags. */
    Replication = 1u << 0, /**< @brief Field is eligible for replication payload traversal. */
};

using FieldFlags = TFlags<EFieldFlagBits>;
template<>
struct EnableFlags<EFieldFlagBits> : std::true_type
{
};

/**
 * @brief Method-level flags for reflection metadata.
 */
enum class EMethodFlagBits : uint32_t
{
    None = 0, /**< @brief No special method behavior flags. */
    RpcReliable = 1u << 0, /**< @brief Prefer reliable transport channel for RPC dispatch. */
    RpcUnreliable = 1u << 1, /**< @brief Prefer unreliable transport channel for RPC dispatch. */
    RpcNetServer = 1u << 2, /**< @brief Method is intended as server-target endpoint. */
    RpcNetClient = 1u << 3, /**< @brief Method is intended as client-target endpoint. */
    RpcNetMulticast = 1u << 4, /**< @brief Method is intended for server-initiated multicast dispatch. */
};

using MethodFlags = TFlags<EMethodFlagBits>;
template<>
struct EnableFlags<EMethodFlagBits> : std::true_type
{
};

/**
 * @brief Reflection metadata for a field.
 * @remarks
 * Field access supports three lanes:
 * - Variant getter/setter for generic scripting/tooling pipelines
 * - VariantView for non-owning fast paths
 * - direct pointer accessors for hot serialization/replication code paths
 */
struct FieldInfo
{
    std::string Name; /**< @brief Field name as registered. */
    TypeId FieldType; /**< @brief TypeId of the field. */
    FieldFlags Flags{}; /**< @brief Field flags (replication, etc.). */
    std::function<TExpected<Variant>(void* Instance)> Getter; /**< @brief Getter callback. */
    std::function<Result(void* Instance, const Variant& Value)> Setter; /**< @brief Setter callback. */
    std::function<TExpected<VariantView>(void* Instance)> ViewGetter; /**< @brief Non-owning getter. */
    std::function<const void*(const void* Instance)> ConstPointer; /**< @brief Direct const pointer accessor. */
    std::function<void*(void* Instance)> MutablePointer; /**< @brief Direct mutable pointer accessor. */
    bool IsConst = false; /**< @brief True if field is const-qualified. */
};

/**
 * @brief Reflection metadata for a method.
 * @remarks Invocation uses variant-packed arguments and variant return payload.
 */
struct MethodInfo
{
    std::string Name; /**< @brief Method name as registered. */
    TypeId ReturnType; /**< @brief Return type id. */
    std::vector<TypeId> ParamTypes; /**< @brief Parameter type ids. */
    MethodInvoker Invoke; /**< @brief Invocation callback. */
    MethodFlags Flags{}; /**< @brief Method flags (rpc, etc.). */
    bool IsConst = false; /**< @brief True if method is const-qualified. */
};

/**
 * @brief Reflection metadata for a constructor.
 * @remarks Construct callback returns owning `shared_ptr<void>` for type-erased instance creation.
 */
struct ConstructorInfo
{
    std::vector<TypeId> ParamTypes; /**< @brief Parameter type ids. */
    std::function<TExpected<std::shared_ptr<void>>(std::span<const Variant> Args)> Construct; /**< @brief Construction callback. */
};

/**
 * @brief Reflection metadata for a type.
 * @remarks Central metadata object consumed by serialization, replication, RPC and tooling.
 */
struct TypeInfo
{
    TypeId Id; /**< @brief Type id (UUID). */
    std::string Name; /**< @brief Fully qualified type name. */
    size_t Size = 0; /**< @brief sizeof(T). */
    size_t Align = 0; /**< @brief alignof(T). */
    std::vector<TypeId> BaseTypes; /**< @brief Base class TypeIds. */
    std::vector<FieldInfo> Fields; /**< @brief Field metadata. */
    std::vector<MethodInfo> Methods; /**< @brief Method metadata. */
    std::vector<ConstructorInfo> Constructors; /**< @brief Constructor metadata. */
};

/**
 * @brief Global registry for reflected types.
 * @remarks
 * Canonical runtime metadata index keyed by deterministic `TypeId`.
 *
 * Read/write model:
 * - normal mode: read/write operations use mutex protection
 * - frozen mode (`Freeze(true)`): read operations use lock-free fast path and registrations are rejected
 *
 * This enables high-frequency lookup paths (replication/serialization) to avoid lock contention
 * after startup metadata registration has completed.
 */
class TypeRegistry
{
public:
    /**
     * @brief Access the singleton TypeRegistry instance.
     * @return Reference to the registry.
     */
    static TypeRegistry& Instance();

    /**
     * @brief Register a new type.
     * @param Info Type metadata.
     * @return Pointer to the stored TypeInfo or error.
     * @remarks Fails on duplicate id/name or when registry is frozen.
     */
    TExpected<TypeInfo*> Register(TypeInfo Info);
    /**
     * @brief Find a type by TypeId.
     * @param Id TypeId to lookup.
     * @return Pointer to TypeInfo or nullptr if not found.
     * @remarks May trigger lazy auto-registration through TypeAutoRegistry on first miss.
     */
    const TypeInfo* Find(const TypeId& Id) const;
    /**
     * @brief Find a type by name.
     * @param Name Fully qualified type name.
     * @return Pointer to TypeInfo or nullptr if not found.
     * @remarks Heterogeneous lookup via transparent string hash/equality.
     */
    const TypeInfo* FindByName(std::string_view Name) const;
    /**
     * @brief Check inheritance between two types.
     * @param Type Derived type id.
     * @param Base Base type id.
     * @return True if Type is-a Base.
     * @remarks Traverses reflected base graph; requires base metadata to be registered.
     */
    bool IsA(const TypeId& Type, const TypeId& Base) const;
    /**
     * @brief Get all types derived from a base.
     * @param Base Base type id.
     * @return Vector of derived type infos.
     * @remarks Includes transitive derivatives in current registry snapshot.
     */
    std::vector<const TypeInfo*> Derived(const TypeId& Base) const;
    /**
     * @brief Enable or disable lock-free reads.
     * @param Enable True to freeze the registry (no further registration).
     * @remarks
     * Freeze should be enabled only after expected metadata registration is complete.
     * Unfreezing re-enables mutation and lock-based reads.
     */
    void Freeze(bool Enable);
    /**
     * @brief Check if the registry is frozen.
     * @return True if frozen.
     */
    bool IsFrozen() const;

private:
    mutable GameMutex m_mutex{}; /**< @brief Guards registry mutation and non-frozen lookups. */
    std::atomic<bool> m_frozen{false}; /**< @brief Frozen state flag controlling read/write mode behavior. */
    std::unordered_map<TypeId, TypeInfo, UuidHash> m_types{}; /**< @brief Primary metadata store keyed by TypeId. */
    std::unordered_map<std::string, TypeId, TransparentStringHash, TransparentStringEqual> m_nameToId{}; /**< @brief Secondary name index for lookup by stable type name. */
};

} // namespace SnAPI::GameFramework
