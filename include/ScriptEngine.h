#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Expected.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

using ScriptInstanceId = uint64_t;

class IScriptEngine
{
public:
    virtual ~IScriptEngine() = default;

    virtual TExpected<void> Initialize() = 0;
    virtual TExpected<void> Shutdown() = 0;
    virtual TExpected<void> LoadModule(const std::string& Path) = 0;
    virtual TExpected<void> ReloadModule(const std::string& Path) = 0;
    virtual TExpected<ScriptInstanceId> CreateInstance(const std::string& TypeName) = 0;
    virtual TExpected<void> DestroyInstance(ScriptInstanceId Instance) = 0;
    virtual TExpected<Variant> Invoke(ScriptInstanceId Instance, std::string_view Method, std::span<const Variant> Args) = 0;
};

class ScriptRuntime
{
public:
    explicit ScriptRuntime(std::shared_ptr<IScriptEngine> Engine)
        : m_engine(std::move(Engine))
    {
    }

    TExpected<void> Initialize()
    {
        if (!m_engine)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Script engine not set"));
        }
        return m_engine->Initialize();
    }

    TExpected<void> Shutdown()
    {
        if (!m_engine)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Script engine not set"));
        }
        return m_engine->Shutdown();
    }

    std::shared_ptr<IScriptEngine> Engine() const
    {
        return m_engine;
    }

private:
    std::shared_ptr<IScriptEngine> m_engine{};
};

} // namespace SnAPI::GameFramework
