#pragma once

#include <type_traits>

#include "Assert.h"
#include "StaticTypeId.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Ensure that a type's reflection metadata has been registered and return its `TypeId`.
 * @tparam T Type with a stable `TTypeNameV<T>`.
 * @return Pointer to the stable `TypeId` on success, or an error.
 *
 * This is the central register-on-first-use entrypoint used by reflective template APIs.
 *
 * Core semantics:
 * - Fast path: if the type already exists in `TypeRegistry`, return immediately.
 * - Slow path: invoke `TypeAutoRegistry::Ensure()` for the deterministic `StaticTypeId<T>()`.
 * - If auto-registration reports an error but the type is already present, the existing registration wins.
 * - If ensure succeeds but the type is still absent, the call fails.
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
 * @ingroup SnAPI_GameFramework
 * @brief Assert that reflection registration exists for a type.
 * @tparam T Type with a stable `TTypeNameV<T>`.
 *
 * This is the assert-checked convenience wrapper used by higher-level template APIs such as node or
 * component creation helpers. In debug builds it surfaces missing registration early instead of
 * letting later reflective operations fail indirectly.
 */
template<typename T>
inline void EnsureReflectionRegistered()
{
    auto Result = StaticType<T>();
    DEBUG_ASSERT(Result.has_value(), "Failed to ensure reflection registration for type: {}", std::string(TTypeNameV<T>));
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register the framework's builtin reflection metadata set.
 *
 * This installs plain metadata for primitive, math, handle, and subsystem enum types that do not use
 * `SNAPI_REFLECT_TYPE`. The implementation intentionally ignores duplicate-registration failures, so
 * the function behaves as a best-effort idempotent bootstrap step.
 *
 * Usage guidance:
 * - Call during runtime bootstrap before systems rely on reflection, serialization, scripting, or RPC.
 * - Call before freezing the `TypeRegistry`.
 */
void RegisterBuiltinTypes();

} // namespace SnAPI::GameFramework
