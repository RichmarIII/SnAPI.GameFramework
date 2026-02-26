#include "GameRuntime.h"

#include "Assert.h"
#include "Profiling.h"
#include "TypeRegistration.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
#include <SnAPI/Profiler/Profiler.h>

#include <cerrno>
#include <cstdlib>
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <VulkanGraphicsAPI.hpp>
#include <WindowBase.hpp>
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER) && __has_include(<SDL3/SDL.h>)
#include <SDL3/SDL.h>
#define SNAPI_GF_RUNTIME_HAS_SDL3 1
#else
#define SNAPI_GF_RUNTIME_HAS_SDL3 0
#endif

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
namespace
{

[[nodiscard]] bool EqualsIgnoreCase(const std::string_view Left, const std::string_view Right)
{
    if (Left.size() != Right.size())
    {
        return false;
    }

    for (std::size_t Index = 0; Index < Left.size(); ++Index)
    {
        const unsigned char LeftChar = static_cast<unsigned char>(Left[Index]);
        const unsigned char RightChar = static_cast<unsigned char>(Right[Index]);

        const unsigned char LeftLower =
            (LeftChar >= 'A' && LeftChar <= 'Z') ? static_cast<unsigned char>(LeftChar + ('a' - 'A')) : LeftChar;
        const unsigned char RightLower =
            (RightChar >= 'A' && RightChar <= 'Z') ? static_cast<unsigned char>(RightChar + ('a' - 'A')) : RightChar;

        if (LeftLower != RightLower)
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] std::optional<bool> ParseBooleanEnvValue(const char* Value)
{
    if (Value == nullptr || Value[0] == '\0')
    {
        return std::nullopt;
    }

    const std::string_view Raw(Value);
    if (EqualsIgnoreCase(Raw, "1") || EqualsIgnoreCase(Raw, "true") || EqualsIgnoreCase(Raw, "yes") ||
        EqualsIgnoreCase(Raw, "on") || EqualsIgnoreCase(Raw, "enabled"))
    {
        return true;
    }
    if (EqualsIgnoreCase(Raw, "0") || EqualsIgnoreCase(Raw, "false") || EqualsIgnoreCase(Raw, "no") ||
        EqualsIgnoreCase(Raw, "off") || EqualsIgnoreCase(Raw, "disabled"))
    {
        return false;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::uint64_t> ParseUnsignedEnv(const char* Name)
{
    const char* Raw = std::getenv(Name);
    if (Raw == nullptr || Raw[0] == '\0')
    {
        return std::nullopt;
    }

    errno = 0;
    char* End = nullptr;
    const unsigned long long Parsed = std::strtoull(Raw, &End, 10);
    if (errno != 0 || End == Raw || End == nullptr || End[0] != '\0')
    {
        return std::nullopt;
    }

    return static_cast<std::uint64_t>(Parsed);
}

void ConfigureRealtimeProfilerStreamForStartup()
{
    SnAPI::Profiler::ProfilerConfig ProfilerConfig =
        ::SnAPI::Profiler::Profiler::Get().GetConfig();
    bool ReconfigureProfiler = false;

    if (const std::optional<std::uint64_t> Capacity = ParseUnsignedEnv("SNAPI_GF_PROFILER_EVENT_BUFFER_CAPACITY"))
    {
        const std::uint64_t Clamped = std::max<std::uint64_t>(*Capacity, 2);
        ProfilerConfig.PerThreadEventBufferCapacity = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(Clamped, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
        ReconfigureProfiler = true;
    }

    if (const std::optional<std::uint64_t> Depth = ParseUnsignedEnv("SNAPI_GF_PROFILER_MAX_SCOPE_DEPTH"))
    {
        const std::uint64_t Clamped = std::max<std::uint64_t>(*Depth, 1);
        ProfilerConfig.MaxScopeDepth = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(Clamped, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
        ReconfigureProfiler = true;
    }

    if (const std::optional<std::uint64_t> History = ParseUnsignedEnv("SNAPI_GF_PROFILER_HISTORY_CAPACITY"))
    {
        ProfilerConfig.FrameHistoryCapacity = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(*History, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
        ReconfigureProfiler = true;
    }

    if (const std::optional<std::uint64_t> MaxEvents = ParseUnsignedEnv("SNAPI_GF_PROFILER_MAX_EVENTS_PER_THREAD_PER_FRAME"))
    {
        ProfilerConfig.MaxEventsPerThreadPerFrame = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(MaxEvents.value(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
        ReconfigureProfiler = true;
    }

    if (const std::optional<bool> PreserveOverflow =
            ParseBooleanEnvValue(std::getenv("SNAPI_GF_PROFILER_PRESERVE_OVERFLOW_EVENTS")))
    {
        ProfilerConfig.PreserveOverflowEvents = *PreserveOverflow;
        ReconfigureProfiler = true;
    }

    if (ReconfigureProfiler)
    {
        ::SnAPI::Profiler::Profiler::Get().Configure(ProfilerConfig);
    }

    SnAPI::Profiler::RawTraceConfig RawTraceConfig =
        ::SnAPI::Profiler::Profiler::Get().GetRawTraceConfig();
    bool ReconfigureRawTrace = false;

    if (const char* RawMode = std::getenv("SNAPI_GF_PROFILER_TRACE_MODE"); RawMode != nullptr && RawMode[0] != '\0')
    {
        const std::string_view ModeValue(RawMode);
        if (EqualsIgnoreCase(ModeValue, "record") || EqualsIgnoreCase(ModeValue, "capture") ||
            EqualsIgnoreCase(ModeValue, "on") || EqualsIgnoreCase(ModeValue, "enabled"))
        {
            RawTraceConfig.Mode = SnAPI::Profiler::RawTraceMode::Record;
            ReconfigureRawTrace = true;
        }
        else if (EqualsIgnoreCase(ModeValue, "off") || EqualsIgnoreCase(ModeValue, "disabled") ||
                 EqualsIgnoreCase(ModeValue, "none"))
        {
            RawTraceConfig.Mode = SnAPI::Profiler::RawTraceMode::Disabled;
            ReconfigureRawTrace = true;
        }
    }

    if (const char* RawPath = std::getenv("SNAPI_GF_PROFILER_TRACE_PATH"); RawPath != nullptr && RawPath[0] != '\0')
    {
        RawTraceConfig.Path = RawPath;
        ReconfigureRawTrace = true;
    }

    if (const std::optional<bool> CaptureOnly =
            ParseBooleanEnvValue(std::getenv("SNAPI_GF_PROFILER_TRACE_CAPTURE_ONLY")))
    {
        RawTraceConfig.CaptureOnly = *CaptureOnly;
        ReconfigureRawTrace = true;
    }

    if (ReconfigureRawTrace)
    {
        ::SnAPI::Profiler::Profiler::Get().ConfigureRawTrace(RawTraceConfig);
    }

    SnAPI::Profiler::RealtimeStreamConfig StreamConfig =
        ::SnAPI::Profiler::Profiler::Get().GetRealtimeStreamConfig();

    bool EnableByDefault = false;
#if !defined(NDEBUG)
    EnableByDefault = true;
#endif

    bool EnableStream = EnableByDefault;
    if (const std::optional<bool> ParsedEnable = ParseBooleanEnvValue(std::getenv("SNAPI_GF_PROFILER_STREAM_ENABLE")))
    {
        EnableStream = *ParsedEnable;
    }
    if (RawTraceConfig.Mode == SnAPI::Profiler::RawTraceMode::Record && RawTraceConfig.CaptureOnly)
    {
        EnableStream = false;
    }
    StreamConfig.Enabled = EnableStream;

    if (const char* Host = std::getenv("SNAPI_GF_PROFILER_STREAM_HOST"); Host != nullptr && Host[0] != '\0')
    {
        StreamConfig.Host = Host;
    }

    if (const std::optional<std::uint64_t> Port = ParseUnsignedEnv("SNAPI_GF_PROFILER_STREAM_PORT"))
    {
        if (*Port > 0 && *Port <= static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            StreamConfig.Port = static_cast<std::uint16_t>(*Port);
        }
    }

    const auto ApplyOptionalSize = [&](const char* Name, std::size_t& Target)
    {
        if (const std::optional<std::uint64_t> Value = ParseUnsignedEnv(Name))
        {
            const std::uint64_t Clamped = std::max<std::uint64_t>(*Value, 1);
            Target = static_cast<std::size_t>(std::min<std::uint64_t>(
                Clamped, static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
        }
    };

    ApplyOptionalSize("SNAPI_GF_PROFILER_STREAM_MAX_SCOPES", StreamConfig.MaxScopes);
    ApplyOptionalSize("SNAPI_GF_PROFILER_STREAM_MAX_RELATIONSHIPS", StreamConfig.MaxRelationships);
    ApplyOptionalSize("SNAPI_GF_PROFILER_STREAM_MAX_CATEGORIES", StreamConfig.MaxCategories);
    ApplyOptionalSize("SNAPI_GF_PROFILER_STREAM_MAX_THREADS", StreamConfig.MaxThreads);

    if (const std::optional<bool> SendFull =
            ParseBooleanEnvValue(std::getenv("SNAPI_GF_PROFILER_STREAM_SEND_FULL")))
    {
        StreamConfig.SendFullSnapshot = *SendFull;
    }

    if (const std::optional<std::uint64_t> MaxPayload = ParseUnsignedEnv("SNAPI_GF_PROFILER_STREAM_MAX_UDP_PAYLOAD_BYTES"))
    {
        StreamConfig.MaxUdpPayloadBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
            std::max<std::uint64_t>(*MaxPayload, 1200),
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
    }

    if (const std::optional<bool> EnableChunking =
            ParseBooleanEnvValue(std::getenv("SNAPI_GF_PROFILER_STREAM_ENABLE_CHUNKING")))
    {
        StreamConfig.EnablePayloadChunking = *EnableChunking;
    }

    if (const std::optional<std::uint64_t> ChunkPayload = ParseUnsignedEnv("SNAPI_GF_PROFILER_STREAM_CHUNK_PAYLOAD_BYTES"))
    {
        StreamConfig.ChunkPayloadBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
            std::max<std::uint64_t>(*ChunkPayload, 512),
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
    }

    ::SnAPI::Profiler::Profiler::Get().ConfigureRealtimeStream(StreamConfig);
}

} // namespace
#endif

namespace
{
constexpr auto kFrameSleepMargin = std::chrono::microseconds(1500);
constexpr auto kFrameYieldMargin = std::chrono::microseconds(200);

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
[[nodiscard]] uint32_t MapUiKeyCode(const SnAPI::Input::EKey Key)
{
    switch (Key)
    {
    case SnAPI::Input::EKey::Backspace:
        return 8u;
    case SnAPI::Input::EKey::Tab:
        return 9u;
    case SnAPI::Input::EKey::Enter:
    case SnAPI::Input::EKey::NumpadEnter:
        return 13u;
    case SnAPI::Input::EKey::Escape:
        return 27u;
    case SnAPI::Input::EKey::Space:
        return static_cast<uint32_t>(' ');
    case SnAPI::Input::EKey::A:
        return static_cast<uint32_t>('a');
    case SnAPI::Input::EKey::B:
        return static_cast<uint32_t>('b');
    case SnAPI::Input::EKey::C:
        return static_cast<uint32_t>('c');
    case SnAPI::Input::EKey::D:
        return static_cast<uint32_t>('d');
    case SnAPI::Input::EKey::E:
        return static_cast<uint32_t>('e');
    case SnAPI::Input::EKey::F:
        return static_cast<uint32_t>('f');
    case SnAPI::Input::EKey::G:
        return static_cast<uint32_t>('g');
    case SnAPI::Input::EKey::H:
        return static_cast<uint32_t>('h');
    case SnAPI::Input::EKey::I:
        return static_cast<uint32_t>('i');
    case SnAPI::Input::EKey::J:
        return static_cast<uint32_t>('j');
    case SnAPI::Input::EKey::K:
        return static_cast<uint32_t>('k');
    case SnAPI::Input::EKey::L:
        return static_cast<uint32_t>('l');
    case SnAPI::Input::EKey::M:
        return static_cast<uint32_t>('m');
    case SnAPI::Input::EKey::N:
        return static_cast<uint32_t>('n');
    case SnAPI::Input::EKey::O:
        return static_cast<uint32_t>('o');
    case SnAPI::Input::EKey::P:
        return static_cast<uint32_t>('p');
    case SnAPI::Input::EKey::Q:
        return static_cast<uint32_t>('q');
    case SnAPI::Input::EKey::R:
        return static_cast<uint32_t>('r');
    case SnAPI::Input::EKey::S:
        return static_cast<uint32_t>('s');
    case SnAPI::Input::EKey::T:
        return static_cast<uint32_t>('t');
    case SnAPI::Input::EKey::U:
        return static_cast<uint32_t>('u');
    case SnAPI::Input::EKey::V:
        return static_cast<uint32_t>('v');
    case SnAPI::Input::EKey::W:
        return static_cast<uint32_t>('w');
    case SnAPI::Input::EKey::X:
        return static_cast<uint32_t>('x');
    case SnAPI::Input::EKey::Y:
        return static_cast<uint32_t>('y');
    case SnAPI::Input::EKey::Z:
        return static_cast<uint32_t>('z');
    case SnAPI::Input::EKey::Num0:
    case SnAPI::Input::EKey::Numpad0:
        return static_cast<uint32_t>('0');
    case SnAPI::Input::EKey::Num1:
    case SnAPI::Input::EKey::Numpad1:
        return static_cast<uint32_t>('1');
    case SnAPI::Input::EKey::Num2:
    case SnAPI::Input::EKey::Numpad2:
        return static_cast<uint32_t>('2');
    case SnAPI::Input::EKey::Num3:
    case SnAPI::Input::EKey::Numpad3:
        return static_cast<uint32_t>('3');
    case SnAPI::Input::EKey::Num4:
    case SnAPI::Input::EKey::Numpad4:
        return static_cast<uint32_t>('4');
    case SnAPI::Input::EKey::Num5:
    case SnAPI::Input::EKey::Numpad5:
        return static_cast<uint32_t>('5');
    case SnAPI::Input::EKey::Num6:
    case SnAPI::Input::EKey::Numpad6:
        return static_cast<uint32_t>('6');
    case SnAPI::Input::EKey::Num7:
    case SnAPI::Input::EKey::Numpad7:
        return static_cast<uint32_t>('7');
    case SnAPI::Input::EKey::Num8:
    case SnAPI::Input::EKey::Numpad8:
        return static_cast<uint32_t>('8');
    case SnAPI::Input::EKey::Num9:
    case SnAPI::Input::EKey::Numpad9:
        return static_cast<uint32_t>('9');
    case SnAPI::Input::EKey::Period:
    case SnAPI::Input::EKey::NumpadPeriod:
        return static_cast<uint32_t>('.');
    case SnAPI::Input::EKey::Minus:
    case SnAPI::Input::EKey::NumpadMinus:
        return static_cast<uint32_t>('-');
    case SnAPI::Input::EKey::Equals:
        return static_cast<uint32_t>('=');
    case SnAPI::Input::EKey::LeftBracket:
        return static_cast<uint32_t>('[');
    case SnAPI::Input::EKey::RightBracket:
        return static_cast<uint32_t>(']');
    case SnAPI::Input::EKey::Backslash:
        return static_cast<uint32_t>('\\');
    case SnAPI::Input::EKey::Semicolon:
        return static_cast<uint32_t>(';');
    case SnAPI::Input::EKey::Apostrophe:
        return static_cast<uint32_t>('\'');
    case SnAPI::Input::EKey::Grave:
        return static_cast<uint32_t>('`');
    case SnAPI::Input::EKey::Comma:
        return static_cast<uint32_t>(',');
    case SnAPI::Input::EKey::Slash:
        return static_cast<uint32_t>('/');
    case SnAPI::Input::EKey::Delete:
        return 127u;
    case SnAPI::Input::EKey::Left:
        return 1073741904u;
    case SnAPI::Input::EKey::Right:
        return 1073741903u;
    case SnAPI::Input::EKey::Up:
        return 1073741906u;
    case SnAPI::Input::EKey::Down:
        return 1073741905u;
    case SnAPI::Input::EKey::Home:
        return 1073741898u;
    case SnAPI::Input::EKey::End:
        return 1073741901u;
    case SnAPI::Input::EKey::PageUp:
        return 1073741899u;
    case SnAPI::Input::EKey::PageDown:
        return 1073741902u;
    default:
        return static_cast<uint32_t>(Key);
    }
}

void PushUtf8CodepointsToUi(const std::string& Text, SnAPI::GameFramework::UISystem& UiSystem)
{
    size_t Index = 0;
    while (Index < Text.size())
    {
        const unsigned char C0 = static_cast<unsigned char>(Text[Index]);
        uint32_t Codepoint = C0;
        size_t Advance = 1;

        if ((C0 & 0xE0u) == 0xC0u && Index + 1 < Text.size())
        {
            const unsigned char C1 = static_cast<unsigned char>(Text[Index + 1]);
            if ((C1 & 0xC0u) == 0x80u)
            {
                Codepoint = ((C0 & 0x1Fu) << 6u) | (C1 & 0x3Fu);
                Advance = 2;
            }
        }
        else if ((C0 & 0xF0u) == 0xE0u && Index + 2 < Text.size())
        {
            const unsigned char C1 = static_cast<unsigned char>(Text[Index + 1]);
            const unsigned char C2 = static_cast<unsigned char>(Text[Index + 2]);
            if ((C1 & 0xC0u) == 0x80u && (C2 & 0xC0u) == 0x80u)
            {
                Codepoint = ((C0 & 0x0Fu) << 12u) | ((C1 & 0x3Fu) << 6u) | (C2 & 0x3Fu);
                Advance = 3;
            }
        }
        else if ((C0 & 0xF8u) == 0xF0u && Index + 3 < Text.size())
        {
            const unsigned char C1 = static_cast<unsigned char>(Text[Index + 1]);
            const unsigned char C2 = static_cast<unsigned char>(Text[Index + 2]);
            const unsigned char C3 = static_cast<unsigned char>(Text[Index + 3]);
            if ((C1 & 0xC0u) == 0x80u && (C2 & 0xC0u) == 0x80u && (C3 & 0xC0u) == 0x80u)
            {
                Codepoint = ((C0 & 0x07u) << 18u) | ((C1 & 0x3Fu) << 12u) | ((C2 & 0x3Fu) << 6u) | (C3 & 0x3Fu);
                Advance = 4;
            }
        }

        SnAPI::UI::TextInputEvent UiText{};
        UiText.Codepoint = Codepoint;
        UiSystem.PushInput(UiText);
        Index += Advance;
    }
}

struct UiViewportTransform
{
    float OutputX = 0.0f;
    float OutputY = 0.0f;
    float OutputWidth = 0.0f;
    float OutputHeight = 0.0f;
    float UiWidth = 0.0f;
    float UiHeight = 0.0f;

    [[nodiscard]] bool IsValid() const
    {
        return std::isfinite(OutputX) && std::isfinite(OutputY) && std::isfinite(OutputWidth) &&
               std::isfinite(OutputHeight) && std::isfinite(UiWidth) && std::isfinite(UiHeight) &&
               OutputWidth > 0.0f && OutputHeight > 0.0f && UiWidth > 0.0f && UiHeight > 0.0f;
    }

    [[nodiscard]] std::pair<float, float> MapWindowPointToUi(const float WindowX, const float WindowY) const
    {
        if (!IsValid())
        {
            return {WindowX, WindowY};
        }

        const float LocalX = (WindowX - OutputX) * (UiWidth / OutputWidth);
        const float LocalY = (WindowY - OutputY) * (UiHeight / OutputHeight);
        return {LocalX, LocalY};
    }
};
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER) && SNAPI_GF_RUNTIME_HAS_SDL3
[[nodiscard]] float QueryWindowDisplayScale(const SnAPI::Graphics::WindowBase* Window)
{
    if (!Window)
    {
        return 0.0f;
    }

    auto* NativeWindow = reinterpret_cast<SDL_Window*>(Window->Handle());
    if (!NativeWindow)
    {
        return 0.0f;
    }

    return SDL_GetWindowDisplayScale(NativeWindow);
}
#endif
} // namespace

Result GameRuntime::Init(const GameRuntimeSettings& Settings)
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    Shutdown();

    m_settings = Settings;
    m_fixedAccumulator = 0.0f;
    m_framePacerStep = FrameClock::duration::zero();
    m_nextFrameDeadline = FrameClock::time_point{};
    m_framePacerArmed = false;
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
    m_uiLeftDown = false;
    m_uiRightDown = false;
    m_uiMiddleDown = false;
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(SNAPI_GF_ENABLE_UI)
    m_uiDpiScaleCache = 0.0f;
#endif
    SNAPI_GF_PROFILE_SET_THREAD_NAME("GameThread");
#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
    ConfigureRealtimeProfilerStreamForStartup();
#endif

    if (m_settings.RegisterBuiltins)
    {
        EnsureBuiltinTypesRegistered();
    }

    std::string WorldName = m_settings.WorldName;
    if (WorldName.empty())
    {
        WorldName = "World";
    }
    if (m_settings.WorldFactory)
    {
        m_world = m_settings.WorldFactory(WorldName);
    }
    else
    {
        m_world = std::make_unique<class World>(std::move(WorldName));
    }
    if (!m_world)
    {
        Shutdown();
        return std::unexpected(MakeError(EErrorCode::NotReady, "Failed to create world instance"));
    }
    m_world->SetGameplayHost(nullptr);
    m_world->SetFixedTickFrameState(false, 0.0f, 1.0f);

#if defined(SNAPI_GF_ENABLE_INPUT)
    if (m_settings.Input)
    {
        auto InitInput = m_world->Input().Initialize(*m_settings.Input);
        if (!InitInput)
        {
            Shutdown();
            return std::unexpected(InitInput.error());
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    if (m_settings.UI)
    {
        auto InitUi = m_world->UI().Initialize(*m_settings.UI);
        if (!InitUi)
        {
            Shutdown();
            return std::unexpected(InitUi.error());
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (m_settings.Physics)
    {
        auto InitPhysics = m_world->Physics().Initialize(*m_settings.Physics);
        if (!InitPhysics)
        {
            Shutdown();
            return std::unexpected(InitPhysics.error());
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_settings.Networking)
    {
        auto InitNetwork = m_world->Networking().InitializeOwnedSession(*m_settings.Networking);
        if (!InitNetwork)
        {
            Shutdown();
            return std::unexpected(InitNetwork.error());
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (m_settings.Renderer)
    {
        if (!m_world->Renderer().Initialize(*m_settings.Renderer))
        {
            Shutdown();
            return std::unexpected(MakeError(EErrorCode::NotReady, "Failed to initialize renderer system"));
        }
    }
#endif

    if (m_settings.Gameplay && m_world->ShouldRunGameplay())
    {
        auto InitGameplay = StartGameplayHost();
        if (!InitGameplay)
        {
            Shutdown();
            return std::unexpected(InitGameplay.error());
        }
    }

    return Ok();
}

void GameRuntime::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    StopGameplayHost();

    if (m_world)
    {
        // Tear down world nodes/components while subsystems are still alive.
        // This ensures render objects/material instances release GPU resources
        // before renderer/device shutdown.
        m_world->Clear();
    }
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        m_world->Networking().ShutdownOwnedSession();
    }
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (m_world)
    {
        m_world->Physics().Shutdown();
    }
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (m_world)
    {
        m_world->Renderer().Shutdown();
    }
#endif
#if defined(SNAPI_GF_ENABLE_UI)
    if (m_world)
    {
        m_world->UI().Shutdown();
    }
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
    if (m_world)
    {
        m_world->Input().Shutdown();
    }
#endif
    m_world.reset();
    m_fixedAccumulator = 0.0f;
    m_framePacerStep = FrameClock::duration::zero();
    m_nextFrameDeadline = FrameClock::time_point{};
    m_framePacerArmed = false;
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
    m_uiLeftDown = false;
    m_uiRightDown = false;
    m_uiMiddleDown = false;
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(SNAPI_GF_ENABLE_UI)
    m_uiDpiScaleCache = 0.0f;
#endif
}

bool GameRuntime::IsInitialized() const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return static_cast<bool>(m_world);
}

bool GameRuntime::Update(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (!m_world)
    {
        return false;
    }

    const auto FrameStart = FrameClock::now();
    bool KeepRunning = true;

    SNAPI_GF_PROFILE_BEGIN_FRAME_AUTO();
    {
        SNAPI_GF_PROFILE_SCOPE("Frame.Update", "Runtime");

        if (m_gameplayHost && m_gameplayHost->IsInitialized() && m_world->ShouldRunGameplay())
        {
            SNAPI_GF_PROFILE_SCOPE("Frame.Gameplay", "Runtime");
            m_gameplayHost->Tick(DeltaSeconds);
        }

        const auto& TickSettings = m_settings.Tick;
        const bool FixedTickEnabled = TickSettings.EnableFixedTick && TickSettings.FixedDeltaSeconds > 0.0f;
        if (FixedTickEnabled)
        {
            SNAPI_GF_PROFILE_SCOPE("Frame.FixedTickLoop", "Runtime");
            {
                SNAPI_GF_PROFILE_SCOPE("Frame.FixedTick.Accumulate", "Runtime");
                m_fixedAccumulator += std::max(0.0f, DeltaSeconds);
            }
            const std::size_t MaxSteps = std::max<std::size_t>(1, TickSettings.MaxFixedStepsPerUpdate);

            std::size_t Steps = 0;
            while (m_fixedAccumulator >= TickSettings.FixedDeltaSeconds && Steps < MaxSteps)
            {
                SNAPI_GF_PROFILE_SCOPE("Frame.FixedTickStep", "Runtime");
                m_world->FixedTick(TickSettings.FixedDeltaSeconds);
                m_fixedAccumulator -= TickSettings.FixedDeltaSeconds;
                ++Steps;
            }

            if (Steps == MaxSteps && m_fixedAccumulator >= TickSettings.FixedDeltaSeconds)
            {
                // Keep a bounded backlog so simulation can catch up after spikes without silently losing time.
                {
                    SNAPI_GF_PROFILE_SCOPE("Frame.FixedTick.BacklogClamp", "Runtime");
                    const float MaxCarryAccumulator = TickSettings.FixedDeltaSeconds * static_cast<float>(MaxSteps);
                    if (m_fixedAccumulator > MaxCarryAccumulator)
                    {
                        m_fixedAccumulator = MaxCarryAccumulator;
                    }
                }
            }
        }
        else
        {
            m_fixedAccumulator = 0.0f;
        }

        const float FixedAlpha =
            FixedTickEnabled ? std::clamp(m_fixedAccumulator / TickSettings.FixedDeltaSeconds, 0.0f, 1.0f) : 1.0f;
        m_world->SetFixedTickFrameState(FixedTickEnabled,
                                        FixedTickEnabled ? TickSettings.FixedDeltaSeconds : 0.0f,
                                        FixedAlpha);

        {
            SNAPI_GF_PROFILE_SCOPE("Frame.VariableTick", "Runtime");
            m_world->Tick(DeltaSeconds);
        }

        if (TickSettings.EnableLateTick)
        {
            SNAPI_GF_PROFILE_SCOPE("Frame.LateTick", "Runtime");
            m_world->LateTick(DeltaSeconds);
        }

        if (TickSettings.EnableEndFrame)
        {
            SNAPI_GF_PROFILE_SCOPE("Frame.EndFrame", "Runtime");
            m_world->EndFrame();
        }

#if defined(SNAPI_GF_ENABLE_RENDERER) || (defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI))
        {
            SNAPI_GF_PROFILE_SCOPE("Frame.PlatformInput", "Runtime");
            KeepRunning = ProcessPlatformAndUiInput() && KeepRunning;
        }
#endif

        {
            SNAPI_GF_PROFILE_SCOPE("Frame.Pacing", "Runtime");
            ApplyFramePacing(FrameStart);
        }
    }

    SNAPI_GF_PROFILE_END_FRAME();

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (m_settings.AutoExitOnWindowClose)
    {
        KeepRunning = KeepRunning && ShouldContinueRunning();
    }
#endif

    return KeepRunning;
}

#if defined(SNAPI_GF_ENABLE_RENDERER) || (defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI))
bool GameRuntime::ProcessPlatformAndUiInput()
{
    if (!m_world)
    {
        return false;
    }

    bool ContinueRunning = true;
#if defined(SNAPI_GF_ENABLE_RENDERER) && SNAPI_GF_RUNTIME_HAS_SDL3
    bool InputAvailableForPlatformEvents = false;
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(SNAPI_GF_ENABLE_UI) && SNAPI_GF_RUNTIME_HAS_SDL3
    if (m_settings.AutoUpdateUiDpiScaleFromWindow && m_world->Renderer().IsInitialized() && m_world->UI().IsInitialized())
    {
        const float Scale = QueryWindowDisplayScale(m_world->Renderer().Window());
        if (std::isfinite(Scale) && Scale > 0.0f && std::abs(Scale - m_uiDpiScaleCache) > 0.0001f)
        {
            m_uiDpiScaleCache = Scale;
            (void)m_world->UI().SetDpiScale(Scale);
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_INPUT)
    bool InputInitialized = m_world->Input().IsInitialized();
    if (InputInitialized)
    {
#if defined(SNAPI_GF_ENABLE_RENDERER) && SNAPI_GF_RUNTIME_HAS_SDL3
#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
        InputAvailableForPlatformEvents = m_world->Input().Settings().Backend == SnAPI::Input::EInputBackend::SDL3;
#else
        InputAvailableForPlatformEvents = false;
#endif
#endif

        const auto* Events = m_world->Input().Events();

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
        const bool ForwardToUi = m_settings.AutoForwardInputEventsToUi && m_world->UI().IsInitialized();

        UiViewportTransform UiTransform{};
        if (ForwardToUi)
        {
#if defined(SNAPI_GF_ENABLE_RENDERER)
            if (m_world->Renderer().IsInitialized())
            {
                if (const auto* GraphicsApi = m_world->Renderer().Graphics())
                {
                    std::optional<std::uint64_t> ViewportId{};
                    const auto RootContextId = m_world->UI().RootContextId();
                    if (RootContextId != 0)
                    {
                        ViewportId = m_world->UI().BoundViewportForContext(RootContextId);
                    }

                    if (!ViewportId.has_value() && GraphicsApi->IsUsingDefaultViewport())
                    {
                        ViewportId = GraphicsApi->DefaultRenderViewportID();
                    }

                    if (ViewportId.has_value() && *ViewportId != 0)
                    {
                        if (const auto Config = GraphicsApi->GetRenderViewportConfig(
                                static_cast<SnAPI::Graphics::RenderViewportID>(*ViewportId)))
                        {
                            UiTransform.OutputX = Config->OutputRect.X;
                            UiTransform.OutputY = Config->OutputRect.Y;
                            UiTransform.OutputWidth = Config->OutputRect.Width;
                            UiTransform.OutputHeight = Config->OutputRect.Height;
                            UiTransform.UiWidth = static_cast<float>(Config->RenderExtent.x());
                            UiTransform.UiHeight = static_cast<float>(Config->RenderExtent.y());
                        }
                    }
                }
            }
#endif

            if (!UiTransform.IsValid())
            {
#if defined(SNAPI_GF_ENABLE_RENDERER)
                if (const auto* Window = m_world->Renderer().Window())
                {
                    const auto WindowSize = Window->Size();
                    UiTransform.OutputX = 0.0f;
                    UiTransform.OutputY = 0.0f;
                    UiTransform.OutputWidth = WindowSize.x();
                    UiTransform.OutputHeight = WindowSize.y();
                }
#endif
                const auto& UiSettings = m_world->UI().Settings();
                UiTransform.UiWidth = UiSettings.ViewportWidth;
                UiTransform.UiHeight = UiSettings.ViewportHeight;
                if (UiTransform.OutputWidth <= 0.0f || UiTransform.OutputHeight <= 0.0f)
                {
                    UiTransform.OutputWidth = UiTransform.UiWidth;
                    UiTransform.OutputHeight = UiTransform.UiHeight;
                }
            }
        }

        const auto ToUiPoint = [&](const float WindowX, const float WindowY) {
            const auto [UiX, UiY] = UiTransform.MapWindowPointToUi(WindowX, WindowY);
            return SnAPI::UI::UIPoint{UiX, UiY};
        };
#endif

        if (Events)
        {
            for (const auto& Event : *Events)
            {
#if defined(SNAPI_GF_ENABLE_RENDERER)
                if (m_settings.AutoExitOnWindowClose && Event.Type == SnAPI::Input::EInputEventType::WindowCloseRequested)
                {
                    ContinueRunning = false;
                }
#endif

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
                if (!ForwardToUi)
                {
                    continue;
                }

                auto& UiSystem = m_world->UI();
                switch (Event.Type)
                {
                case SnAPI::Input::EInputEventType::MouseMove:
                    if (const auto* MoveData = std::get_if<SnAPI::Input::MouseMoveEvent>(&Event.Data))
                    {
                        SnAPI::UI::PointerEvent UiPointer{};
                        UiPointer.Position = ToUiPoint(MoveData->X, MoveData->Y);
                        UiPointer.LeftDown = m_uiLeftDown;
                        UiPointer.RightDown = m_uiRightDown;
                        UiPointer.MiddleDown = m_uiMiddleDown;
                        UiSystem.PushInput(UiPointer);
                    }
                    break;
                case SnAPI::Input::EInputEventType::MouseButtonDown:
                case SnAPI::Input::EInputEventType::MouseButtonUp:
                    if (const auto* ButtonData = std::get_if<SnAPI::Input::MouseButtonEvent>(&Event.Data))
                    {
                        const bool Down = (Event.Type == SnAPI::Input::EInputEventType::MouseButtonDown);
                        switch (ButtonData->Button)
                        {
                        case SnAPI::Input::EMouseButton::Left:
                            m_uiLeftDown = Down;
                            break;
                        case SnAPI::Input::EMouseButton::Right:
                            m_uiRightDown = Down;
                            break;
                        case SnAPI::Input::EMouseButton::Middle:
                            m_uiMiddleDown = Down;
                            break;
                        default:
                            break;
                        }

                        SnAPI::UI::PointerEvent UiPointer{};
                        UiPointer.LeftDown = m_uiLeftDown;
                        UiPointer.RightDown = m_uiRightDown;
                        UiPointer.MiddleDown = m_uiMiddleDown;

                        if (const auto* Snapshot = m_world->Input().Snapshot())
                        {
                            UiPointer.Position = ToUiPoint(Snapshot->Mouse().X, Snapshot->Mouse().Y);
                        }

                        UiSystem.PushInput(UiPointer);
                    }
                    break;
                case SnAPI::Input::EInputEventType::MouseWheel:
                    if (const auto* WheelData = std::get_if<SnAPI::Input::MouseWheelEvent>(&Event.Data))
                    {
                        SnAPI::UI::WheelEvent UiWheel{};
                        UiWheel.DeltaX = WheelData->DeltaX;
                        UiWheel.DeltaY = WheelData->DeltaY;
                        if (const auto* Snapshot = m_world->Input().Snapshot())
                        {
                            UiWheel.Position = ToUiPoint(Snapshot->Mouse().X, Snapshot->Mouse().Y);
                        }
                        UiSystem.PushInput(UiWheel);
                    }
                    break;
                case SnAPI::Input::EInputEventType::KeyDown:
                case SnAPI::Input::EInputEventType::KeyUp:
                    if (const auto* KeyData = std::get_if<SnAPI::Input::KeyEvent>(&Event.Data))
                    {
                        SnAPI::UI::KeyEvent UiKey{};
                        UiKey.KeyCode = MapUiKeyCode(KeyData->Key);
                        UiKey.Down = (Event.Type == SnAPI::Input::EInputEventType::KeyDown);
                        UiKey.Repeat = KeyData->Repeat;
                        UiKey.Shift = KeyData->Modifiers.Shift();
                        UiKey.Ctrl = KeyData->Modifiers.Control();
                        UiKey.Alt = KeyData->Modifiers.Alt();
                        UiSystem.PushInput(UiKey);
                    }
                    break;
                case SnAPI::Input::EInputEventType::TextInput:
                    if (const auto* TextData = std::get_if<SnAPI::Input::TextInputEvent>(&Event.Data))
                    {
                        PushUtf8CodepointsToUi(TextData->Text, UiSystem);
                    }
                    break;
                default:
                    break;
                }
#endif
            }
        }

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
        if (ForwardToUi)
        {
            if (const auto* Snapshot = m_world->Input().Snapshot())
            {
                m_uiLeftDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Left);
                m_uiRightDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Right);
                m_uiMiddleDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Middle);
            }
        }
#endif
    }
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER) && SNAPI_GF_RUNTIME_HAS_SDL3
    if (!InputAvailableForPlatformEvents && m_settings.AutoExitOnWindowClose && m_world->Renderer().IsInitialized())
    {
        SDL_Event Event{};
        while (SDL_PollEvent(&Event))
        {
            if (Event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED || Event.type == SDL_EVENT_QUIT)
            {
                ContinueRunning = false;
            }
        }
    }
#endif

    return ContinueRunning;
}
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
bool GameRuntime::ShouldContinueRunning() const
{
    if (!m_world)
    {
        return false;
    }

    const auto& Renderer = m_world->Renderer();
    if (!Renderer.IsInitialized())
    {
        return true;
    }

    const auto* Window = Renderer.Window();
    if (!Window)
    {
        return true;
    }

    return Window->IsOpen();
}
#endif

void GameRuntime::ApplyFramePacing(const FrameClock::time_point FrameStart)
{
    if (!ShouldCapFrameRate())
    {
        m_framePacerStep = FrameClock::duration::zero();
        m_nextFrameDeadline = FrameClock::time_point{};
        m_framePacerArmed = false;
        return;
    }

    const float MaxFps = m_settings.Tick.MaxFpsWhenVSyncOff;
    const auto TargetStep = std::chrono::duration_cast<FrameClock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(MaxFps)));
    if (TargetStep <= FrameClock::duration::zero())
    {
        m_framePacerStep = FrameClock::duration::zero();
        m_nextFrameDeadline = FrameClock::time_point{};
        m_framePacerArmed = false;
        return;
    }

    if (!m_framePacerArmed || m_framePacerStep != TargetStep)
    {
        m_framePacerStep = TargetStep;
        m_nextFrameDeadline = FrameStart + m_framePacerStep;
        m_framePacerArmed = true;
    }
    else if (FrameStart >= m_nextFrameDeadline)
    {
        // If the frame already exceeded target budget, do not add extra delay:
        // rebase deadline from the current frame start to avoid pacing oscillation.
        m_nextFrameDeadline = FrameStart + m_framePacerStep;
    }

    for (;;)
    {
        const auto Now = FrameClock::now();
        if (Now >= m_nextFrameDeadline)
        {
            break;
        }

        const auto Remaining = m_nextFrameDeadline - Now;
        if (Remaining > kFrameSleepMargin)
        {
            std::this_thread::sleep_for(Remaining - kFrameSleepMargin);
        }
        else if (Remaining > kFrameYieldMargin)
        {
            std::this_thread::yield();
        }
    }

    m_nextFrameDeadline += m_framePacerStep;
}

bool GameRuntime::ShouldCapFrameRate() const
{
    if (!m_world)
    {
        return false;
    }

    const float MaxFps = m_settings.Tick.MaxFpsWhenVSyncOff;
    if (!std::isfinite(MaxFps) || MaxFps <= 0.0f)
    {
        return false;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    const auto& Renderer = m_world->Renderer();
    if (!Renderer.IsInitialized())
    {
        return false;
    }

    const auto* Window = Renderer.Window();
    if (!Window || !Window->IsOpen())
    {
        return false;
    }

    //return Window->VSyncMode() == SnAPI::Graphics::EWindowVSyncMode::Off;
    return true;
#else
    return false;
#endif
}

World* GameRuntime::WorldPtr()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return m_world.get();
}

const World* GameRuntime::WorldPtr() const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return m_world.get();
}

World& GameRuntime::World()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    DEBUG_ASSERT(m_world != nullptr, "GameRuntime::World() called before Init()");
    return *m_world;
}

const World& GameRuntime::World() const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    DEBUG_ASSERT(m_world != nullptr, "GameRuntime::World() called before Init()");
    return *m_world;
}

const GameRuntimeSettings& GameRuntime::Settings() const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return m_settings;
}

GameplayHost* GameRuntime::Gameplay()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return m_gameplayHost.get();
}

const GameplayHost* GameRuntime::Gameplay() const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return m_gameplayHost.get();
}

Result GameRuntime::StartGameplayHost()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "GameRuntime is not initialized"));
    }

    if (!m_settings.Gameplay.has_value())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime gameplay settings are not configured"));
    }

    if (m_gameplayHost && m_gameplayHost->IsInitialized())
    {
        return Ok();
    }

    if (m_gameplayHost)
    {
        m_gameplayHost->Shutdown();
        m_gameplayHost.reset();
    }

    m_gameplayHost = std::make_unique<GameplayHost>();
    m_world->SetGameplayHost(m_gameplayHost.get());
    auto InitGameplay = m_gameplayHost->Initialize(*this, *m_settings.Gameplay);
    if (!InitGameplay)
    {
        m_world->SetGameplayHost(nullptr);
        m_gameplayHost.reset();
        return std::unexpected(InitGameplay.error());
    }

    return Ok();
}

