#pragma once

#include <string>

#include "IComponent.h"
#include "ScriptEngine.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Component that binds a node to a script instance.
 * @remarks Stores module/type identifiers and the runtime instance id.
 */
class ScriptComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ScriptComponent";

    std::string ScriptModule; /**< @brief Script module path or name. */
    std::string ScriptType; /**< @brief Script type/class name. */
    ScriptInstanceId Instance = 0; /**< @brief Runtime instance id. */
};

} // namespace SnAPI::GameFramework
