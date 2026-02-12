#pragma once

#include "Expected.h"
#include "Reflection.h"
#include "StaticTypeId.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Helper for registering reflected types with script bindings.
 * @remarks
 * Current implementation is a validation stub that ensures reflected metadata is present.
 * Backends can extend this class/pattern to emit concrete VM bindings.
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
        auto* Info = TypeRegistry::Instance().Find(StaticTypeId<T>());
        if (!Info)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
        }
        return Ok();
    }
};

} // namespace SnAPI::GameFramework
