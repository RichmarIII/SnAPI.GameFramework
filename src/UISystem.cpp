#include "UISystem.h"
#include "GameThreading.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "Profiling.h"

#include <cmath>
#include <utility>

namespace SnAPI::GameFramework
{

namespace
{

[[nodiscard]] bool IsPositiveFinite(const float Value)
{
    return std::isfinite(Value) && Value > 0.0f;
}

} // namespace

UISystem::~UISystem()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    Shutdown();
}

UISystem::UISystem(UISystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(Other.m_mutex);
    m_taskQueue = std::move(Other.m_taskQueue);
    m_settings = std::move(Other.m_settings);
    m_context = std::move(Other.m_context);
    m_initialized = Other.m_initialized;
    Other.m_initialized = false;
}

UISystem& UISystem::operator=(UISystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (this == &Other)
    {
        return *this;
    }

    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    ShutdownUnlocked();
    m_taskQueue = std::move(Other.m_taskQueue);
    m_settings = std::move(Other.m_settings);
    m_context = std::move(Other.m_context);
    m_initialized = Other.m_initialized;
    Other.m_initialized = false;
    return *this;
}

Result UISystem::Initialize()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    return Initialize(UIBootstrapSettings{});
}

Result UISystem::Initialize(const UIBootstrapSettings& SettingsValue)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);

    if (!IsPositiveFinite(SettingsValue.ViewportWidth) || !IsPositiveFinite(SettingsValue.ViewportHeight))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI viewport dimensions must be finite and greater than zero"));
    }
    if (SettingsValue.DpiScaleOverride && !IsPositiveFinite(*SettingsValue.DpiScaleOverride))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI DPI scale override must be finite and greater than zero"));
    }

    ShutdownUnlocked();
    m_settings = SettingsValue;

    auto Context = std::make_unique<SnAPI::UI::UIContext>();
    Context->EnsureDefaultSetup();
    Context->SetViewportSize(m_settings.ViewportWidth, m_settings.ViewportHeight);
    if (m_settings.DpiScaleOverride)
    {
        Context->SetDpiScale(*m_settings.DpiScaleOverride);
    }

    m_context = std::move(Context);
    m_initialized = true;
    return Ok();
}

void UISystem::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    ShutdownUnlocked();
}

bool UISystem::IsInitialized() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_initialized && static_cast<bool>(m_context);
}

void UISystem::Tick(const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return;
    }
    m_context->Tick(DeltaSeconds);
}

Result UISystem::BuildRenderPackets(SnAPI::UI::RenderPacketList& OutPackets)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    m_context->BuildRenderPackets(OutPackets);
    return Ok();
}

void UISystem::PushInput(const SnAPI::UI::PointerEvent& EventValue) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return;
    }
    m_context->PushInput(EventValue);
}

void UISystem::PushInput(const SnAPI::UI::KeyEvent& EventValue) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return;
    }
    m_context->PushInput(EventValue);
}

void UISystem::PushInput(const SnAPI::UI::TextInputEvent& EventValue) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return;
    }
    m_context->PushInput(EventValue);
}

void UISystem::PushInput(const SnAPI::UI::WheelEvent& EventValue) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return;
    }
    m_context->PushInput(EventValue);
}

Result UISystem::SetViewportSize(const float Width, const float Height)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (!IsPositiveFinite(Width) || !IsPositiveFinite(Height))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI viewport dimensions must be finite and greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    m_settings.ViewportWidth = Width;
    m_settings.ViewportHeight = Height;
    m_context->SetViewportSize(Width, Height);
    return Ok();
}

Result UISystem::SetDpiScale(const float Scale)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (!IsPositiveFinite(Scale))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI DPI scale must be finite and greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    m_settings.DpiScaleOverride = Scale;
    m_context->SetDpiScale(Scale);
    return Ok();
}

TaskHandle UISystem::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void UISystem::EnqueueThreadTask(std::function<void()> InTask)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void UISystem::ExecuteQueuedTasks()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    m_taskQueue.ExecuteQueuedTasks(*this, m_mutex);
}

const UIBootstrapSettings& UISystem::Settings() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_settings;
}

SnAPI::UI::UIContext* UISystem::Context()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_context.get();
}

const SnAPI::UI::UIContext* UISystem::Context() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_context.get();
}

void UISystem::ShutdownUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    m_context.reset();
    m_initialized = false;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
