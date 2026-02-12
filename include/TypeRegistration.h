#pragma once

#include <type_traits>

#include "Assert.h"
#include "StaticTypeId.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Ensure a type is registered in TypeRegistry and return its TypeId pointer.
 * @tparam T Type with a stable TTypeNameV<T>.
 * @return Pointer to a stable TypeId on success, or error.
 * @remarks This is the "register on first use" path.
 */
template<typename T>
inline TExpected<TypeId*> StaticType()
{
    const TypeId& Id = StaticTypeId<T>();
    if (TypeRegistry::Instance().Find(Id))
    {
        return const_cast<TypeId*>(&Id);
    }

    auto EnsureResult = TypeAutoRegistry::Instance().Ensure(Id);
    if (!EnsureResult)
    {
        // If the type was manually registered elsewhere, treat that as success.
        if (TypeRegistry::Instance().Find(Id))
        {
            return const_cast<TypeId*>(&Id);
        }
        return std::unexpected(EnsureResult.error());
    }

    if (!TypeRegistry::Instance().Find(Id))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered after ensure"));
    }
    return const_cast<TypeId*>(&Id);
}

/**
 * @brief Ensure reflection registration for a type.
 * @tparam T Type with a stable TTypeNameV<T>.
 * @remarks Used by template APIs (CreateNode/AddComponent, etc).
 */
template<typename T>
inline void EnsureReflectionRegistered()
{
    auto Result = StaticType<T>();
    DEBUG_ASSERT(Result.has_value(), "Failed to ensure reflection registration for type: {}", std::string(TTypeNameV<T>));
}

/**
 * @brief Register built-in types and default serializers.
 * @remarks Must be called once at startup before using reflection/serialization.
 * @note Safe to call multiple times; duplicate registrations are ignored or fail gracefully.
 */
void RegisterBuiltinTypes();

} // namespace SnAPI::GameFramework
