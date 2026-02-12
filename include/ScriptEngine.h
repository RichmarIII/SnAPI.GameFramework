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

/**
 * @brief Unique identifier for a script instance.
 * @remarks Provided by the script runtime implementation.
 */
using ScriptInstanceId = uint64_t;

/**
 * @brief Interface for a scripting backend (Lua, C#, etc).
 * @remarks
 * Engine abstraction deliberately keeps transport and VM details outside GameFramework.
 * Backends are expected to interoperate with Variant-based invocation and reflected type ids.
 */
class IScriptEngine
{
public:
    /** @brief Virtual destructor. */
    virtual ~IScriptEngine() = default;

    /**
     * @brief Initialize the scripting runtime.
     * @return Success or error.
     */
    virtual TExpected<void> Initialize() = 0;
    /**
     * @brief Shutdown the scripting runtime.
     * @return Success or error.
     */
    virtual TExpected<void> Shutdown() = 0;
    /**
     * @brief Load a script module from disk.
     * @param Path Module path.
     * @return Success or error.
     * @remarks Module identity/path semantics are backend-defined.
     */
    virtual TExpected<void> LoadModule(const std::string& Path) = 0;
    /**
     * @brief Reload a script module from disk.
     * @param Path Module path.
     * @return Success or error.
     */
    virtual TExpected<void> ReloadModule(const std::string& Path) = 0;
    /**
     * @brief Create a script instance of a type.
     * @param TypeName Fully qualified script type name.
     * @return Script instance id or error.
     */
    virtual TExpected<ScriptInstanceId> CreateInstance(const std::string& TypeName) = 0;
    /**
     * @brief Destroy a script instance.
     * @param Instance Instance id to destroy.
     * @return Success or error.
     */
    virtual TExpected<void> DestroyInstance(ScriptInstanceId Instance) = 0;
    /**
     * @brief Invoke a method on a script instance.
     * @param Instance Instance id.
     * @param Method Method name.
     * @param Args Argument list.
     * @return Variant result or error.
     * @remarks Argument conversion and method binding behavior are backend-defined.
     */
    virtual TExpected<Variant> Invoke(ScriptInstanceId Instance, std::string_view Method, std::span<const Variant> Args) = 0;
};

/**
 * @brief Wrapper owning a scripting engine instance.
 * @remarks Small orchestration wrapper for engine lifecycle and shared ownership semantics.
 */
class ScriptRuntime
{
public:
    /**
     * @brief Construct with an engine instance.
     * @param Engine Shared engine pointer.
     */
    explicit ScriptRuntime(std::shared_ptr<IScriptEngine> Engine)
        : m_engine(std::move(Engine))
    {
    }

    /**
     * @brief Initialize the runtime.
     * @return Success or error.
     */
    TExpected<void> Initialize()
    {
        if (!m_engine)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Script engine not set"));
        }
        return m_engine->Initialize();
    }

    /**
     * @brief Shutdown the runtime.
     * @return Success or error.
     */
    TExpected<void> Shutdown()
    {
        if (!m_engine)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Script engine not set"));
        }
        return m_engine->Shutdown();
    }

    /**
     * @brief Get the underlying engine.
     * @return Shared pointer to the engine.
     */
    std::shared_ptr<IScriptEngine> Engine() const
    {
        return m_engine;
    }

private:
    std::shared_ptr<IScriptEngine> m_engine{}; /**< @brief Owned engine instance. */
};

} // namespace SnAPI::GameFramework
