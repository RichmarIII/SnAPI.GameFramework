#pragma once

#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Get the deterministic `TypeId` for a C++ type.
 * @tparam T Type with a stable `TTypeNameV<T>`.
 * @return Reference to a function-local cached `TypeId`.
 *
 * This helper performs pure name-based identity derivation and does not require the type to be
 * registered in `TypeRegistry`. It is therefore safe on cold paths that need a stable id before
 * reflection metadata exists.
 *
 * Performance:
 * - The `TypeId` is computed once per process per type and then reused by reference.
 */
template<typename T>
inline const TypeId& StaticTypeId()
{
    static const TypeId Id = TypeIdFromName(ReflectedTypeName<T>());
    return Id;
}

} // namespace SnAPI::GameFramework
