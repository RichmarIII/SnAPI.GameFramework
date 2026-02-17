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
 * @brief Bootstrap settings for world-owned SnAPI.Input integration.
 * @remarks
 * This settings object controls:
 * - Which backend is instantiated for the world input context.
 * - Which built-in backend factories are auto-registered into the runtime registry.
 * - Per-context feature switches via `InputBackendCreateDesc`.
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
 * @brief World-owned adapter over SnAPI.Input runtime/context.
 * @remarks
 * This subsystem provides a single world-scoped input context with:
 * - explicit initialize/shutdown lifecycle,
 * - backend-factory registration for shipped backends,
 * - per-frame pumping that updates normalized snapshot/event buffers.
 *
 * Threading:
 * - Internal state is game-thread owned.
 * - Cross-thread interactions should use `EnqueueTask(...)`.
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
     */
    Result Initialize();

    /**
     * @brief Initialize input system with explicit bootstrap settings.
     * @param Settings Input bootstrap settings.
     * @return Success or error.
     */
    Result Initialize(const InputBootstrapSettings& Settings);

    /**
     * @brief Shutdown active input context.
     * @remarks Safe to call repeatedly.
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
     */
    const InputBootstrapSettings& Settings() const;

    /**
     * @brief Access mutable runtime registry/runtime facade.
     * @return Mutable runtime reference.
     * @remarks
     * Advanced use only. Prefer `Initialize(...)` for standard startup flow.
     */
    SnAPI::Input::InputRuntime& Runtime();

    /**
     * @brief Access immutable runtime registry/runtime facade.
     * @return Immutable runtime reference.
     */
    const SnAPI::Input::InputRuntime& Runtime() const;

    /**
     * @brief Access active input context.
     * @return Context pointer or nullptr when uninitialized.
     */
    SnAPI::Input::InputContext* Context();

    /**
     * @brief Access active input context (const).
     * @return Context pointer or nullptr when uninitialized.
     */
    const SnAPI::Input::InputContext* Context() const;

    /**
     * @brief Access latest normalized snapshot.
     * @return Snapshot pointer or nullptr when uninitialized.
     */
    const SnAPI::Input::InputSnapshot* Snapshot() const;

    /**
     * @brief Access latest event stream.
     * @return Event vector pointer or nullptr when uninitialized.
     */
    const std::vector<SnAPI::Input::InputEvent>* Events() const;

    /**
     * @brief Access latest enumerated devices.
     * @return Device vector pointer or nullptr when uninitialized.
     */
    const std::vector<std::shared_ptr<SnAPI::Input::IInputDevice>>* Devices() const;

    /**
     * @brief Access mutable action map bound to active context.
     * @return Action map pointer or nullptr when uninitialized.
     */
    SnAPI::Input::ActionMap* Actions();

    /**
     * @brief Access immutable action map bound to active context.
     * @return Action map pointer or nullptr when uninitialized.
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
