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

#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
#include <SnAPI/Profiler/Profiler.h>

#include <cerrno>
#include <cstdlib>
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <WindowBase.hpp>
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
    m_world = std::make_unique<class World>(std::move(WorldName));
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

    return Ok();
}

void GameRuntime::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
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
}

bool GameRuntime::IsInitialized() const
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    return static_cast<bool>(m_world);
}

void GameRuntime::Update(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    if (!m_world)
    {
        return;
    }

    const auto FrameStart = FrameClock::now();

    SNAPI_GF_PROFILE_BEGIN_FRAME_AUTO();
    {
        SNAPI_GF_PROFILE_SCOPE("Frame.Update", "Runtime");

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

        {
            SNAPI_GF_PROFILE_SCOPE("Frame.Pacing", "Runtime");
            ApplyFramePacing(FrameStart);
        }
    }

    SNAPI_GF_PROFILE_END_FRAME();
}

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

void GameRuntime::EnsureBuiltinTypesRegistered()
{
    SNAPI_GF_PROFILE_FUNCTION("Runtime");
    static std::once_flag Once;
    std::call_once(Once, [] {
        RegisterBuiltinTypes();
    });
}

} // namespace SnAPI::GameFramework
