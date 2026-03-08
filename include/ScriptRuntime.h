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
 * @ingroup SnAPI_GameFramework
 * @brief Enumerates the script runtimes known to GameFramework.
 *
 * The enum provides a stable backend key for registration and selection. Using an enum
 * avoids string-based backend dispatch in gameplay code and makes it cheap to store one
 * backend slot per supported runtime.
 *
 * @note `None` is a sentinel meaning "no backend could be resolved".
 * @see ScriptRuntimeService, ScriptComponent
 */
enum class EScriptBackend : std::uint8_t
{
    None = 0, /**< @brief Sentinel used when no valid backend is available or selected. */
    Lua = 1, /**< @brief Built-in Lua backend slot. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Well-known component lifecycle hooks forwarded from `ScriptComponent` to `IScript`.
 *
 * These hook ids define the contract between engine-side component lifecycle events and
 * backend-side script execution.
 *
 * @see ScriptComponent, IScript::InvokeHook()
 */
enum class EScriptHook : std::uint8_t
{
    OnCreate = 0, /**< @brief Delivered once after a script instance is successfully bound for the current component lifetime. */
    OnDestroy = 1, /**< @brief Delivered before an existing bound instance is torn down, but only if `OnCreate` was previously delivered. */
    PreTick = 2, /**< @brief Delivered during the component pre-tick phase with `DeltaSeconds`. */
    Tick = 3, /**< @brief Delivered during the component main tick phase with `DeltaSeconds`. */
    FixedTick = 4, /**< @brief Delivered during the fixed-timestep simulation phase with `DeltaSeconds`. */
    LateTick = 5, /**< @brief Delivered during the component late-tick phase with `DeltaSeconds`. */
    PostTick = 6, /**< @brief Delivered during the component post-tick phase with `DeltaSeconds`. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Opaque runtime identifier assigned to one live script instance.
 *
 * The value is backend-generated and intended for diagnostics, caching, and change
 * detection. It is not a persistent asset id.
 *
 * @note `ScriptComponent` uses `0` to mean "currently unbound".
 */
using ScriptInstanceId = std::uint64_t;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Non-owning object context injected into a newly created script instance.
 *
 * The runtime passes this structure to the backend so the script can discover the World,
 * owner Node, and owner Component that caused it to be created.
 *
 * Ownership and lifetime:
 * - All pointers are borrowed.
 * - They remain valid only as long as the corresponding World and gameplay objects exist.
 * - Backends may copy the pointers into script-side state, but they do not acquire
 *   ownership.
 *
 * @warning The engine does not make these pointers thread-safe. Backends should treat
 * them as main-thread/world-thread objects unless the engine integration states otherwise.
 */
struct ScriptInstanceContext
{
    IWorld* World = nullptr; /**< @brief Borrowed World that owns the script runtime and gameplay graph. */
    BaseNode* OwnerNode = nullptr; /**< @brief Borrowed Node that owns the Component requesting the script instance. */
    BaseComponent* OwnerComponent = nullptr; /**< @brief Borrowed Component responsible for the script instance, typically a `ScriptComponent`. */
    TypeId OwnerComponentType{}; /**< @brief Reflected concrete component type. Used when the backend needs a stable type name even if only a base pointer is available. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Parameters required to create one script instance.
 *
 * `ScriptCreateInfo` describes what module to load, which entry point inside that module
 * to instantiate, and which gameplay objects should be exposed to the script as context.
 *
 * @see ScriptRuntimeService::CreateScript(), IScriptEngineBackend::CreateScript()
 */
struct ScriptCreateInfo
{
    std::string ScriptPath; /**< @brief Backend-specific script module path or module identifier. Typically a path that has already been normalized or can be normalized by the backend. */
    std::string EntryPoint; /**< @brief Optional backend-specific class, table, or factory name inside the module. Empty means "use the module root/default entry". */
    ScriptInstanceContext Context{}; /**< @brief Borrowed World and owner-object context to inject into the script instance. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime-facing interface for one live script instance.
 *
 * An `IScript` is the backend-neutral handle returned by the scripting runtime after a
 * module and entry point have been resolved successfully. The object encapsulates
 * backend state such as a Lua registry reference or VM object handle while exposing a
 * uniform engine API.
 *
 * Ownership and lifetime:
 * - Instances are returned as `std::shared_ptr<IScript>`.
 * - Callers may keep shared ownership for as long as the backend remains initialized and
 *   the script instance is logically valid.
 *
 * Threading:
 * - No thread-safety is guaranteed by this interface.
 * - In normal GameFramework usage, scripts are invoked from the main/world thread.
 *
 * Error semantics:
 * - Hook and member mutators fail by returning `Result`.
 * - Value-returning operations fail by returning `TExpected<Variant>`.
 *
 * @see IScriptEngineBackend, ScriptRuntimeService, ScriptComponent
 */
class IScript
{
public:
    virtual ~IScript() = default;

    /**
     * @brief Return the backend-assigned instance id.
     * @return Opaque non-persistent identifier for this live instance.
     */
    [[nodiscard]] virtual ScriptInstanceId InstanceId() const = 0;

    /**
     * @brief Return the backend that created this instance.
     * @return Backend enum value for the owning script runtime.
     */
    [[nodiscard]] virtual EScriptBackend BackendType() const = 0;

    /**
     * @brief Return the module path used to create this instance.
     * @return Borrowed string view into backend-owned storage. Valid for the lifetime of
     *         the script instance.
     */
    [[nodiscard]] virtual std::string_view ScriptPath() const = 0;

    /**
     * @brief Return the backend's current generation counter for the underlying module.
     * @return Hot-reload generation for the module that produced this instance.
     */
    [[nodiscard]] virtual std::uint64_t ModuleGeneration() const = 0;

    /**
     * @brief Invoke one well-known engine lifecycle hook.
     * @param Hook Hook identifier to run.
     * @param Args Optional arguments supplied by the engine, usually `DeltaSeconds` for
     *        tick hooks.
     * @return `Ok()` on success or an error describing the backend-side failure.
     */
    virtual Result InvokeHook(EScriptHook Hook, std::span<const Variant> Args = {}) = 0;

    /**
     * @brief Invoke an arbitrary backend-visible method on the script instance.
     * @param Method Method name in backend-defined naming conventions.
     * @param Args Optional `Variant` argument list in backend-defined order.
     * @return Reflected return value on success or an error describing why invocation
     *         failed.
     */
    virtual TExpected<Variant> Invoke(std::string_view Method, std::span<const Variant> Args = {}) = 0;

    /**
     * @brief Read a named script member.
     * @param Name Backend-defined member or property name.
     * @return Member value on success or an error when the member is absent or not
     *         readable.
     */
    virtual TExpected<Variant> GetMember(std::string_view Name) const = 0;

    /**
     * @brief Write a named script member.
     * @param Name Backend-defined member or property name.
     * @param Value New value to assign.
     * @return `Ok()` on success or an error when the member cannot be written.
     */
    virtual Result SetMember(std::string_view Name, const Variant& Value) = 0;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Backend interface implemented by each supported scripting language runtime.
 *
 * `IScriptEngineBackend` owns the VM-level integration details for one language. The
 * GameFramework runtime manages exactly one backend instance per `EScriptBackend` slot and
 * uses this interface to initialize the VM, load or reload modules, create live script
 * objects, and process hot-reload work.
 *
 * Ownership:
 * - `ScriptRuntimeService` takes ownership of backend implementations via `unique_ptr`.
 * - Backends create and return shared `IScript` instances to callers.
 *
 * Threading:
 * - This interface does not promise thread safety.
 * - Individual backends may add internal locking, but callers should still treat them as
 *   runtime services rather than free-threaded utilities.
 */
class IScriptEngineBackend
{
public:
    virtual ~IScriptEngineBackend() = default;

    /**
     * @brief Return the enum slot represented by this backend.
     * @return Backend identifier. Must not return `EScriptBackend::None`.
     */
    [[nodiscard]] virtual EScriptBackend BackendType() const = 0;

    /**
     * @brief Initialize the backend runtime.
     * @return `Ok()` on success or an error when the runtime cannot be started.
     *
     * @note `ScriptRuntimeService` calls this lazily before the first create or hot-reload
     * use.
     */
    virtual Result Initialize() = 0;

    /**
     * @brief Shut the backend runtime down.
     * @return `Ok()` on success or an error when shutdown reports a backend failure.
     *
     * @post The backend should release any runtime-owned script state it created.
     */
    virtual Result Shutdown() = 0;

    /**
     * @brief Load a module into the backend.
     * @param ScriptPath Backend-specific module path or module identifier.
     * @return `Ok()` on success or an error when the module cannot be loaded.
     */
    virtual Result LoadModule(std::string_view ScriptPath) = 0;

    /**
     * @brief Reload a previously known module.
     * @param ScriptPath Backend-specific module path or module identifier.
     * @return `Ok()` on success or an error when reload fails.
     */
    virtual Result ReloadModule(std::string_view ScriptPath) = 0;

    /**
     * @brief Query the current hot-reload generation for one module.
     * @param ScriptPath Backend-specific module path or module identifier.
     * @return Monotonic generation counter, or `0` when the backend has no known module
     *         record for that path.
     */
    [[nodiscard]] virtual std::uint64_t ModuleGeneration(std::string_view ScriptPath) const = 0;

    /**
     * @brief Create one live script instance.
     * @param CreateInfo Module path, entry point, and owner-object context.
     * @return Shared script instance on success or an error explaining why instance
     *         creation failed.
     */
    virtual TExpected<std::shared_ptr<IScript>> CreateScript(const ScriptCreateInfo& CreateInfo) = 0;

    /**
     * @brief Advance any backend-specific hot-reload work.
     * @return `Ok()` on success or the first backend error encountered during processing.
     *
     * @note `ScriptRuntimeService` only calls this for backends that have already been
     * initialized.
     */
    virtual Result TickHotReload() = 0;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned coordinator for all registered scripting backends.
 *
 * This service is the engine-facing entry point for script creation. It stores one backend
 * slot per `EScriptBackend`, initializes backends lazily on first use, and exposes a
 * backend-neutral API to gameplay systems such as `ScriptComponent`.
 *
 * Core semantics:
 * - Exactly zero or one backend may be registered for each enum slot.
 * - Backend registration does not initialize the backend immediately.
 * - `CreateScript()` ensures the selected backend is initialized before delegating to it.
 * - `Shutdown()` shuts down initialized backends but keeps the backend objects registered
 *   so they can be initialized again later.
 *
 * Ownership and lifetime:
 * - The service owns registered backends.
 * - Returned backend pointers are borrowed and remain valid until the service is
 *   destroyed or the backend registration model changes.
 *
 * Threading:
 * - Not thread-safe. The service mutates backend state and should be driven from a single
 *   owner thread, typically the World thread.
 *
 * @see RegisterBuiltinScriptBackends(), ScriptComponent, IScriptEngineBackend
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

    /**
     * @brief Register a backend implementation in its enum slot.
     * @param Backend Owning backend pointer. Ownership transfers to the runtime on
     *        success.
     * @return `Ok()` on success or an error when the pointer is null, the backend reports
     *         `EScriptBackend::None`, the enum is out of range, or a backend is already
     *         registered in that slot.
     */
    Result RegisterBackend(std::unique_ptr<IScriptEngineBackend> Backend);

    /**
     * @brief Check whether a backend is registered for one slot.
     * @param BackendType Backend slot to query.
     * @return `true` when a backend object is registered, regardless of initialization
     *         state.
     */
    [[nodiscard]] bool HasBackend(EScriptBackend BackendType) const;

    /**
     * @brief Borrow the backend registered for one slot.
     * @param BackendType Backend slot to query.
     * @return Borrowed backend pointer on success or an error when the slot is invalid or
     *         unregistered.
     */
    TExpected<IScriptEngineBackend*> Backend(EScriptBackend BackendType);

    /**
     * @brief Borrow the backend registered for one slot through a const view.
     * @param BackendType Backend slot to query.
     * @return Borrowed backend pointer on success or an error when the slot is invalid or
     *         unregistered.
     */
    TExpected<const IScriptEngineBackend*> Backend(EScriptBackend BackendType) const;

    /**
     * @brief Create a script instance using one backend slot.
     * @param BackendType Backend slot to use.
     * @param CreateInfo Module path, entry point, and owner-object context.
     * @return Shared script instance on success or an error when the backend is missing,
     *         cannot be initialized, or cannot create the instance.
     */
    TExpected<std::shared_ptr<IScript>> CreateScript(EScriptBackend BackendType, const ScriptCreateInfo& CreateInfo);

    /**
     * @brief Query a backend's current hot-reload generation for a module path.
     * @param BackendType Backend slot to query.
     * @param ScriptPath Backend-specific module path or module identifier.
     * @return Generation counter, or `0` when the backend slot is invalid or unregistered,
     *         or when the backend has no known module record for that path.
     */
    [[nodiscard]] std::uint64_t ModuleGeneration(EScriptBackend BackendType, std::string_view ScriptPath) const;

    /**
     * @brief Advance hot-reload processing on every initialized backend.
     * @return `Ok()` on success or the first backend error encountered.
     *
     * @note Uninitialized backends are skipped.
     */
    Result TickHotReload();

    /**
     * @brief Shut down all initialized backends.
     *
     * Backend objects remain registered after shutdown; only their initialized runtime
     * state is torn down.
     */
    void Shutdown();

private:
    struct RuntimeEntry
    {
        std::unique_ptr<IScriptEngineBackend> Backend; /**< @brief Owning backend instance for one enum slot. */
        bool Initialized = false; /**< @brief True once `Initialize()` has succeeded and until `Shutdown()` resets the slot. */
    };

    [[nodiscard]] static std::size_t BackendIndex(EScriptBackend BackendType);
    Result EnsureBackendInitialized(RuntimeEntry& Entry);

    static constexpr std::size_t kBackendSlotCount = static_cast<std::size_t>(EScriptBackend::Lua) + 1u; /**< @brief Size of the fixed backend slot array. Extend this when new backend enum values are added. */
    RuntimeEntry m_entries[kBackendSlotCount]{};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register the built-in script backends compiled into GameFramework.
 *
 * The current implementation registers the Lua backend when Lua support is enabled at
 * build time. Registration failures are logged to `stderr`; they are not surfaced through
 * a return value.
 *
 * @param Runtime Runtime service that will own any successfully registered built-in
 *        backends.
 */
SNAPI_GAMEFRAMEWORK_API void RegisterBuiltinScriptBackends(ScriptRuntimeService& Runtime);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a backend enum to a diagnostic string.
 * @param Backend Backend enum value to stringify.
 * @return Static null-terminated string naming the backend.
 */
SNAPI_GAMEFRAMEWORK_API const char* ToString(EScriptBackend Backend);

} // namespace SnAPI::GameFramework
