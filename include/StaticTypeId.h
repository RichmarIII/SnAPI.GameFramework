#pragma once

#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Get the deterministic TypeId for a type, cached in a function-local static.
 * @tparam T Type with a stable TTypeNameV<T>.
 * @return Stable TypeId reference.
 * @remarks This avoids calling TypeIdFromName repeatedly on hot paths.
 */
template<typename T>
inline const TypeId& StaticTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<T>);
    return Id;
}

} // namespace SnAPI::GameFramework

