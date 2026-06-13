#include "InputSystem.h"
#include "GameThreading.h"

#if defined(SNAPI_GF_ENABLE_INPUT)

#include "Profiling.h"

#include <memory>
#include <utility>

namespace SnAPI::GameFramework
{

InputSystem::~InputSystem()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    Shutdown();
}

InputSystem::InputSystem(InputSystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(Other.m_mutex);
    m_taskQueue = std::move(Other.m_taskQueue);
    m_settings = std::move(Other.m_settings);
    m_runtime = std::move(Other.m_runtime);
    m_context = std::move(Other.m_context);
    m_initialized = Other.m_initialized;
    if (!m_runtime)
    {
        m_runtime = std::make_unique<SnAPI::Input::InputRuntime>();
    }
    if (!Other.m_runtime)
    {
        Other.m_runtime = std::make_unique<SnAPI::Input::InputRuntime>();
    }
    Other.m_initialized = false;
}

InputSystem& InputSystem::operator=(InputSystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    if (this == &Other)
    {
        return *this;
    }

    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    ShutdownUnlocked();
    m_taskQueue = std::move(Other.m_taskQueue);
    m_settings = std::move(Other.m_settings);
    m_runtime = std::move(Other.m_runtime);
    m_context = std::move(Other.m_context);
    m_initialized = Other.m_initialized;
    if (!m_runtime)
    {
        m_runtime = std::make_unique<SnAPI::Input::InputRuntime>();
    }
    if (!Other.m_runtime)
    {
        Other.m_runtime = std::make_unique<SnAPI::Input::InputRuntime>();
    }
    Other.m_initialized = false;
    return *this;
}

Result InputSystem::Initialize()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    return Initialize(InputBootstrapSettings{});
}

Result InputSystem::Initialize(const InputBootstrapSettings& SettingsValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);

    ShutdownUnlocked();
    m_settings = SettingsValue;

    auto ValidateResult = ValidateBackendSelection(m_settings);
    if (!ValidateResult)
    {
        return ValidateResult;
    }

    auto RegisterResult = RegisterConfiguredBackends(m_settings);
    if (!RegisterResult)
    {
        return RegisterResult;
    }

    auto ContextResult = m_runtime->CreateContext(m_settings.Backend, m_settings.CreateDesc);
    if (!ContextResult)
    {
        return std::unexpected(MapInputError(ContextResult.error()));
    }

    m_context = std::move(*ContextResult);
    m_initialized = true;
    return Ok();
}

void InputSystem::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    ShutdownUnlocked();
}

bool InputSystem::IsInitialized() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_context && m_context->IsInitialized();
}

Result InputSystem::Pump()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_context)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Input system is not initialized"));
    }

    auto PumpResult = m_context->Pump();
    if (!PumpResult)
    {
        return std::unexpected(MapInputError(PumpResult.error()));
    }

    return Ok();
}

TaskHandle InputSystem::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void InputSystem::EnqueueThreadTask(std::function<void()> InTask)
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void InputSystem::ExecuteQueuedTasks()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    m_taskQueue.ExecuteQueuedTasks(*this, m_mutex);
}

const InputBootstrapSettings& InputSystem::Settings() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    return m_settings;
}

SnAPI::Input::InputRuntime& InputSystem::Runtime()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    return *m_runtime;
}

const SnAPI::Input::InputRuntime& InputSystem::Runtime() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    return *m_runtime;
}

SnAPI::Input::InputContext* InputSystem::Context()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    return m_context.get();
}

const SnAPI::Input::InputContext* InputSystem::Context() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    return m_context.get();
}

const SnAPI::Input::InputSnapshot* InputSystem::Snapshot() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    if (!m_context)
    {
        return nullptr;
    }
    return &m_context->Snapshot();
}

const std::vector<SnAPI::Input::InputEvent>* InputSystem::Events() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    if (!m_context)
    {
        return nullptr;
    }
    return &m_context->Events();
}

const std::vector<std::shared_ptr<SnAPI::Input::IInputDevice>>* InputSystem::Devices() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    if (!m_context)
    {
        return nullptr;
    }
    return &m_context->Devices();
}

