#pragma once

#include "Expected.h"
#include "Reflection.h"

namespace SnAPI::GameFramework
{

class ScriptBindings
{
public:
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
