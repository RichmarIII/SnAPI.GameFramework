#pragma once

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

private:
    mutable std::mutex m_mutex{}; /**< @brief Protects registry maps. */
    std::unordered_map<TypeId, TypeInfo, UuidHash> m_types{}; /**< @brief TypeId -> TypeInfo. */
    std::unordered_map<std::string, TypeId> m_nameToId{}; /**< @brief Name -> TypeId. */
};

} // namespace SnAPI::GameFramework
