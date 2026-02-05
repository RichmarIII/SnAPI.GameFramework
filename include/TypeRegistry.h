#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Expected.h"
#include "Invoker.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

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
 * @brief Reflection metadata for a field.
 * @remarks Getter/Setter use Variant for type-erased access.
 */
struct FieldInfo
{
    std::string Name; /**< @brief Field name as registered. */
    TypeId FieldType; /**< @brief TypeId of the field. */
    std::function<TExpected<Variant>(void* Instance)> Getter; /**< @brief Getter callback. */
    std::function<Result(void* Instance, const Variant& Value)> Setter; /**< @brief Setter callback. */
    std::function<TExpected<VariantView>(void* Instance)> ViewGetter; /**< @brief Non-owning getter. */
    std::function<const void*(const void* Instance)> ConstPointer; /**< @brief Direct const pointer accessor. */
    std::function<void*(void* Instance)> MutablePointer; /**< @brief Direct mutable pointer accessor. */
    bool IsConst = false; /**< @brief True if field is const-qualified. */
};

/**
 * @brief Reflection metadata for a method.
 * @remarks Invoke uses Variant arguments and returns Variant.
 */
struct MethodInfo
{
    std::string Name; /**< @brief Method name as registered. */
    TypeId ReturnType; /**< @brief Return type id. */
    std::vector<TypeId> ParamTypes; /**< @brief Parameter type ids. */
    MethodInvoker Invoke; /**< @brief Invocation callback. */
    bool IsConst = false; /**< @brief True if method is const-qualified. */
};

/**
 * @brief Reflection metadata for a constructor.
 * @remarks Construct returns a shared_ptr<void> for type-erased ownership.
 */
struct ConstructorInfo
{
    std::vector<TypeId> ParamTypes; /**< @brief Parameter type ids. */
    std::function<TExpected<std::shared_ptr<void>>(std::span<const Variant> Args)> Construct; /**< @brief Construction callback. */
};

/**
 * @brief Reflection metadata for a type.
 * @remarks Includes fields, methods, constructors, and base types.
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
 * @remarks Thread-safe registry used by serialization and scripting.
 * @note Types are keyed by TypeId.
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
     * @remarks Fails if the type is already registered.
     */
    TExpected<TypeInfo*> Register(TypeInfo Info);
    /**
     * @brief Find a type by TypeId.
     * @param Id TypeId to lookup.
     * @return Pointer to TypeInfo or nullptr if not found.
     */
    const TypeInfo* Find(const TypeId& Id) const;
    /**
     * @brief Find a type by name.
     * @param Name Fully qualified type name.
     * @return Pointer to TypeInfo or nullptr if not found.
     */
    const TypeInfo* FindByName(std::string_view Name) const;
    /**
     * @brief Check inheritance between two types.
     * @param Type Derived type id.
     * @param Base Base type id.
     * @return True if Type is-a Base.
     * @remarks Traverses registered base type chains.
     */
    bool IsA(const TypeId& Type, const TypeId& Base) const;
    /**
     * @brief Get all types derived from a base.
     * @param Base Base type id.
     * @return Vector of derived type infos.
     * @remarks Includes indirect derivatives.
     */
    std::vector<const TypeInfo*> Derived(const TypeId& Base) const;
    /**
     * @brief Enable or disable lock-free reads.
     * @param Enable True to freeze the registry (no further registration).
     * @remarks When enabled, read operations skip locking.
     */
    void Freeze(bool Enable);
    /**
     * @brief Check if the registry is frozen.
     * @return True if frozen.
     */
    bool IsFrozen() const;

private:
    mutable std::mutex m_mutex{}; /**< @brief Protects registry maps. */
    std::atomic<bool> m_frozen{false}; /**< @brief If true, reads skip locking and registration is disabled. */
    std::unordered_map<TypeId, TypeInfo, UuidHash> m_types{}; /**< @brief TypeId -> TypeInfo. */
    std::unordered_map<std::string, TypeId, TransparentStringHash, TransparentStringEqual> m_nameToId{}; /**< @brief Name -> TypeId. */
};

} // namespace SnAPI::GameFramework
