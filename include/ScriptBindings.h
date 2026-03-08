#pragma once

#include "Expected.h"
#include "Reflection.h"
#include "StaticTypeId.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection-validation entry point for script-binding registration.
 *
 * `ScriptBindings` currently acts as a minimal front door for script backends that need
 * to ensure a type is reflected before generating or attaching bindings.
 *
 * Current semantics:
 * - No backend-specific binding tables are emitted here yet.
 * - `RegisterType<T>()` succeeds only when `T` is already present in `TypeRegistry`.
 * - Missing reflection metadata is reported as `EErrorCode::NotFound`.
 *
 * This keeps the public API stable while the concrete scripting backends evolve.
 */
class ScriptBindings
{
public:
    /**
     * @brief Validate that a type is reflected and therefore eligible for scripting integration.
     * @tparam T Type to register.
     * @return Success when reflection metadata exists for `T`; otherwise an error.
     *
     * Backends can extend this pattern to emit VM bindings, native thunks, or ABI glue
     * after the metadata presence check succeeds.
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