SnAPI::Input::ActionMap* InputSystem::Actions()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    if (!m_context)
    {
        return nullptr;
    }
    return &m_context->Actions();
}

const SnAPI::Input::ActionMap* InputSystem::Actions() const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    GameLockGuard Lock(m_mutex);
    if (!m_context)
    {
        return nullptr;
    }
    return &m_context->Actions();
}

Error InputSystem::MapInputError(const SnAPI::Input::Error& ErrorValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    switch (ErrorValue.Code)
    {
    case SnAPI::Input::EErrorCode::InvalidArgument:
        return MakeError(EErrorCode::InvalidArgument, ErrorValue.Message);
    case SnAPI::Input::EErrorCode::NotFound:
        return MakeError(EErrorCode::NotFound, ErrorValue.Message);
    case SnAPI::Input::EErrorCode::AlreadyExists:
        return MakeError(EErrorCode::AlreadyExists, ErrorValue.Message);
    case SnAPI::Input::EErrorCode::NotSupported:
    case SnAPI::Input::EErrorCode::NotReady:
    case SnAPI::Input::EErrorCode::Timeout:
        return MakeError(EErrorCode::NotReady, ErrorValue.Message);
    case SnAPI::Input::EErrorCode::BackendError:
    case SnAPI::Input::EErrorCode::InternalError:
    case SnAPI::Input::EErrorCode::Ok:
    default:
        return MakeError(EErrorCode::InternalError, ErrorValue.Message);
    }
}

Result InputSystem::RegisterConfiguredBackends(const InputBootstrapSettings& SettingsValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Input");

#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
    if (SettingsValue.RegisterSdl3Backend)
    {
        auto RegisterResult = SnAPI::Input::RegisterSdl3Backend(m_runtime->Registry());
        if (!RegisterResult && RegisterResult.error().Code != SnAPI::Input::EErrorCode::AlreadyExists)
        {
            return std::unexpected(MapInputError(RegisterResult.error()));
        }
    }
#endif

#if defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) && SNAPI_INPUT_ENABLE_BACKEND_HIDAPI
    if (SettingsValue.RegisterHidApiBackend)
    {
        auto RegisterResult = SnAPI::Input::RegisterHidApiBackend(m_runtime->Registry());
        if (!RegisterResult && RegisterResult.error().Code != SnAPI::Input::EErrorCode::AlreadyExists)
        {
            return std::unexpected(MapInputError(RegisterResult.error()));
        }
    }
#endif

#if defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) && SNAPI_INPUT_ENABLE_BACKEND_LIBUSB
    if (SettingsValue.RegisterLibUsbBackend)
    {
        auto RegisterResult = SnAPI::Input::RegisterLibUsbBackend(m_runtime->Registry());
        if (!RegisterResult && RegisterResult.error().Code != SnAPI::Input::EErrorCode::AlreadyExists)
        {
            return std::unexpected(MapInputError(RegisterResult.error()));
        }
    }
#endif

    return Ok();
}

Result InputSystem::ValidateBackendSelection(const InputBootstrapSettings& SettingsValue) const
{
    SNAPI_GF_PROFILE_FUNCTION("Input");

    switch (SettingsValue.Backend)
    {
    case SnAPI::Input::EInputBackend::SDL3:
#if !defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) || !SNAPI_INPUT_ENABLE_BACKEND_SDL3
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "SDL3 input backend is compiled out"));
#endif
        break;
    case SnAPI::Input::EInputBackend::HIDAPI:
#if !defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) || !SNAPI_INPUT_ENABLE_BACKEND_HIDAPI
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "HIDAPI input backend is compiled out"));
#endif
        break;
    case SnAPI::Input::EInputBackend::LIBUSB:
#if !defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) || !SNAPI_INPUT_ENABLE_BACKEND_LIBUSB
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "libusb input backend is compiled out"));
#endif
        break;
    case SnAPI::Input::EInputBackend::Invalid:
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Input backend must not be Invalid"));
    default:
        break;
    }

    return Ok();
}

void InputSystem::ShutdownUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("Input");
    if (m_context)
    {
        m_context->Shutdown();
        m_context.reset();
    }
    m_initialized = false;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT
