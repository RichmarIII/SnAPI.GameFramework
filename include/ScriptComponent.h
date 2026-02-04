#pragma once

#include <string>

#include "IComponent.h"
#include "ScriptEngine.h"

namespace SnAPI::GameFramework
{

class ScriptComponent : public IComponent
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ScriptComponent";

    std::string ScriptModule;
    std::string ScriptType;
    ScriptInstanceId Instance = 0;
};

} // namespace SnAPI::GameFramework
