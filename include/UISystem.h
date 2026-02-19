#pragma once

#if defined(SNAPI_GF_ENABLE_UI)

#include <functional>
#include <memory>
#include <optional>

#include "Expected.h"
#include "GameThreading.h"

#include <UIContext.h>

namespace SnAPI::GameFramework
{

/**
 * @brief Bootstrap settings for world-owned SnAPI.UI integration.
 * @remarks
 * These settings are applied when `UISystem::Initialize(...)` creates the
 * world UI context. They define baseline viewport sizing and optional DPI
 * override behavior.
 */
struct UIBootstrapSettings
{
    float ViewportWidth = 1600.0f; /**< @brief Initial logical viewport width in UI units; must be finite and > 0. */
    float ViewportHeight = 900.0f; /**< @brief Initial logical viewport height in UI units; must be finite and > 0. */
    std::optional<float> DpiScaleOverride{}; /**< @brief Optional explicit DPI scale override; when nullopt, UIContext defaults/environment values are preserved. */
};

/**
 * @brief World-owned adapter over `SnAPI::UI::UIContext`.
 * @remarks
 * `UISystem` provides:
 * - explicit initialize/shutdown lifecycle for one world-scoped UI context,
 * - per-frame `Tick(...)` execution for input processing and async UI tasks,
 * - typed UI input forwarding (`PointerEvent`, `KeyEvent`, `TextInputEvent`),
 * - render packet extraction for renderer integration layers.
 *
 * Threading:
 * - internal state is game-thread owned (`GameMutex` affinity guard),
 * - cross-thread interaction should use `EnqueueTask(...)`.
 */
class UISystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(UISystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;

    /** @brief Construct an uninitialized UI system. */
    UISystem() = default;
    /** @brief Destructor; shuts down active UI context if initialized. */
    ~UISystem() override;

    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;

    /**
     * @brief Move constructor; transfers UI context ownership.
     */
    UISystem(UISystem&& Other) noexcept;
    /**
     * @brief Move assignment; transfers UI context ownership safely.
     */
    UISystem& operator=(UISystem&& Other) noexcept;

    /**
     * @brief Initialize UI system with default bootstrap settings.
     * @return Success or error.
     */
    Result Initialize();

    /**
     * @brief Initialize UI system with explicit bootstrap settings.
     * @param SettingsValue UI bootstrap settings snapshot.
     * @return Success or error.
     * @remarks Reinitializes existing context when already initialized.
     */
    Result Initialize(const UIBootstrapSettings& SettingsValue);

    /**
     * @brief Shutdown active UI context.
     * @remarks Safe to call repeatedly.
     */
    void Shutdown();

    /**
     * @brief Check whether a UI context is initialized and ready.
     * @return True when initialized.
     */
    bool IsInitialized() const;

    /**
     * @brief Tick the UI context for the current frame.
     * @param DeltaSeconds Time since last frame.
     * @remarks
     * Executes queued UI tasks then advances `UIContext::Tick(...)`.
     * Calling this when uninitialized is a no-op.
     */
    void Tick(float DeltaSeconds);

    /**
     * @brief Build a frame render packet list from current UI tree state.
     * @param OutPackets Output packet list populated by `UIContext`.
     * @return Success or error.
     */
    Result BuildRenderPackets(SnAPI::UI::RenderPacketList& OutPackets);

    /**
     * @brief Forward one pointer input event to the active UI context.
     * @param EventValue Pointer event payload.
     */
    void PushInput(const SnAPI::UI::PointerEvent& EventValue) const;

    /**
     * @brief Forward one key input event to the active UI context.
     * @param EventValue Key event payload.
     */
    void PushInput(const SnAPI::UI::KeyEvent& EventValue) const;

    /**
     * @brief Forward one text input event to the active UI context.
     * @param EventValue Text input event payload.
     */
    void PushInput(const SnAPI::UI::TextInputEvent& EventValue) const;

    /**
    * @brief Forward one Wheel input event to the active UI context.
    * @param EventValue Wheel input event payload.
    */
    void PushInput(const SnAPI::UI::WheelEvent& EventValue) const;

    /**
     * @brief Update logical UI viewport size.
     * @param Width Viewport width in UI units.
     * @param Height Viewport height in UI units.
     * @return Success or error.
     */
    Result SetViewportSize(float Width, float Height);

    /**
     * @brief Override UI DPI scale.
     * @param Scale DPI scale where 1.0 corresponds to 96 DPI.
     * @return Success or error.
     */
    Result SetDpiScale(float Scale);

    /**
     * @brief Enqueue work on the UI system thread.
     * @param InTask Work callback executed on UI-thread affinity.
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
     * @brief Execute all queued tasks on the UI thread.
     */
    void ExecuteQueuedTasks();

    /**
     * @brief Access active bootstrap settings snapshot.
     * @return Settings currently used by this subsystem.
     */
    const UIBootstrapSettings& Settings() const;

    /**
     * @brief Access active UI context.
     * @return Context pointer or nullptr when uninitialized.
     */
    SnAPI::UI::UIContext* Context();

    /**
     * @brief Access active UI context (const).
     * @return Context pointer or nullptr when uninitialized.
     */
    const SnAPI::UI::UIContext* Context() const;

    /**
     * @brief Register an external UI element type into the active UI context.
     * @tparam TElement Element type deriving from `SnAPI::UI::IUIElement`.
     * @param ThemeTypeHash Optional theme style type hash (defaults to element type hash).
     * @return Success or error when UI is not initialized.
     */
    template<typename TElement>
    Result RegisterElementType(uint32_t ThemeTypeHash = SnAPI::UI::TypeHash<TElement>())
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || !m_context)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
        }

        m_context->template RegisterElementType<TElement>(ThemeTypeHash);
        return Ok();
    }

    /**
     * @brief Unregister an external UI element type from the active UI context.
     * @tparam TElement Element type deriving from `SnAPI::UI::IUIElement`.
     * @return Success or error when UI is not initialized.
     */
    template<typename TElement>
    Result UnregisterElementType()
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || !m_context)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
        }

        m_context->template UnregisterElementType<TElement>();
        return Ok();
    }

    /**
     * @brief Check whether an element type is registered in the active UI context.
     * @tparam TElement Element type deriving from `SnAPI::UI::IUIElement`.
     * @return True when registered and UI is initialized.
     */
    template<typename TElement>
    bool IsElementTypeRegistered() const
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || !m_context)
        {
            return false;
        }

        return m_context->template IsElementTypeRegistered<TElement>();
    }

private:
    void ShutdownUnlocked();

    mutable GameMutex m_mutex{}; /**< @brief UI-system thread affinity guard. */
    TSystemTaskQueue<UISystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    UIBootstrapSettings m_settings{}; /**< @brief Active UI bootstrap settings snapshot. */
    std::unique_ptr<SnAPI::UI::UIContext> m_context{}; /**< @brief Active UI context instance. */
    bool m_initialized = false; /**< @brief True when context has been initialized and can be ticked. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
