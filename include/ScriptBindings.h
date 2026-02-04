#pragma once

#include "Expected.h"
#include "Reflection.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Helper for registering reflected types with script bindings.
 * @remarks Currently validates that types are registered in TypeRegistry.
 */
class ScriptBindings
{
public:
    /**
     * @brief Register a type for scripting.
     * @tparam T Type to register.
     * @return Success or error if the type is not reflected.
     * @remarks Extend this for backend-specific binding generation.
     */
    template<typename T>
    static TExpected<void> RegisterType()
    {
        auto* Info = TypeRegistry::Instance().Find(TypeIdFromName(TTypeNameV<T>));
        if (!Info)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
        }
        return Ok();
    }
};

} // namespace SnAPI::GameFramework
