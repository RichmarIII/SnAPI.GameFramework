#pragma once

#include <string>

#include "IComponent.h"
#include "ScriptEngine.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Component that binds a node to a script instance.
 * @remarks
 * Data-only binding contract used by scripting integration layers.
 * Engine glue is expected to:
 * - resolve `ScriptModule` + `ScriptType`
 * - create/destroy `Instance`
 * - drive scripted lifecycle callbacks as desired
 */
class ScriptComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ScriptComponent";

    std::string ScriptModule; /**< @brief Backend-defined module identifier/path used for loading. */
    std::string ScriptType; /**< @brief Backend-defined type/class identifier instantiated from module. */
    ScriptInstanceId Instance = 0; /**< @brief Live runtime instance id (0 indicates not currently bound). */
};

} // namespace SnAPI::GameFramework