void GameRuntime::StopGameplayHost()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (m_gameplayHost)
    {
        m_gameplayHost->Shutdown();
    }

    if (m_world)
    {
        m_world->SetGameplayHost(nullptr);
    }

    m_gameplayHost.reset();
}

#if defined(SNAPI_GF_ENABLE_UI) && defined(SNAPI_GF_ENABLE_RENDERER)
Result GameRuntime::BindViewportWithUI(const std::uint64_t ViewportID, const UISystem::ContextId ContextID)
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (ViewportID == 0 || ContextID == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Viewport and context ids must be greater than zero"));
    }

    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "GameRuntime is not initialized"));
    }

    auto& UI = m_world->UI();
    auto& Renderer = m_world->Renderer();
    if (!UI.IsInitialized() || !Renderer.IsInitialized())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI or renderer system is not initialized"));
    }

    if (!Renderer.HasRenderViewport(ViewportID))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Renderer viewport does not exist"));
    }

    return UI.BindViewportContext(ViewportID, ContextID);
}

Result GameRuntime::UnbindViewportFromUI(const std::uint64_t ViewportID)
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (ViewportID == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Viewport id must be greater than zero"));
    }

    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "GameRuntime is not initialized"));
    }

    auto& UI = m_world->UI();
    if (!UI.IsInitialized())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    return UI.UnbindViewportContext(ViewportID);
}

std::optional<UISystem::ContextId> GameRuntime::BoundUIContext(const std::uint64_t ViewportID) const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (!m_world || !m_world->UI().IsInitialized())
    {
        return std::nullopt;
    }

    return m_world->UI().BoundContextForViewport(ViewportID);
}

std::optional<std::uint64_t> GameRuntime::BoundViewport(const UISystem::ContextId ContextID) const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (!m_world || !m_world->UI().IsInitialized())
    {
        return std::nullopt;
    }

    return m_world->UI().BoundViewportForContext(ContextID);
}
#endif

void GameRuntime::EnsureBuiltinTypesRegistered()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    static std::once_flag Once;
    std::call_once(Once, [] {
        RegisterBuiltinTypes();
    });
}

} // namespace SnAPI::GameFramework
