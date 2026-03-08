#pragma once

#if defined(SNAPI_GF_ENABLE_INPUT)

#include <functional>
#include <memory>
#include <vector>

#include "Expected.h"
#include "GameThreading.h"

#include <Input.h>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bootstrap settings for world-owned SnAPI.Input integration.
 *
 * `InputBootstrapSettings` captures the complete policy used when `InputSystem`
 * creates its single world-scoped `InputContext`. The settings are copied during
 * `InputSystem::Initialize(...)` and become the authoritative startup snapshot for
 * that system instance until the next reinitialization.
 *
 * Core semantics:
 * - exactly one backend is selected for context creation
 * - backend factory registration flags only affect initialization-time registry setup
 * - `CreateDesc` is forwarded directly into SnAPI.Input and therefore defines backend-specific behavior
 *
 * Validation:
 * - initialization fails if `Backend` is invalid
 * - initialization fails if the chosen backend factory is unavailable
 *
 * Threading model:
 * - Treat this as immutable configuration data after passing it to `Initialize(...)`.
 *
 * @see InputSystem
 */
struct InputBootstrapSettings
{
#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
    SnAPI::Input::EInputBackend Backend = SnAPI::Input::EInputBackend::SDL3; /**< @brief Backend selected for context creation. */
#elif defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) && SNAPI_INPUT_ENABLE_BACKEND_HIDAPI
    SnAPI::Input::EInputBackend Backend = SnAPI::Input::EInputBackend::HIDAPI; /**< @brief Backend selected for context creation. */
#elif defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) && SNAPI_INPUT_ENABLE_BACKEND_LIBUSB
    SnAPI::Input::EInputBackend Backend = SnAPI::Input::EInputBackend::LIBUSB; /**< @brief Backend selected for context creation. */
#else
    SnAPI::Input::EInputBackend Backend = SnAPI::Input::EInputBackend::Invalid; /**< @brief Backend selected for context creation. */
#endif
    SnAPI::Input::InputBackendCreateDesc CreateDesc{}; /**< @brief Context creation descriptor passed directly to SnAPI.Input backend creation. */

#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
    bool RegisterSdl3Backend = true; /**< @brief Auto-register SDL3 backend factory before creating context. */
#else
    bool RegisterSdl3Backend = false; /**< @brief Auto-register SDL3 backend factory before creating context. */
#endif

#if defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) && SNAPI_INPUT_ENABLE_BACKEND_HIDAPI
    bool RegisterHidApiBackend = true; /**< @brief Auto-register HIDAPI backend factory before creating context. */
#else
    bool RegisterHidApiBackend = false; /**< @brief Auto-register HIDAPI backend factory before creating context. */
#endif

#if defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) && SNAPI_INPUT_ENABLE_BACKEND_LIBUSB
    bool RegisterLibUsbBackend = true; /**< @brief Auto-register libusb backend factory before creating context. */
#else
    bool RegisterLibUsbBackend = false; /**< @brief Auto-register libusb backend factory before creating context. */
#endif
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned adapter over SnAPI.Input runtime/context.
 *
 * `InputSystem` gives each `World` a single normalized input pipeline. It owns the
 * underlying SnAPI.Input runtime registry and, once initialized, exactly one active
 * `InputContext`. Gameplay code, UI, and services read from that shared context instead
 * of each creating separate backend handles.
 *
 * Why this abstraction exists:
 * - to make input initialization follow world/runtime lifetime instead of global process lifetime
 * - to normalize backend selection behind a stable GameFramework surface
 * - to provide one place where per-frame pumping and cross-thread task marshalling are coordinated
 *
 * Core semantics:
 * - `Initialize(...)` replaces any previous context and settings snapshot
 * - `Pump()` is the frame boundary that refreshes snapshot, events, devices, and action state
 * - accessors such as `Snapshot()` and `Events()` return borrowed pointers into the active context
 * - borrowed pointers become invalid when the system shuts down or is reinitialized
 *
 * Ownership and lifetime:
 * - Owned by `World`.
 * - Owns the `InputRuntime` and active `InputContext`.
 * - Returned pointers are non-owning and must not be cached across lifecycle changes.
 *
 * Threading model:
 * - Main-thread oriented.
 * - Internal state is guarded by `GameMutex`, but callers should still treat mutation APIs as serialized.
 * - Cross-thread work should be marshaled via `EnqueueTask(...)`.
 *
 * Performance notes:
 * - `Pump()` is expected once per frame.
 * - Accessors are constant-time and allocation-free.
 *
 * @see World
 * @see InputBootstrapSettings
 */
class InputSystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(InputSystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;

    /** @brief Construct an uninitialized input system. */
    InputSystem() = default;
    /** @brief Destructor; shuts down active input context if initialized. */
    ~InputSystem();

    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    /**
     * @brief Move constructor; transfers runtime/context ownership.
     */
    InputSystem(InputSystem&& Other) noexcept;
    /**
     * @brief Move assignment; transfers runtime/context ownership safely.
     */
    InputSystem& operator=(InputSystem&& Other) noexcept;

    /**
     * @brief Initialize input system with default bootstrap settings.
     * @return Success or error.
     * @post On success, the system owns an initialized `InputContext`.
     * @warning Reinitializes the system and discards any previous context state.
     */
    Result Initialize();

    /**
     * @brief Initialize input system with explicit bootstrap settings.
     * @param Settings Input bootstrap settings copied into the system on success.
     * @return Success or error.
     * @post On success, `Settings()` returns a copy of @p Settings and the active context is recreated.
     * @warning Reinitializes the system and discards any previous context state.
     */
    Result Initialize(const InputBootstrapSettings& Settings);

    /**
     * @brief Shutdown active input context.
     * @remarks
     * Safe to call repeatedly. After shutdown, all pointers previously returned by
     * `Context()`, `Snapshot()`, `Events()`, `Devices()`, and `Actions()` are invalid.
     */
    void Shutdown();

    /**
     * @brief Check whether a context is initialized and ready for pumping.
     * @return True when initialized.
     */
    bool IsInitialized() const;

    /**
     * @brief Pump one input frame and update normalized snapshot/events.
     * @return Success or error.
     * @pre The system must be initialized.
     * @post On success, `Snapshot()`, `Events()`, `Devices()`, and `Actions()` reflect the latest backend state.
     * @warning This is the frame boundary for world input state; skipping it leaves consumers observing stale data.
     */
    Result Pump();

    /**
     * @brief Enqueue work on the input system thread.
     * @param InTask Work callback executed on input-thread affinity.
     * @param OnComplete Optional completion callback marshaled to caller dispatcher.
     * @return Task handle for wait/cancel polling.
     */
    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});

    /**
     * @brief Enqueue a generic thread task for dispatcher marshalling.
     * @param InTask Callback to execute on this system thread.
     */
    void EnqueueThreadTask(std::function<void()> InTask) override;

    /**
     * @brief Execute all queued tasks on the input thread.
     */
    void ExecuteQueuedTasks();

    /**
     * @brief Access active bootstrap settings snapshot.
     * @return Settings currently used by this subsystem.
     * @remarks Returns a borrowed reference owned by the subsystem.
     */
    const InputBootstrapSettings& Settings() const;

    /**
     * @brief Access mutable runtime registry/runtime facade.
     * @return Mutable runtime reference.
     * @remarks
     * Advanced use only. Prefer `Initialize(...)` for standard startup flow.
     * Mutating the runtime directly can change which backends or factories are available to future initialization calls.
     */
    SnAPI::Input::InputRuntime& Runtime();

    /**
     * @brief Access immutable runtime registry/runtime facade.
     * @return Immutable runtime reference.
     */
    const SnAPI::Input::InputRuntime& Runtime() const;

    /**
     * @brief Access active input context.
     * @return Non-owning context pointer or `nullptr` when uninitialized.
     * @warning The returned pointer is invalidated by `Shutdown()` and successful reinitialization.
     */
    SnAPI::Input::InputContext* Context();

    /**
     * @brief Access active input context (const).
     * @return Non-owning context pointer or `nullptr` when uninitialized.
     * @warning The returned pointer is invalidated by `Shutdown()` and successful reinitialization.
     */
    const SnAPI::Input::InputContext* Context() const;

    /**
     * @brief Access latest normalized snapshot.
     * @return Non-owning snapshot pointer or `nullptr` when uninitialized.
     * @remarks The snapshot contents change on each successful `Pump()`.
     */
    const SnAPI::Input::InputSnapshot* Snapshot() const;

    /**
     * @brief Access latest event stream.
     * @return Non-owning event-vector pointer or `nullptr` when uninitialized.
     * @remarks The pointed-to container belongs to the active input context.
     */
    const std::vector<SnAPI::Input::InputEvent>* Events() const;

    /**
     * @brief Access latest enumerated devices.
     * @return Non-owning device-vector pointer or `nullptr` when uninitialized.
     * @remarks Device objects are owned by the active backend/context.
     */
    const std::vector<std::shared_ptr<SnAPI::Input::IInputDevice>>* Devices() const;

    /**
     * @brief Access mutable action map bound to active context.
     * @return Non-owning action-map pointer or `nullptr` when uninitialized.
     * @remarks Mutations affect how future `Pump()` calls interpret backend input.
     */
    SnAPI::Input::ActionMap* Actions();

    /**
     * @brief Access immutable action map bound to active context.
     * @return Non-owning action-map pointer or `nullptr` when uninitialized.
     */
    const SnAPI::Input::ActionMap* Actions() const;

private:
    static Error MapInputError(const SnAPI::Input::Error& ErrorValue);
    Result RegisterConfiguredBackends(const InputBootstrapSettings& SettingsValue);
    Result ValidateBackendSelection(const InputBootstrapSettings& SettingsValue) const;
    void ShutdownUnlocked();

    mutable GameMutex m_mutex{}; /**< @brief Input-system thread affinity guard. */
    TSystemTaskQueue<InputSystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    InputBootstrapSettings m_settings{}; /**< @brief Active input bootstrap settings snapshot. */
    std::unique_ptr<SnAPI::Input::InputRuntime> m_runtime = std::make_unique<SnAPI::Input::InputRuntime>(); /**< @brief Owned SnAPI.Input runtime facade with backend registry. */
    std::unique_ptr<SnAPI::Input::InputContext> m_context{}; /**< @brief Active input context instance. */
    bool m_initialized = false; /**< @brief True when context has been initialized and can be pumped. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT
