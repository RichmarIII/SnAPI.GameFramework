#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "Expected.h"
#include "Export.h"
#include "Uuid.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

class IWorld;
class BaseNode;
class BaseComponent;

/**
 * @brief Enumerates built-in script backend identifiers.
 * @remarks
 * Backend identifiers are enum-based to avoid stringly-typed backend selection.
 */
enum class EScriptBackend : std::uint8_t
{
    None = 0,
    Lua = 1,
};

/**
 * @brief Well-known component lifecycle hooks forwarded to scripts.
 */
enum class EScriptHook : std::uint8_t
{
    OnCreate = 0,
    OnDestroy = 1,
    PreTick = 2,
    Tick = 3,
    FixedTick = 4,
    LateTick = 5,
    PostTick = 6,
};

/**
 * @brief Runtime script instance id.
 */
using ScriptInstanceId = std::uint64_t;

/**
 * @brief Context passed when creating a script instance.
 */
struct ScriptInstanceContext
{
    IWorld* World = nullptr;
    BaseNode* OwnerNode = nullptr;
    BaseComponent* OwnerComponent = nullptr;
    TypeId OwnerComponentType{};
};

/**
 * @brief Parameters used to create one script instance.
 */
struct ScriptCreateInfo
{
    std::string ScriptPath;
    std::string EntryPoint;
    ScriptInstanceContext Context{};
};

/**
 * @brief Runtime handle for a script instance.
 */
class IScript
{
public:
    virtual ~IScript() = default;

    [[nodiscard]] virtual ScriptInstanceId InstanceId() const = 0;
    [[nodiscard]] virtual EScriptBackend BackendType() const = 0;
    [[nodiscard]] virtual std::string_view ScriptPath() const = 0;
    [[nodiscard]] virtual std::uint64_t ModuleGeneration() const = 0;

    virtual Result InvokeHook(EScriptHook Hook, std::span<const Variant> Args = {}) = 0;
    virtual TExpected<Variant> Invoke(std::string_view Method, std::span<const Variant> Args = {}) = 0;

    virtual TExpected<Variant> GetMember(std::string_view Name) const = 0;
    virtual Result SetMember(std::string_view Name, const Variant& Value) = 0;
};

/**
 * @brief Script backend interface implemented per language runtime.
 */
class IScriptEngineBackend
{
public:
    virtual ~IScriptEngineBackend() = default;

    [[nodiscard]] virtual EScriptBackend BackendType() const = 0;

    virtual Result Initialize() = 0;
    virtual Result Shutdown() = 0;

    virtual Result LoadModule(std::string_view ScriptPath) = 0;
    virtual Result ReloadModule(std::string_view ScriptPath) = 0;
    [[nodiscard]] virtual std::uint64_t ModuleGeneration(std::string_view ScriptPath) const = 0;

    virtual TExpected<std::shared_ptr<IScript>> CreateScript(const ScriptCreateInfo& CreateInfo) = 0;

    virtual Result TickHotReload() = 0;
};

/**
 * @brief World-owned scripting runtime orchestrator.
 */
class SNAPI_GAMEFRAMEWORK_API ScriptRuntimeService
{
public:
    ScriptRuntimeService() = default;
    ~ScriptRuntimeService();

    ScriptRuntimeService(const ScriptRuntimeService&) = delete;
    ScriptRuntimeService& operator=(const ScriptRuntimeService&) = delete;
    ScriptRuntimeService(ScriptRuntimeService&&) = delete;
    ScriptRuntimeService& operator=(ScriptRuntimeService&&) = delete;

    Result RegisterBackend(std::unique_ptr<IScriptEngineBackend> Backend);
    [[nodiscard]] bool HasBackend(EScriptBackend BackendType) const;

    TExpected<IScriptEngineBackend*> Backend(EScriptBackend BackendType);
    TExpected<const IScriptEngineBackend*> Backend(EScriptBackend BackendType) const;

    TExpected<std::shared_ptr<IScript>> CreateScript(EScriptBackend BackendType, const ScriptCreateInfo& CreateInfo);
    [[nodiscard]] std::uint64_t ModuleGeneration(EScriptBackend BackendType, std::string_view ScriptPath) const;

    Result TickHotReload();
    void Shutdown();

private:
    struct RuntimeEntry
    {
        std::unique_ptr<IScriptEngineBackend> Backend;
        bool Initialized = false;
    };

    [[nodiscard]] static std::size_t BackendIndex(EScriptBackend BackendType);
    Result EnsureBackendInitialized(RuntimeEntry& Entry);

    static constexpr std::size_t kBackendSlotCount = static_cast<std::size_t>(EScriptBackend::Lua) + 1u;
    RuntimeEntry m_entries[kBackendSlotCount]{};
};

SNAPI_GAMEFRAMEWORK_API void RegisterBuiltinScriptBackends(ScriptRuntimeService& Runtime);
SNAPI_GAMEFRAMEWORK_API const char* ToString(EScriptBackend Backend);

} // namespace SnAPI::GameFramework
