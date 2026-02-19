#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "GameFramework.hpp"
#include "VulkanGraphicsAPI.hpp"
#include <SSAOPass.hpp>
#include <SSRPass.hpp>
#include <MeshManager.hpp>
#include <CameraBase.hpp>

#include "LightManager.hpp"
#if defined(SNAPI_GF_ENABLE_UI)
#include <UIDockZone.h>
#include <UIPanel.h>
#include <UIProperties.h>
#include <UISlider.h>
#include <UIRealtimeGraph.h>
#include <UIScrollContainer.h>
#include <UISizing.h>
#include <UIText.h>
#include <UITextInput.h>
#include <UIContext.h>
#include <UITheme.h>
#endif
#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
#include <SnAPI/Profiler/Profiler.h>
#endif

#if !defined(SNAPI_GF_ENABLE_RENDERER)
int main(int, char**)
{
    std::cerr << "MultiplayerExample requires SNAPI_GF_ENABLE_RENDERER\n";
    return 1;
}
#else

#include <SDL3/SDL.h>

using namespace SnAPI::GameFramework;
using namespace SnAPI::Networking;

namespace
{

constexpr std::string_view kCubeMeshPath = "assets/cube.obj";
constexpr std::string_view kGroundMeshPath = "assets/ground.obj";
constexpr std::string_view kPlayerMeshPath = "assets/cube.obj";
constexpr Vec3::Scalar kEarthSurfaceY = static_cast<Vec3::Scalar>(6360e3) + static_cast<Vec3::Scalar>(10.0);

#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
std::optional<bool> ParseProfilerBooleanEnv(const char* Value)
{
    if (Value == nullptr || Value[0] == '\0')
    {
        return std::nullopt;
    }

    const std::string_view Raw(Value);
    if (Raw == "1" || Raw == "true" || Raw == "TRUE" || Raw == "on" || Raw == "ON")
    {
        return true;
    }
    if (Raw == "0" || Raw == "false" || Raw == "FALSE" || Raw == "off" || Raw == "OFF")
    {
        return false;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> ParseProfilerUnsignedEnv(const char* Name)
{
    const char* Raw = std::getenv(Name);
    if (Raw == nullptr || Raw[0] == '\0')
    {
        return std::nullopt;
    }

    char* End = nullptr;
    const unsigned long long Parsed = std::strtoull(Raw, &End, 10);
    if (End == Raw || End == nullptr || End[0] != '\0')
    {
        return std::nullopt;
    }

    return static_cast<std::uint64_t>(Parsed);
}

enum class EExampleProfilerMode
{
    RawReplay,
    Stream,
};

bool EqualsIgnoreCase(const std::string_view Left, const std::string_view Right)
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

EExampleProfilerMode ResolveExampleProfilerMode()
{
    const char* RawMode = std::getenv("SNAPI_MULTIPLAYER_PROFILER_MODE");
    if (RawMode == nullptr || RawMode[0] == '\0')
    {
        return EExampleProfilerMode::RawReplay;
    }

    const std::string_view ModeValue(RawMode);
    if (EqualsIgnoreCase(ModeValue, "stream") || EqualsIgnoreCase(ModeValue, "udp"))
    {
        return EExampleProfilerMode::Stream;
    }
    return EExampleProfilerMode::RawReplay;
}

void ConfigureProfilerStreamForMultiplayerExample()
{
    auto RuntimeProfilerConfig = SnAPI::Profiler::Profiler::Get().GetConfig();
    // Preserve-overflow has high runtime cost when scope volume exceeds the ring budget.
    // Keep it opt-in for MultiplayerExample so profiling overhead stays predictable.
    RuntimeProfilerConfig.PreserveOverflowEvents = false;
    RuntimeProfilerConfig.PerThreadEventBufferCapacity =
        std::max<std::uint32_t>(RuntimeProfilerConfig.PerThreadEventBufferCapacity, 131072u);
    if (const auto ParsedPreserve = ParseProfilerBooleanEnv(std::getenv("SNAPI_GF_PROFILER_PRESERVE_OVERFLOW_EVENTS")))
    {
        RuntimeProfilerConfig.PreserveOverflowEvents = *ParsedPreserve;
    }

    if (const auto EventCapacity = ParseProfilerUnsignedEnv("SNAPI_GF_PROFILER_EVENT_BUFFER_CAPACITY"))
    {
        RuntimeProfilerConfig.PerThreadEventBufferCapacity = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            std::max<std::uint64_t>(*EventCapacity, 2),
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
    }

    SnAPI::Profiler::Profiler::Get().Configure(RuntimeProfilerConfig);

    const EExampleProfilerMode DefaultMode = ResolveExampleProfilerMode();

    auto RawConfig = SnAPI::Profiler::Profiler::Get().GetRawTraceConfig();
    RawConfig.Mode = DefaultMode == EExampleProfilerMode::RawReplay
                         ? SnAPI::Profiler::RawTraceMode::Record
                         : SnAPI::Profiler::RawTraceMode::Disabled;
    RawConfig.CaptureOnly = DefaultMode == EExampleProfilerMode::RawReplay;

    if (const char* TraceModeValue = std::getenv("SNAPI_GF_PROFILER_TRACE_MODE");
        TraceModeValue != nullptr && TraceModeValue[0] != '\0')
    {
        const std::string_view ModeValue(TraceModeValue);
        if (EqualsIgnoreCase(ModeValue, "record") || EqualsIgnoreCase(ModeValue, "capture") ||
            EqualsIgnoreCase(ModeValue, "on") || EqualsIgnoreCase(ModeValue, "enabled"))
        {
            RawConfig.Mode = SnAPI::Profiler::RawTraceMode::Record;
        }
        else if (EqualsIgnoreCase(ModeValue, "off") || EqualsIgnoreCase(ModeValue, "disabled") ||
                 EqualsIgnoreCase(ModeValue, "none"))
        {
            RawConfig.Mode = SnAPI::Profiler::RawTraceMode::Disabled;
        }
    }

    if (const char* TracePath = std::getenv("SNAPI_GF_PROFILER_TRACE_PATH");
        TracePath != nullptr && TracePath[0] != '\0')
    {
        RawConfig.Path = TracePath;
    }

    if (const auto CaptureOnlyOverride = ParseProfilerBooleanEnv(std::getenv("SNAPI_GF_PROFILER_TRACE_CAPTURE_ONLY")))
    {
        RawConfig.CaptureOnly = *CaptureOnlyOverride;
    }

    SnAPI::Profiler::Profiler::Get().ConfigureRawTrace(RawConfig);

    auto StreamConfig = SnAPI::Profiler::Profiler::Get().GetRealtimeStreamConfig();

    // MultiplayerExample defaults to raw replay capture; stream mode can be enabled explicitly.
    StreamConfig.Enabled = DefaultMode == EExampleProfilerMode::Stream;
    StreamConfig.SendFullSnapshot = DefaultMode == EExampleProfilerMode::Stream;
    if (const std::optional<bool> ParsedEnable =
            ParseProfilerBooleanEnv(std::getenv("SNAPI_GF_PROFILER_STREAM_ENABLE")))
    {
        StreamConfig.Enabled = *ParsedEnable;
    }

    if (const auto ParsedSendFull = ParseProfilerBooleanEnv(std::getenv("SNAPI_GF_PROFILER_STREAM_SEND_FULL")))
    {
        StreamConfig.SendFullSnapshot = *ParsedSendFull;
    }

    if (const auto MaxPayload = ParseProfilerUnsignedEnv("SNAPI_GF_PROFILER_STREAM_MAX_UDP_PAYLOAD_BYTES"))
    {
        StreamConfig.MaxUdpPayloadBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
            std::max<std::uint64_t>(*MaxPayload, 1200),
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
    }

    if (const auto ChunkPayload = ParseProfilerUnsignedEnv("SNAPI_GF_PROFILER_STREAM_CHUNK_PAYLOAD_BYTES"))
    {
        StreamConfig.ChunkPayloadBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
            std::max<std::uint64_t>(*ChunkPayload, 512),
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
    }

    if (const auto Chunking = ParseProfilerBooleanEnv(std::getenv("SNAPI_GF_PROFILER_STREAM_ENABLE_CHUNKING")))
    {
        StreamConfig.EnablePayloadChunking = *Chunking;
    }

    if (RawConfig.Mode == SnAPI::Profiler::RawTraceMode::Record && RawConfig.CaptureOnly)
    {
        StreamConfig.Enabled = false;
    }

    SnAPI::Profiler::Profiler::Get().ConfigureRealtimeStream(StreamConfig);
}
#endif

/**
 * @brief Parsed command-line configuration for multiplayer example runtime mode.
 */
struct Args
{
    bool Server = false;
    bool Client = false;
    bool Local = false;
    bool DisableInterpolation = false;
    bool ResetSimulation = false;
    std::string Host = "127.0.0.1";
    std::string Bind = "0.0.0.0";
    std::uint16_t Port = 7777;
    int CubeCount = 256;
    float FixedHz = 30.0f;
    int MaxFixedSteps = 4;
    std::optional<std::uint32_t> MaxSubStepping{3};
    bool CubeShadows = true;
};

void InitializeWorkingDirectory(const char* ExeArgv0)
{
    if (!ExeArgv0 || *ExeArgv0 == '\0')
    {
        return;
    }

    namespace fs = std::filesystem;
    std::error_code Ec;
    fs::path ExePath(ExeArgv0);
    if (ExePath.is_relative())
    {
        ExePath = fs::absolute(ExePath, Ec);
        if (Ec)
        {
            return;
        }
    }

    const fs::path ExeDir = ExePath.parent_path();
    if (ExeDir.empty())
    {
        return;
    }

    fs::current_path(ExeDir, Ec);
}

std::string EndpointToString(const NetEndpoint& Endpoint)
{
    return Endpoint.Address + ":" + std::to_string(Endpoint.Port);
}

const char* DisconnectReasonToString(const EDisconnectReason Reason)
{
    switch (Reason)
    {
    case EDisconnectReason::None:
        return "None";
    case EDisconnectReason::ReliableSendExceeded:
        return "ReliableSendExceeded";
    default:
        return "Unknown";
    }
}

/**
 * @brief Session listener that logs high-signal connection lifecycle events.
 */
struct SessionListener final : public INetSessionListener
{
    explicit SessionListener(std::string LabelValue, NetSession* SessionValue = nullptr)
        : Label(std::move(LabelValue))
        , SessionRef(SessionValue)
    {
    }

    void OnConnectionAdded(const NetConnectionEvent& Event) override
    {
        std::cout << "[" << Label << "] Connection added handle=" << Event.Handle
                  << " transport=" << Event.Transport
                  << " remote=" << EndpointToString(Event.Remote) << "\n";
    }

    void OnConnectionReady(const NetConnectionEvent& Event) override
    {
        std::cout << "[" << Label << "] Connection ready handle=" << Event.Handle
                  << " remote=" << EndpointToString(Event.Remote) << "\n";
    }

    void OnConnectionClosed(const NetConnectionEvent& Event) override
    {
        std::cout << "[" << Label << "] Connection closed handle=" << Event.Handle
                  << " remote=" << EndpointToString(Event.Remote);
        if (SessionRef)
        {
            if (const auto Dump = SessionRef->DumpConnection(Event.Handle, Clock::now()))
            {
                std::cout << " reason=" << DisconnectReasonToString(Dump->DisconnectReason)
                          << " pending_rel=" << Dump->PendingReliableCount
                          << " pending_unrel=" << Dump->PendingUnreliableCount
                          << " strikes=" << Dump->Strikes;
            }
        }
        std::cout << "\n";
    }

    void OnConnectionMigrated(const NetConnectionEvent& Event,
                              const NetEndpoint& PreviousRemote) override
    {
        std::cout << "[" << Label << "] Connection migrated handle=" << Event.Handle
                  << " from=" << EndpointToString(PreviousRemote)
                  << " to=" << EndpointToString(Event.Remote) << "\n";
    }

    std::string Label;
    NetSession* SessionRef = nullptr;
};

void PrintConnectionDump(const std::string& Label,
                         const NetConnectionDump& Dump,
                         const float TimeSeconds)
{
    std::cout << "[" << Label << "] t=" << TimeSeconds
              << " handle=" << Dump.Handle
              << " remote=" << EndpointToString(Dump.Remote)
              << " ready=" << (Dump.HandshakeComplete ? "true" : "false")
              << " mtu=" << Dump.MtuBytes
              << " pending_rel=" << Dump.PendingReliableCount
              << " pending_unrel=" << Dump.PendingUnreliableCount
              << " strikes=" << Dump.Strikes
              << " pkt_sent=" << Dump.Stats.PacketsSent
              << " pkt_acked=" << Dump.Stats.PacketsAcked
              << " pkt_lost=" << Dump.Stats.PacketsLost;

    if (Dump.DisconnectRequested || Dump.DisconnectSent)
    {
        std::cout << " disconnect=" << DisconnectReasonToString(Dump.DisconnectReason);
    }

    if (Dump.PendingPathRemote)
    {
        std::cout << " pending_path=" << EndpointToString(*Dump.PendingPathRemote);
    }

    std::cout << "\n";
}

void PrintUsage(const char* Exe)
{
    std::cout
        << "Usage:\n"
        << "  " << Exe
        << " --server [--bind <addr>] [--port <port>] [--count <max-live>] [--fixed-hz <hz>] [--max-fixed-steps <n>] [--substeps <n>] [--cube-shadows] [--no-reset-sim]\n"
        << "  " << Exe
        << " --client [--host <addr>] [--bind <addr>] [--port <port>] [--fixed-hz <hz>] [--max-fixed-steps <n>] [--substeps <n>] [--cube-shadows] [--no-interp]\n"
        << "  " << Exe
        << " --local [--count <max-live>] [--fixed-hz <hz>] [--max-fixed-steps <n>] [--substeps <n>] [--cube-shadows] [--no-interp] [--no-reset-sim]\n";
}

bool ParseArgs(const int argc, char** argv, Args& Out)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view Arg = argv[i];
        if (Arg == "--server")
        {
            Out.Server = true;
        }
        else if (Arg == "--client")
        {
            Out.Client = true;
        }
        else if (Arg == "--local")
        {
            Out.Local = true;
        }
        else if (Arg == "--host" && i + 1 < argc)
        {
            Out.Host = argv[++i];
        }
        else if (Arg == "--bind" && i + 1 < argc)
        {
            Out.Bind = argv[++i];
        }
        else if (Arg == "--port" && i + 1 < argc)
        {
            Out.Port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (Arg == "--count" && i + 1 < argc)
        {
            Out.CubeCount = std::max(1, std::stoi(argv[++i]));
        }
        else if (Arg == "--fixed-hz" && i + 1 < argc)
        {
            Out.FixedHz = std::max(1.0f, std::stof(argv[++i]));
        }
        else if (Arg == "--max-fixed-steps" && i + 1 < argc)
        {
            Out.MaxFixedSteps = std::max(1, std::stoi(argv[++i]));
        }
        else if (Arg == "--substeps" && i + 1 < argc)
        {
            const int Requested = std::stoi(argv[++i]);
            if (Requested <= 0)
            {
                Out.MaxSubStepping.reset();
            }
            else
            {
                Out.MaxSubStepping = static_cast<std::uint32_t>(Requested);
            }
        }
        else if (Arg == "--cube-shadows")
        {
            Out.CubeShadows = true;
        }
        else if (Arg == "--no-cube-shadows")
        {
            Out.CubeShadows = false;
        }
        else if (Arg == "--no-interp")
        {
            Out.DisableInterpolation = true;
        }
        else if (Arg == "--no-reset-sim")
        {
            Out.ResetSimulation = false;
        }
        else if (Arg == "--reset-sim")
        {
            Out.ResetSimulation = true;
        }
        else if (Arg == "--help")
        {
            return false;
        }
        else
        {
            std::cerr << "Unknown argument: " << Arg << "\n";
            return false;
        }
    }

    const int ModeCount = (Out.Server ? 1 : 0) + (Out.Client ? 1 : 0) + (Out.Local ? 1 : 0);
    return ModeCount == 1;
}

void RegisterExampleTypes()
{
    static bool Registered = false;
    if (Registered)
    {
        return;
    }
    RegisterBuiltinTypes();
    Registered = true;
}

NetConfig MakeNetConfig()
{
    NetConfig Config{};
    Config.Threading.UseInternalThreads = true;

    // 25 Mbit/s target budget.
    Config.Pacing.MaxBytesPerSecond = 300'125'000;
    Config.Pacing.BurstBytes = 160 * 1024 * 1024;
    Config.Pacing.MaxBytesPerPump = 800 * 1024 * 1024;

    Config.Reliability.ResendTimeout = Milliseconds{300};
    Config.Reliability.MaxAttempts = 32;
    Config.Queues.MaxReliablePendingBytes = 100 * 1024 * 1024;

    Config.Replication.MaxEntitiesPerPump = 0; // 0 = no per-pump entity cap
    Config.KeepAlive.Timeout = Milliseconds{20000};
    return Config;
}

UdpTransportConfig MakeUdpTransportConfig()
{
    UdpTransportConfig TransportConfig{};
    TransportConfig.MaxDatagramBytes = 2048;
    TransportConfig.NonBlocking = true;
    return TransportConfig;
}

GameRuntimeSettings MakeRuntimeSettings(const Args& Parsed,
                                        const bool ServerMode,
                                        SessionListener* Listener,
                                        const bool EnableNetworking,
                                        const bool EnableWindow)
{
    GameRuntimeSettings Settings{};

    if (EnableNetworking)
    {
        Settings.WorldName = ServerMode ? "ServerWorld" : "ClientWorld";
    }
    else
    {
        Settings.WorldName = "LocalWorld";
    }

    Settings.RegisterBuiltins = false;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / std::max(1.0f, Parsed.FixedHz);
    // A higher catch-up budget avoids visible slow-motion when frames occasionally exceed the fixed-step budget.
    Settings.Tick.MaxFixedStepsPerUpdate = static_cast<std::size_t>(std::max(Parsed.MaxFixedSteps, 1));
    Settings.Tick.EnableLateTick = true;
    Settings.Tick.EnableEndFrame = true;
    // Keep frame cadence deterministic when VSync is toggled off at runtime.
    Settings.Tick.MaxFpsWhenVSyncOff = 60.0f;

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (EnableNetworking)
    {
        GameRuntimeNetworkingSettings NetSettings{};
        NetSettings.Role = ServerMode ? ESessionRole::Server : ESessionRole::Client;
        NetSettings.Net = MakeNetConfig();
        NetSettings.Transport = MakeUdpTransportConfig();
        NetSettings.BindAddress = Parsed.Bind;
        NetSettings.BindPort = ServerMode ? Parsed.Port : 0;
        NetSettings.ConnectAddress = Parsed.Host;
        NetSettings.ConnectPort = Parsed.Port;
        NetSettings.AutoConnect = !ServerMode;
        if (Listener)
        {
            NetSettings.SessionListeners.push_back(Listener);
        }
        Settings.Networking = NetSettings;
    }
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    GameRuntimePhysicsSettings PhysicsSettings{};
    const std::uint64_t CubeCount = static_cast<std::uint64_t>(std::max(Parsed.CubeCount, 1));
    PhysicsSettings.Scene.MaxBodyPairs = static_cast<std::uint32_t>(
        std::clamp<std::uint64_t>(CubeCount * CubeCount * 2ull, 65'536ull, 1'000'000ull));
    PhysicsSettings.Scene.MaxContactConstraints = static_cast<std::uint32_t>(
        std::clamp<std::uint64_t>(CubeCount * 64ull, 10'240ull, 262'144ull));
    PhysicsSettings.Scene.TempAllocatorBytes = static_cast<std::uint32_t>(
        std::clamp<std::uint64_t>((16ull * 1024ull * 1024ull) + (CubeCount * 96ull * 1024ull),
                                  16ull * 1024ull * 1024ull,
                                  512ull * 1024ull * 1024ull));
    PhysicsSettings.TickInFixedTick = true;
    PhysicsSettings.TickInVariableTick = false;
    PhysicsSettings.EnableFloatingOrigin = true;
    PhysicsSettings.AutoRebaseFloatingOrigin = true;
    PhysicsSettings.FloatingOriginRebaseDistance = 512.0;
    PhysicsSettings.InitializeFloatingOriginFromFirstBody = false;
    PhysicsSettings.InitialFloatingOrigin = Vec3(0.0, kEarthSurfaceY, 0.0);
    PhysicsSettings.ThreadCount = 8;
    PhysicsSettings.MaxSubStepping = Parsed.MaxSubStepping;
    Settings.Physics = PhysicsSettings;
#endif

#if defined(SNAPI_GF_ENABLE_INPUT)
#if (defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3) || \
    (defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) && SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) || \
    (defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) && SNAPI_INPUT_ENABLE_BACKEND_LIBUSB)
    if (EnableWindow)
    {
        GameRuntimeInputSettings InputSettings{};
#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
        // MultiplayerExample uses a renderer window, so prefer SDL3 input routing when available.
        InputSettings.Backend = SnAPI::Input::EInputBackend::SDL3;
        InputSettings.RegisterSdl3Backend = true;
#endif
        InputSettings.CreateDesc.EnableKeyboard = true;
        InputSettings.CreateDesc.EnableMouse = true;
        InputSettings.CreateDesc.EnableGamepad = true;
        InputSettings.CreateDesc.EnableTextInput = true;
        Settings.Input = InputSettings;
    }
#endif
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (EnableWindow)
    {
        GameRuntimeRendererSettings RendererSettings{};
        RendererSettings.CreateGraphicsApi = true;
        RendererSettings.CreateWindow = true;
        RendererSettings.WindowTitle = EnableNetworking ? "MultiplayerExample" : "MultiplayerExample (Local)";
        RendererSettings.WindowWidth = 1280.0f;
        RendererSettings.WindowHeight = 720.0f;
        RendererSettings.FullScreen = false;
        RendererSettings.Resizable = true;
        RendererSettings.Visible = true;
        RendererSettings.CreateDefaultLighting = true;
        RendererSettings.RegisterDefaultPassGraph = true;
        RendererSettings.CreateDefaultMaterials = true;
        RendererSettings.CreateDefaultEnvironmentProbe = true;
        RendererSettings.DefaultEnvironmentProbeY = 6360e3f + 1000.0f;
        Settings.Renderer = RendererSettings;
    }
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    if (EnableWindow)
    {
        GameRuntimeUiSettings UiSettings{};
        UiSettings.ViewportWidth = 1280.0f;
        UiSettings.ViewportHeight = 720.0f;
        Settings.UI = UiSettings;
    }
#endif

    return Settings;
}

struct CubeSlot
{
    NodeHandle Handle{};
    TransformComponent* Transform = nullptr;
    StaticMeshComponent* Mesh = nullptr;
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    RigidBodyComponent* Body = nullptr;
    Vec3 ParkPosition{};
#endif
    bool Active = false;
    float SpawnedAtSeconds = 0.0f;
};

struct PlayerActor
{
    NodeHandle Handle{};
    TransformComponent* Transform = nullptr;
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    RigidBodyComponent* Body = nullptr;
    CharacterMovementController* Movement = nullptr;
#endif
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)
    InputComponent* Input = nullptr;
#endif
};

struct CubeSharedMaterials
{
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> GBuffer{};
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> Shadow{};
};

CubeSharedMaterials BuildSharedCubeMaterialInstances(World& Graph, const bool CubeShadows)
{
    CubeSharedMaterials Shared{};

    auto* Meshes = SnAPI::Graphics::MeshManager::Instance();
    if (!Meshes)
    {
        return Shared;
    }

    const auto LoadedMesh = Meshes->Load(std::string(kCubeMeshPath));
    const auto SourceMesh = LoadedMesh.lock();
    if (!SourceMesh)
    {
        return Shared;
    }

    const auto GBufferMaterial = Graph.Renderer().DefaultGBufferMaterial();
    if (!GBufferMaterial)
    {
        return Shared;
    }

    std::size_t MaterialIndex = 0;
    if (!SourceMesh->SubMeshes.empty())
    {
        MaterialIndex = static_cast<std::size_t>(SourceMesh->SubMeshes[0].MaterialIndex);
    }

    SnAPI::Graphics::VertexSourceMaterial SourceMaterial{};
    bool HasSourceMaterial = false;
    if (!SourceMesh->Materials.empty())
    {
        if (MaterialIndex >= SourceMesh->Materials.size())
        {
            MaterialIndex = 0;
        }

        const auto& MeshMaterial = SourceMesh->Materials[MaterialIndex];
        SourceMaterial.Name = MeshMaterial.Name;
        SourceMaterial.DiffuseTexturePath = MeshMaterial.DiffuseTexturePath;
        SourceMaterial.NormalTexturePath = MeshMaterial.NormalTexturePath;
        SourceMaterial.MaterialTexturePath = MeshMaterial.MaterialTexturePath;
        SourceMaterial.EmissiveTexturePath = MeshMaterial.EmissiveTexturePath;
        SourceMaterial.BaseColor = SnAPI::Vector4DF{
            MeshMaterial.BaseColor[0],
            MeshMaterial.BaseColor[1],
            MeshMaterial.BaseColor[2],
            MeshMaterial.BaseColor[3]
        };
        SourceMaterial.EmissiveColor = SnAPI::Vector4DF{
            MeshMaterial.EmissiveColor[0],
            MeshMaterial.EmissiveColor[1],
            MeshMaterial.EmissiveColor[2],
            MeshMaterial.EmissiveColor[3]
        };
        SourceMaterial.Roughness = MeshMaterial.Roughness;
        SourceMaterial.Metallic = MeshMaterial.Metallic;
        SourceMaterial.Occlusion = MeshMaterial.Occlusion;
        HasSourceMaterial = true;

        Shared.GBuffer = Meshes->CreateMaterialInstanceFromSourceMaterial(SourceMaterial, GBufferMaterial);
    }
    else
    {
        Shared.GBuffer = GBufferMaterial->CreateMaterialInstance();
    }

    if (CubeShadows)
    {
        if (const auto ShadowMaterial = Graph.Renderer().DefaultShadowMaterial())
        {
            if (HasSourceMaterial)
            {
                Shared.Shadow = Meshes->CreateMaterialInstanceFromSourceMaterial(SourceMaterial, ShadowMaterial);
            }
            else
            {
                Shared.Shadow = ShadowMaterial->CreateMaterialInstance();
            }
        }
    }

    return Shared;
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)
void SpawnPhysicsGround(World& Graph)
{
    auto GroundNodeResult = Graph.CreateNode("Ground");
    if (!GroundNodeResult)
    {
        return;
    }

    auto* GroundNode = GroundNodeResult->Borrowed();
    if (!GroundNode)
    {
        return;
    }

    if (auto Transform = GroundNode->Add<TransformComponent>())
    {
        Transform->Replicated(true);
        Transform->Position = Vec3(0.0f, kEarthSurfaceY - 1.0f, 0.0f);
        Transform->Scale = Vec3(25.0f, 1.0f, 25.0f);
    }

    if (auto Collider = GroundNode->Add<ColliderComponent>())
    {
        Collider->Replicated(true);
        auto& Settings = Collider->EditSettings();
        Settings.Shape = SnAPI::Physics::EShapeType::Box;
        Settings.HalfExtent = Vec3(255.0f, 0.5f, 255.0f);
        Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
        Settings.Mask = kCollisionMaskAll;
        Settings.Friction = 0.55f;
        Settings.Restitution = 0.2f;
    }

    if (auto RigidBody = GroundNode->Add<RigidBodyComponent>())
    {
        RigidBody->Replicated(true);
        auto& Settings = RigidBody->EditSettings();
        Settings.BodyType = SnAPI::Physics::EBodyType::Static;
        Settings.SyncToPhysics = true;
        (void)RigidBody->RecreateBody();
    }

    if (auto Mesh = GroundNode->Add<StaticMeshComponent>())
    {
        Mesh->Replicated(true);
        auto& Settings = Mesh->EditSettings();
        Settings.MeshPath = std::string(kGroundMeshPath);
        Settings.Visible = true;
        Settings.CastShadows = true;
        Settings.SyncFromTransform = true;
        Settings.RegisterWithRenderer = true;
    }
}

bool CreateCubeSlot(World& Graph,
                    const int CubeIndex,
                    CubeSlot& OutSlot,
                    const bool CubeShadows,
                    const bool DisableInterpolation,
                    const std::shared_ptr<SnAPI::Graphics::MaterialInstance>& SharedGBufferInstance,
                    const std::shared_ptr<SnAPI::Graphics::MaterialInstance>& SharedShadowInstance)
{
    auto NodeResult = Graph.CreateNode("Cube_" + std::to_string(CubeIndex));
    if (!NodeResult)
    {
        return false;
    }

    auto* Node = NodeResult->Borrowed();
    if (!Node)
    {
        return false;
    }

    auto TransformResult = Node->Add<TransformComponent>();
    if (!TransformResult)
    {
        return false;
    }

    auto* Transform = &*TransformResult;
    constexpr int ParkColumns = 64;
    constexpr float ParkSpacing = 3.0f;
    const int ParkXIndex = CubeIndex % ParkColumns;
    const int ParkZIndex = CubeIndex / ParkColumns;
    const float ParkX = (static_cast<float>(ParkXIndex) - (static_cast<float>(ParkColumns) * 0.5f)) * ParkSpacing;
    const float ParkZ = static_cast<float>(ParkZIndex) * ParkSpacing;
    OutSlot.ParkPosition = Vec3(ParkX, kEarthSurfaceY - 1000.0f, ParkZ);

    // Delay initial replication for pooled slots.
    Transform->Replicated(false);
    Transform->Position = OutSlot.ParkPosition;
    Transform->Scale = Vec3(0.0f, 0.0f, 0.0f);

    if (auto Mesh = Node->Add<StaticMeshComponent>())
    {
        Mesh->Replicated(false);
        auto& Settings = Mesh->EditSettings();
        Settings.MeshPath = std::string(kCubeMeshPath);
        Settings.Visible = true;
        Settings.CastShadows = CubeShadows;
        Settings.SyncFromTransform = true;
        Settings.RegisterWithRenderer = true;
        Mesh->SetSharedMaterialInstances(SharedGBufferInstance, SharedShadowInstance);
        OutSlot.Mesh = &*Mesh;
    }

    if (auto Collider = Node->Add<ColliderComponent>())
    {
        auto& Settings = Collider->EditSettings();
        Settings.Shape = SnAPI::Physics::EShapeType::Box;
        Settings.HalfExtent = Vec3(0.5f, 0.5f, 0.5f);
        Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldDynamic);
        Settings.Mask = CollisionMaskFlags(ECollisionFilterBits::WorldStatic | ECollisionFilterBits::WorldDynamic);
        Settings.Friction = 0.55f;
        Settings.Restitution = 0.2f;
    }

    RigidBodyComponent::Settings Settings;
    Settings.BodyType = SnAPI::Physics::EBodyType::Dynamic;
    Settings.Mass = 1.0f;
    Settings.LinearDamping = 0.05f;
    Settings.AngularDamping = 0.08f;
    Settings.EnableCcd = false;
    Settings.StartActive = false;
    Settings.SyncFromPhysics = true;
    Settings.SyncToPhysics = false;
    Settings.EnableRenderInterpolation = !DisableInterpolation;
    if (auto RigidBody = Node->Add<RigidBodyComponent>(Settings))
    {
        (void)RigidBody->Teleport(OutSlot.ParkPosition, Quat::Identity(), true);
        OutSlot.Body = &*RigidBody;
    }

    OutSlot.Handle = Node->Handle();
    OutSlot.Transform = Transform;
    OutSlot.Active = false;
    OutSlot.SpawnedAtSeconds = 0.0f;
    return true;
}

std::size_t CountActiveRenderedCubes(World& Graph)
{
    std::size_t Count = 0;
    Graph.NodePool().ForEach([&Count](const NodeHandle&, BaseNode& Node) {
        auto MeshResult = Node.Component<StaticMeshComponent>();
        if (!MeshResult)
        {
            return;
        }

        const auto& MeshSettings = MeshResult->GetSettings();
        if (!MeshSettings.Visible || std::string_view(MeshSettings.MeshPath) != kCubeMeshPath)
        {
            return;
        }

        // Exclude the controllable player actor, which also uses the cube mesh.
        if (Node.Component<CharacterMovementController>())
        {
            return;
        }

        auto TransformResult = Node.Component<TransformComponent>();
        if (!TransformResult)
        {
            return;
        }

        const Vec3 Scale = TransformResult->Scale;
        const double MaxAbsScale = std::max({
            std::abs(static_cast<double>(Scale.x())),
            std::abs(static_cast<double>(Scale.y())),
            std::abs(static_cast<double>(Scale.z()))});
        if (MaxAbsScale < 0.01)
        {
            return;
        }

        ++Count;
    });
    return Count;
}

void ActivateCubeSlot(CubeSlot& Slot, const float TimeSeconds, std::mt19937& Rng, const float SpawnHalfExtent)
{
    if (!Slot.Transform || !Slot.Body)
    {
        return;
    }

    std::uniform_real_distribution<float> SpawnXZ(-SpawnHalfExtent, SpawnHalfExtent);
    std::uniform_real_distribution<float> SpawnYOffset(8.0f, 12.0f);
    std::uniform_real_distribution<float> InitialVX(-2.5f, 2.5f);
    std::uniform_real_distribution<float> InitialVY(0.0f, 3.0f);
    std::uniform_real_distribution<float> InitialVZ(-2.5f, 2.5f);
    std::uniform_real_distribution<float> InitialAngular(-6.0f, 6.0f);

    if (!Slot.Transform->Replicated())
    {
        Slot.Transform->Replicated(true);
    }
    if (Slot.Mesh && !Slot.Mesh->Replicated())
    {
        Slot.Mesh->Replicated(true);
    }

    const Vec3 SpawnPosition = Vec3(SpawnXZ(Rng), kEarthSurfaceY + SpawnYOffset(Rng), SpawnXZ(Rng));
    Slot.Transform->Scale = Vec3(1.0f, 1.0f, 1.0f);

    (void)Slot.Body->Teleport(SpawnPosition, Quat::Identity(), true);
    (void)Slot.Body->SetVelocity(Vec3(InitialVX(Rng), InitialVY(Rng), InitialVZ(Rng)),
                                 Vec3(InitialAngular(Rng), InitialAngular(Rng), InitialAngular(Rng)));

    Slot.Active = true;
    Slot.SpawnedAtSeconds = TimeSeconds;
}

void DeactivateCubeSlot(CubeSlot& Slot, const float TimeSeconds)
{
    if (!Slot.Transform || !Slot.Body)
    {
        Slot.Active = false;
        Slot.SpawnedAtSeconds = TimeSeconds;
        return;
    }

    (void)Slot.Body->Teleport(Slot.ParkPosition, Quat::Identity(), true);
    Slot.Transform->Scale = Vec3(0.0f, 0.0f, 0.0f);
    Slot.Transform->Position = Slot.ParkPosition;
    Slot.Active = false;
    Slot.SpawnedAtSeconds = TimeSeconds;
}

PlayerActor CreatePlayerActor(World& Graph, const bool EnableInput, const bool DisableInterpolation)
{
    PlayerActor Player{};

    auto PlayerNodeResult = Graph.CreateNode("Player");
    if (!PlayerNodeResult)
    {
        return Player;
    }

    auto* PlayerNode = PlayerNodeResult->Borrowed();
    if (!PlayerNode)
    {
        return Player;
    }
    Player.Handle = PlayerNode->Handle();

    if (auto Transform = PlayerNode->Add<TransformComponent>())
    {
        Transform->Replicated(false);
        Transform->Position = Vec3(0.0f, kEarthSurfaceY + 1.0f, -8.0f);
        Transform->Scale = Vec3(0.8f, 1.8f, 0.8f);
        Player.Transform = &*Transform;
    }

    if (auto Collider = PlayerNode->Add<ColliderComponent>())
    {
        Collider->Replicated(false);
        auto& Settings = Collider->EditSettings();
        Settings.Shape = SnAPI::Physics::EShapeType::Box;
        Settings.HalfExtent = Vec3(0.4f, 0.9f, 0.4f);
        Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldDynamic);
        Settings.Mask = CollisionMaskFlags(ECollisionFilterBits::WorldStatic | ECollisionFilterBits::WorldDynamic);
        Settings.Friction = 0.9f;
        Settings.Restitution = 0.0f;
    }

    RigidBodyComponent::Settings BodySettings{};
    BodySettings.BodyType = SnAPI::Physics::EBodyType::Dynamic;
    BodySettings.Mass = 85.0f;
    BodySettings.LinearDamping = 0.1f;
    BodySettings.AngularDamping = 0.6f;
    BodySettings.EnableCcd = true;
    BodySettings.StartActive = true;
    BodySettings.SyncFromPhysics = true;
    BodySettings.SyncToPhysics = false;
    BodySettings.EnableRenderInterpolation = !DisableInterpolation;
    BodySettings.AutoDeactivateWhenSleeping = false;
    if (auto RigidBody = PlayerNode->Add<RigidBodyComponent>(BodySettings))
    {
        RigidBody->Replicated(false);
        (void)RigidBody->RecreateBody();
        Player.Body = &*RigidBody;
    }

    if (auto Movement = PlayerNode->Add<CharacterMovementController>())
    {
        auto& MovementSettings = Movement->EditSettings();
        MovementSettings.MoveForce = 70.0f;
        MovementSettings.JumpImpulse = 6.5f;
        MovementSettings.GroundProbeStartOffset = 0.15f;
        MovementSettings.GroundProbeDistance = 1.35f;
        MovementSettings.GroundMask = CollisionMaskFlags(ECollisionFilterBits::WorldStatic | ECollisionFilterBits::WorldDynamic);
        MovementSettings.ConsumeInputEachTick = false;
        Player.Movement = &*Movement;
    }

#if defined(SNAPI_GF_ENABLE_INPUT)
    if (EnableInput)
    {
        if (auto Input = PlayerNode->Add<InputComponent>())
        {
            auto& InputSettings = Input->EditSettings();
            InputSettings.MoveScale = 1.0f;
            InputSettings.GamepadDeadzone = 0.18f;
            InputSettings.RequireInputFocus = true;
            InputSettings.NormalizeMove = true;
            InputSettings.MoveForwardKey = SnAPI::Input::EKey::W;
            InputSettings.MoveBackwardKey = SnAPI::Input::EKey::S;
            InputSettings.MoveLeftKey = SnAPI::Input::EKey::A;
            InputSettings.MoveRightKey = SnAPI::Input::EKey::D;
            InputSettings.JumpKey = SnAPI::Input::EKey::Space;
            InputSettings.MoveGamepadXAxis = SnAPI::Input::EGamepadAxis::LeftX;
            InputSettings.MoveGamepadYAxis = SnAPI::Input::EGamepadAxis::LeftY;
            InputSettings.JumpGamepadButton = SnAPI::Input::EGamepadButton::South;
            Player.Input = &*Input;
        }
    }
#else
    (void)EnableInput;
#endif

    if (auto Mesh = PlayerNode->Add<StaticMeshComponent>())
    {
        Mesh->Replicated(false);
        auto& Settings = Mesh->EditSettings();
        Settings.MeshPath = std::string(kPlayerMeshPath);
        Settings.Visible = true;
        Settings.CastShadows = true;
        Settings.SyncFromTransform = true;
        Settings.RegisterWithRenderer = true;
    }

    return Player;
}
#endif

SnAPI::Vector3D ToRendererVector3(const Vec3& Value)
{
    return SnAPI::Vector3D{
        static_cast<SnAPI::Vector3D::Scalar>(Value.x()),
        static_cast<SnAPI::Vector3D::Scalar>(Value.y()),
        static_cast<SnAPI::Vector3D::Scalar>(Value.z())};
}

CameraComponent* CreateStationaryObserverCamera(World& Graph)
{
    auto CameraNodeResult = Graph.CreateNode("StationaryObserverCamera");
    if (!CameraNodeResult)
    {
        return nullptr;
    }

    auto* CameraNode = CameraNodeResult->Borrowed();
    if (!CameraNode)
    {
        return nullptr;
    }

    auto CameraResult = CameraNode->Add<CameraComponent>();
    if (!CameraResult)
    {
        return nullptr;
    }

    auto* Camera = &*CameraResult;
    auto& Settings = Camera->EditSettings();
    Settings.Active = true;
    Settings.SyncFromTransform = false;
    Settings.FovDegrees = 60.0f;
    Settings.NearClip = 0.05f;
    Settings.FarClip = 1200.0f;
    Settings.Aspect = 16.0f / 9.0f;

    if (auto* RendererCamera = Camera->Camera())
    {
        const Vec3 ObserverPosition = Vec3(18.0f, kEarthSurfaceY + 16.0f, 22.0f);
        const Vec3 ObserverTarget = Vec3(0.0f, kEarthSurfaceY + 1.0f, 0.0f);
        RendererCamera->Position(ToRendererVector3(ObserverPosition));
        RendererCamera->LookAt(ToRendererVector3(ObserverTarget));
    }

    return Camera;
}

CameraComponent* CreateViewCamera(World& Graph, const NodeHandle& FollowTarget)
{
    auto CameraNodeResult = Graph.CreateNode("ViewCamera");
    if (!CameraNodeResult)
    {
        return nullptr;
    }

    auto* CameraNode = CameraNodeResult->Borrowed();
    if (!CameraNode)
    {
        return nullptr;
    }

    if (auto Follow = CameraNode->Add<FollowTargetComponent>())
    {
        auto& FollowSettings = Follow->EditSettings();
        FollowSettings.Target = FollowTarget;
        FollowSettings.PositionOffset = Vec3(0.0f, 4.0f, 8.0f);
        FollowSettings.SyncPosition = true;
        FollowSettings.SyncRotation = false;
    }

    if (auto Transform = CameraNode->Add<TransformComponent>())
    {
        Transform->Position = Vec3(0.0f, kEarthSurfaceY + 5.0f, 8.0f);
        Transform->Rotation =
            SnAPI::Math::AngleAxis3D(-SnAPI::Math::SLinearAlgebra::DegreesToRadians(25.0f), SnAPI::Math::Vector3::UnitX());
    }

    auto CameraResult = CameraNode->Add<CameraComponent>();
    if (!CameraResult)
    {
        return nullptr;
    }

    auto* Camera = &*CameraResult;
    auto& Settings = Camera->EditSettings();
    Settings.Active = true;
    Settings.SyncFromTransform = true;
    Settings.FovDegrees = 60.0f;
    Settings.NearClip = 0.05f;
    Settings.FarClip = 800.0f;
    Settings.Aspect = 16.0f / 9.0f;

    return Camera;
}

struct UiViewportTransform
{
    float OutputX{0.0f};
    float OutputY{0.0f};
    float OutputWidth{0.0f};
    float OutputHeight{0.0f};
    float UiWidth{0.0f};
    float UiHeight{0.0f};

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

UiViewportTransform ResolveUiViewportTransform(World& Graph)
{
    UiViewportTransform Transform{};

    if (auto* GraphicsApi = Graph.Renderer().Graphics())
    {
        const auto DefaultViewportId = GraphicsApi->DefaultRenderViewportID();
        if (const auto Config = GraphicsApi->GetRenderViewportConfig(DefaultViewportId))
        {
            Transform.OutputX = Config->OutputRect.X;
            Transform.OutputY = Config->OutputRect.Y;
            Transform.OutputWidth = Config->OutputRect.Width;
            Transform.OutputHeight = Config->OutputRect.Height;
            Transform.UiWidth = static_cast<float>(Config->RenderExtent.x());
            Transform.UiHeight = static_cast<float>(Config->RenderExtent.y());
        }
    }

    if (!Transform.IsValid())
    {
        if (auto* Window = Graph.Renderer().Window())
        {
            const auto WindowSize = Window->Size();
            Transform.OutputX = 0.0f;
            Transform.OutputY = 0.0f;
            Transform.OutputWidth = WindowSize.x();
            Transform.OutputHeight = WindowSize.y();
            Transform.UiWidth = WindowSize.x();
            Transform.UiHeight = WindowSize.y();
        }
    }

    return Transform;
}

void PollRendererEvents(World& Graph, const CameraComponent* Camera, bool& Running)
{
    (void)Camera;

#if defined(SNAPI_GF_ENABLE_INPUT)
    if (Graph.Input().IsInitialized())
    {
        static bool UiLeftDown = false;
        static bool UiRightDown = false;
        static bool UiMiddleDown = false;
#if defined(SNAPI_GF_ENABLE_UI)
        static float UiDpiScaleCache = 0.0f;
        if (Graph.UI().IsInitialized())
        {
            if (const auto* Window = Graph.Renderer().Window())
            {
                if (auto* NativeWindow = reinterpret_cast<SDL_Window*>(Window->Handle()))
                {
                    const float Scale = SDL_GetWindowDisplayScale(NativeWindow);
                    if (std::isfinite(Scale) && Scale > 0.0f && std::abs(Scale - UiDpiScaleCache) > 0.0001f)
                    {
                        UiDpiScaleCache = Scale;
                        (void)Graph.UI().SetDpiScale(Scale);
                    }
                }
            }
        }
#endif

        if (const auto* Events = Graph.Input().Events())
        {
#if defined(SNAPI_GF_ENABLE_UI)
            const auto MapUiKeyCode = [](const SnAPI::Input::EKey Key) -> uint32_t {
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
            };
            const auto PushUtf8Codepoints = [&](const std::string& Text) {
                if (!Graph.UI().IsInitialized())
                {
                    return;
                }

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
                            Codepoint = ((C0 & 0x07u) << 18u) | ((C1 & 0x3Fu) << 12u) | ((C2 & 0x3Fu) << 6u) |
                                        (C3 & 0x3Fu);
                            Advance = 4;
                        }
                    }

                    SnAPI::UI::TextInputEvent UiText{};
                    UiText.Codepoint = Codepoint;
                    Graph.UI().PushInput(UiText);
                    Index += Advance;
                }
            };
            const UiViewportTransform UiTransform = ResolveUiViewportTransform(Graph);
            const auto ToUiPoint = [&](const float WindowX, const float WindowY) {
                const auto [UiX, UiY] = UiTransform.MapWindowPointToUi(WindowX, WindowY);
                return SnAPI::UI::UIPoint{UiX, UiY};
            };
#endif
            for (const auto& Event : *Events)
            {
                if (Event.Type == SnAPI::Input::EInputEventType::WindowCloseRequested)
                {
                    Running = false;
                    return;
                }

#if defined(SNAPI_GF_ENABLE_UI)
                if (!Graph.UI().IsInitialized())
                {
                    continue;
                }

                switch (Event.Type)
                {
                case SnAPI::Input::EInputEventType::MouseMove:
                    if (const auto* MoveData = std::get_if<SnAPI::Input::MouseMoveEvent>(&Event.Data))
                    {
                        SnAPI::UI::PointerEvent UiPointer{};
                        UiPointer.Position = ToUiPoint(MoveData->X, MoveData->Y);
                        UiPointer.LeftDown = UiLeftDown;
                        UiPointer.RightDown = UiRightDown;
                        UiPointer.MiddleDown = UiMiddleDown;
                        Graph.UI().PushInput(UiPointer);
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
                            UiLeftDown = Down;
                            break;
                        case SnAPI::Input::EMouseButton::Right:
                            UiRightDown = Down;
                            break;
                        case SnAPI::Input::EMouseButton::Middle:
                            UiMiddleDown = Down;
                            break;
                        default:
                            break;
                        }

                        SnAPI::UI::PointerEvent UiPointer{};
                        UiPointer.LeftDown = UiLeftDown;
                        UiPointer.RightDown = UiRightDown;
                        UiPointer.MiddleDown = UiMiddleDown;

                        if (const auto* Snapshot = Graph.Input().Snapshot())
                        {
                            UiPointer.Position = ToUiPoint(Snapshot->Mouse().X, Snapshot->Mouse().Y);
                        }

                        Graph.UI().PushInput(UiPointer);
                    }
                    break;
                case SnAPI::Input::EInputEventType::MouseWheel:
                    if (const auto* WheelData = std::get_if<SnAPI::Input::MouseWheelEvent>(&Event.Data))
                    {
                        SnAPI::UI::WheelEvent UiWheel{};
                        UiWheel.DeltaX = WheelData->DeltaX;
                        UiWheel.DeltaY = WheelData->DeltaY;
                        if (const auto* Snapshot = Graph.Input().Snapshot())
                        {
                            UiWheel.Position = ToUiPoint(Snapshot->Mouse().X, Snapshot->Mouse().Y);
                        }
                        Graph.UI().PushInput(UiWheel);
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
                        Graph.UI().PushInput(UiKey);
                    }
                    break;
                case SnAPI::Input::EInputEventType::TextInput:
                    if (const auto* TextData = std::get_if<SnAPI::Input::TextInputEvent>(&Event.Data))
                    {
                        PushUtf8Codepoints(TextData->Text);
                    }
                    break;
                default:
                    break;
                }
#endif
            }
        }

        if (const auto* Snapshot = Graph.Input().Snapshot())
        {
            UiLeftDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Left);
            UiRightDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Right);
            UiMiddleDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Middle);
        }
        return;
    }
#endif

    SDL_Event Event{};
    while (SDL_PollEvent(&Event))
    {
        if (Event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED || Event.type == SDL_EVENT_QUIT)
        {
            Running = false;
            return;
        }
    }
}

enum class ERunMode
{
    Server,
    Client,
    Local
};

const char* ModeLabel(const ERunMode Mode)
{
    switch (Mode)
    {
    case ERunMode::Server:
        return "Server";
    case ERunMode::Client:
        return "Client";
    case ERunMode::Local:
        return "Local";
    default:
        return "Unknown";
    }
}

bool ModeHasNetworking(const ERunMode Mode)
{
    return Mode != ERunMode::Local;
}

bool ModeHasWindow(const ERunMode Mode)
{
    return Mode != ERunMode::Server;
}

bool ModeUsesServerRole(const ERunMode Mode)
{
    return Mode != ERunMode::Client;
}

#if defined(SNAPI_GF_ENABLE_UI)
const char* VSyncModeLabel(const SnAPI::Graphics::EWindowVSyncMode Mode)
{
    switch (Mode)
    {
    case SnAPI::Graphics::EWindowVSyncMode::Off:
        return "Off";
    case SnAPI::Graphics::EWindowVSyncMode::On:
        return "On";
    case SnAPI::Graphics::EWindowVSyncMode::Adaptive:
        return "Adaptive";
    case SnAPI::Graphics::EWindowVSyncMode::LowLatency:
        return "LowLatency";
    default:
        return "Unknown";
    }
}

std::string FormatFloat(const float Value, const int Precision = 2)
{
    char Buffer[64]{};
    std::snprintf(Buffer, sizeof(Buffer), "%.*f", Precision, Value);
    return std::string(Buffer);
}

template <typename TElement>
TElement* ResolveUiElement(SnAPI::UI::UIContext& Context, const SnAPI::UI::ElementHandle<TElement>& Handle)
{
    if (Handle.Id.Value == 0)
    {
        return nullptr;
    }

    return &static_cast<TElement&>(Context.GetElement(Handle.Id));
}

struct MultiplayerHud
{
    static constexpr SnAPI::UI::PropertyKey TitleKey = SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Title");
    static constexpr SnAPI::UI::PropertyKey SubtitleKey =
        SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Subtitle");
    static constexpr SnAPI::UI::PropertyKey FpsKey = SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Fps");
    static constexpr SnAPI::UI::PropertyKey ModeKey = SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Mode");
    static constexpr SnAPI::UI::PropertyKey CubeKey = SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Cubes");
    static constexpr SnAPI::UI::PropertyKey SimulationKey =
        SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Simulation");
    static constexpr SnAPI::UI::PropertyKey WindowKey =
        SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Window");
    static constexpr SnAPI::UI::PropertyKey LightingKey =
        SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Lighting");
    static constexpr SnAPI::UI::PropertyKey NetworkingKey =
        SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Networking");
    static constexpr SnAPI::UI::PropertyKey HintKey = SnAPI::UI::MakePropertyKey<std::string>("MultiplayerHud.Hint");
    static constexpr SnAPI::UI::PropertyKey ActiveTabKey =
        SnAPI::UI::MakePropertyKey<std::size_t>("MultiplayerHud.ActiveTab");
    static constexpr SnAPI::UI::PropertyKey SsaoIntensityKey =
        SnAPI::UI::MakePropertyKey<float>("MultiplayerHud.SsaoIntensity");
    static constexpr SnAPI::UI::PropertyKey SsaoRadiusKey =
        SnAPI::UI::MakePropertyKey<float>("MultiplayerHud.SsaoRadius");
    static constexpr SnAPI::UI::PropertyKey SsrMaxDistanceKey =
        SnAPI::UI::MakePropertyKey<float>("MultiplayerHud.SsrMaxDistance");
    static constexpr SnAPI::UI::PropertyKey SsrMaxStepsKey =
        SnAPI::UI::MakePropertyKey<float>("MultiplayerHud.SsrMaxSteps");
    static constexpr SnAPI::UI::PropertyKey ShadowBiasKey =
        SnAPI::UI::MakePropertyKey<float>("MultiplayerHud.ShadowBias");
    static constexpr SnAPI::UI::PropertyKey ShadowSoftnessKey =
        SnAPI::UI::MakePropertyKey<float>("MultiplayerHud.ShadowSoftness");

    bool Initialized = false;
    SnAPI::UI::PropertyMap ViewModel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIDockZone> RootTabs{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> TitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SubtitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> FpsText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> ModeText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> CubeText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SimulationText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> WindowText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> LightingText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> NetworkingText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> HintText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIRealtimeGraph> PerformanceGraph{};
    uint32_t FpsSeries = SnAPI::UI::UIRealtimeGraph::InvalidSeries;
    uint32_t FrameTimeSeries = SnAPI::UI::UIRealtimeGraph::InvalidSeries;
};

namespace MultiplayerEditorThemeColors
{
constexpr SnAPI::UI::Color TitleText{255, 244, 199, 255};
constexpr SnAPI::UI::Color AccentText{102, 224, 255, 255};
constexpr SnAPI::UI::Color PrimaryText{242, 247, 255, 255};
constexpr SnAPI::UI::Color SecondaryText{194, 209, 233, 255};
constexpr SnAPI::UI::Color MutedText{124, 145, 174, 255};

constexpr SnAPI::UI::Color CardFill{16, 22, 34, 214};
constexpr SnAPI::UI::Color CardBorder{78, 142, 204, 186};
constexpr SnAPI::UI::Color CardFillPressed{24, 33, 49, 230};

constexpr SnAPI::UI::Color TabBg{9, 13, 20, 244};
constexpr SnAPI::UI::Color TabActive{22, 44, 70, 252};
constexpr SnAPI::UI::Color TabHover{17, 33, 52, 244};
constexpr SnAPI::UI::Color TabText{233, 242, 255, 255};
constexpr SnAPI::UI::Color Splitter{44, 74, 111, 224};
constexpr SnAPI::UI::Color SplitterHover{122, 192, 255, 248};
constexpr SnAPI::UI::Color DropZone{70, 166, 255, 112};
constexpr SnAPI::UI::Color DropZoneHover{100, 204, 255, 172};

constexpr SnAPI::UI::Color SliderTrack{31, 43, 62, 232};
constexpr SnAPI::UI::Color SliderFill{81, 210, 255, 248};
constexpr SnAPI::UI::Color SliderThumb{255, 255, 255, 255};
constexpr SnAPI::UI::Color SliderValueBg{9, 15, 26, 242};
constexpr SnAPI::UI::Color SliderBorder{99, 158, 216, 212};
constexpr SnAPI::UI::Color SliderThumbPressed{202, 236, 255, 255};

constexpr SnAPI::UI::Color ScrollBg{8, 13, 23, 214};
constexpr SnAPI::UI::Color ScrollTrack{24, 36, 54, 200};
constexpr SnAPI::UI::Color ScrollThumb{99, 146, 196, 232};
constexpr SnAPI::UI::Color ScrollThumbHover{140, 212, 255, 248};

constexpr SnAPI::UI::Color InputBg{8, 14, 24, 234};
constexpr SnAPI::UI::Color InputBorder{98, 153, 210, 216};
constexpr SnAPI::UI::Color InputBorderFocused{132, 222, 255, 255};
constexpr SnAPI::UI::Color InputSelection{74, 173, 255, 156};
constexpr SnAPI::UI::Color InputSpellError{255, 95, 120, 255};

constexpr SnAPI::UI::Color GraphBg{5, 8, 14, 176};
constexpr SnAPI::UI::Color GraphPlotBg{7, 14, 25, 236};
constexpr SnAPI::UI::Color GraphBorder{86, 148, 212, 142};
constexpr SnAPI::UI::Color GraphGrid{79, 114, 152, 94};
constexpr SnAPI::UI::Color GraphAxis{152, 210, 255, 182};
} // namespace MultiplayerEditorThemeColors

class MultiplayerEditorTheme final : public SnAPI::UI::Theme
{
public:
    MultiplayerEditorTheme()
        : SnAPI::UI::Theme("MultiplayerEditor")
    {
    }

    void Initialize() override
    {
        using namespace MultiplayerEditorThemeColors;

        Define<SnAPI::UI::UIText>()
            .Set(SnAPI::UI::UIText::TextColorKey, PrimaryText)
            .Set(SnAPI::UI::UIText::WrappingKey, SnAPI::UI::ETextWrapping::NoWrap)
            .Set(SnAPI::UI::UIText::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
            .Set(SnAPI::UI::UIText::TextJustifyKey, SnAPI::UI::ETextJustify::None)
            .Disabled()
                .Set(SnAPI::UI::UIText::TextColorKey, MutedText);

        Define<SnAPI::UI::UIPanel>()
            .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Vertical)
            .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
            .Set(SnAPI::UI::UIPanel::GapKey, 4.0f)
            .Set(SnAPI::UI::UIPanel::BackgroundKey, SnAPI::UI::Color{0, 0, 0, 0})
            .Set(SnAPI::UI::UIPanel::BorderColorKey, SnAPI::UI::Color{0, 0, 0, 0})
            .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f)
            .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 8.0f);

        DefineClass("hud.card")
            .Set(SnAPI::UI::UIPanel::BackgroundKey, CardFill)
            .Set(SnAPI::UI::UIPanel::BorderColorKey, CardBorder)
            .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
            .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 10.0f)
            .Set(SnAPI::UI::UIPanel::PaddingKey, 12.0f)
            .Set(SnAPI::UI::UIPanel::GapKey, 4.0f)
            .Hovered()
                .Set(SnAPI::UI::UIPanel::BorderColorKey, SnAPI::UI::Color{110, 182, 246, 218})
            .Pressed()
                .Set(SnAPI::UI::UIPanel::BackgroundKey, CardFillPressed);

        DefineClass("hud.flat_row")
            .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
            .Set(SnAPI::UI::UIPanel::PaddingKey, 0.0f)
            .Set(SnAPI::UI::UIPanel::GapKey, 8.0f)
            .Set(SnAPI::UI::UIPanel::BackgroundKey, SnAPI::UI::Color{0, 0, 0, 0})
            .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f);

        Define<SnAPI::UI::UIDockZone>()
            .Set(SnAPI::UI::UIDockZone::SplitterColorKey, Splitter)
            .Set(SnAPI::UI::UIDockZone::SplitterHoverColorKey, SplitterHover)
            .Set(SnAPI::UI::UIDockZone::TabBgColorKey, TabBg)
            .Set(SnAPI::UI::UIDockZone::TabActiveColorKey, TabActive)
            .Set(SnAPI::UI::UIDockZone::TabHoverColorKey, TabHover)
            .Set(SnAPI::UI::UIDockZone::TabTextColorKey, TabText)
            .Set(SnAPI::UI::UIDockZone::DropZoneColorKey, DropZone)
            .Set(SnAPI::UI::UIDockZone::DropZoneHoverColorKey, DropZoneHover);

        Define<SnAPI::UI::UISlider>()
            .Set(SnAPI::UI::UISlider::TrackColorKey, SliderTrack)
            .Set(SnAPI::UI::UISlider::FillColorKey, SliderFill)
            .Set(SnAPI::UI::UISlider::ThumbColorKey, SliderThumb)
            .Set(SnAPI::UI::UISlider::LabelColorKey, PrimaryText)
            .Set(SnAPI::UI::UISlider::ValueTextColorKey, SecondaryText)
            .Set(SnAPI::UI::UISlider::ValueBackgroundColorKey, SliderValueBg)
            .Set(SnAPI::UI::UISlider::BorderColorKey, SliderBorder)
            .Hovered()
                .Set(SnAPI::UI::UISlider::FillColorKey, SnAPI::UI::Color{115, 226, 255, 252})
                .Set(SnAPI::UI::UISlider::BorderColorKey, SnAPI::UI::Color{124, 198, 255, 230})
            .Pressed()
                .Set(SnAPI::UI::UISlider::ThumbColorKey, SliderThumbPressed)
                .Set(SnAPI::UI::UISlider::FillColorKey, SnAPI::UI::Color{134, 232, 255, 255})
                .Set(SnAPI::UI::UISlider::BorderColorKey, SnAPI::UI::Color{146, 218, 255, 242});

        Define<SnAPI::UI::UIRealtimeGraph>()
            .Set(SnAPI::UI::UIRealtimeGraph::BackgroundColorKey, GraphBg)
            .Set(SnAPI::UI::UIRealtimeGraph::PlotBackgroundColorKey, GraphPlotBg)
            .Set(SnAPI::UI::UIRealtimeGraph::BorderColorKey, GraphBorder)
            .Set(SnAPI::UI::UIRealtimeGraph::GridColorKey, GraphGrid)
            .Set(SnAPI::UI::UIRealtimeGraph::AxisColorKey, GraphAxis)
            .Set(SnAPI::UI::UIRealtimeGraph::TitleColorKey, AccentText)
            .Set(SnAPI::UI::UIRealtimeGraph::LegendTextColorKey, PrimaryText);

        Define<SnAPI::UI::UIScrollContainer>()
            .Set(SnAPI::UI::UIScrollContainer::BackgroundColorKey, ScrollBg)
            .Set(SnAPI::UI::UIScrollContainer::BorderColorKey, CardBorder)
            .Set(SnAPI::UI::UIScrollContainer::BorderThicknessKey, 1.0f)
            .Set(SnAPI::UI::UIScrollContainer::CornerRadiusKey, 8.0f)
            .Set(SnAPI::UI::UIScrollContainer::ScrollbarThicknessKey, 9.0f)
            .Set(SnAPI::UI::UIScrollContainer::ScrollbarTrackColorKey, ScrollTrack)
            .Set(SnAPI::UI::UIScrollContainer::ScrollbarThumbColorKey, ScrollThumb)
            .Set(SnAPI::UI::UIScrollContainer::ScrollbarThumbHoverColorKey, ScrollThumbHover)
            .Set(SnAPI::UI::UIScrollContainer::SmoothKey, true);

        Define<SnAPI::UI::UITextInput>()
            .Set(SnAPI::UI::UITextInput::BackgroundColorKey, InputBg)
            .Set(SnAPI::UI::UITextInput::BorderColorKey, InputBorder)
            .Set(SnAPI::UI::UITextInput::BorderThicknessKey, 1.0f)
            .Set(SnAPI::UI::UITextInput::CornerRadiusKey, 6.0f)
            .Set(SnAPI::UI::UITextInput::TextColorKey, PrimaryText)
            .Set(SnAPI::UI::UITextInput::PlaceholderColorKey, MutedText)
            .Set(SnAPI::UI::UITextInput::SelectionColorKey, InputSelection)
            .Set(SnAPI::UI::UITextInput::CaretColorKey, SnAPI::UI::Color{236, 246, 255, 255})
            .Set(SnAPI::UI::UITextInput::SpellErrorColorKey, InputSpellError)
            .Set(SnAPI::UI::UITextInput::PaddingKey, 8.0f)
            .Set(SnAPI::UI::UITextInput::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
            .Set(SnAPI::UI::UITextInput::TextJustifyKey, SnAPI::UI::ETextJustify::None)
            .Hovered()
                .Set(SnAPI::UI::UITextInput::BorderColorKey, SnAPI::UI::Color{108, 154, 208, 224})
            .Focused()
                .Set(SnAPI::UI::UITextInput::BorderColorKey, InputBorderFocused);
    }
};

template <typename TValue>
SnAPI::UI::TPropertyRef<TValue> HudVmProperty(MultiplayerHud& Hud, const SnAPI::UI::PropertyKey Key)
{
    return SnAPI::UI::TPropertyRef<TValue>(&Hud.ViewModel, Key);
}

bool BuildMultiplayerHud(World& Graph, const ERunMode Mode, const Args& Parsed, MultiplayerHud& OutHud)
{
    if (!Graph.UI().IsInitialized())
    {
        return false;
    }

    auto* Context = Graph.UI().Context();
    if (!Context)
    {
        return false;
    }

    auto Root = Context->Root();
    auto& RootPanel = Root.Element();
    RootPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    RootPanel.Padding().Set(0.0f);
    RootPanel.Gap().Set(0.0f);

    constexpr SnAPI::UI::Color kTitleColor = MultiplayerEditorThemeColors::TitleText;
    constexpr SnAPI::UI::Color kAccentColor = MultiplayerEditorThemeColors::AccentText;
    constexpr SnAPI::UI::Color kPrimaryTextColor = MultiplayerEditorThemeColors::PrimaryText;
    constexpr SnAPI::UI::Color kSecondaryTextColor = MultiplayerEditorThemeColors::SecondaryText;
    constexpr SnAPI::UI::Color kMutedTextColor = MultiplayerEditorThemeColors::MutedText;

    auto Tabs = Root.Add(SnAPI::UI::UIDockZone{});
    auto& TabsElement = Tabs.Element();
    TabsElement.SplitDirection().Set(SnAPI::UI::EDockSplit::Leaf);
    TabsElement.Width().Set(SnAPI::UI::Sizing::Auto());
    TabsElement.Height().Set(SnAPI::UI::Sizing::Auto());
    TabsElement.HAlign().Set(SnAPI::UI::EAlignment::Start);
    TabsElement.VAlign().Set(SnAPI::UI::EAlignment::Start);
    TabsElement.ElementMargin().Set(SnAPI::UI::Margin{12.0f, 12.0f, 0.0f, 0.0f});
    OutHud.RootTabs = Tabs.Handle();

    auto AddTextLine = [&](auto& Panel, const std::string_view Text, const SnAPI::UI::Color Color) {
        auto Line = Panel.Add(SnAPI::UI::UIText(Text));
        Line.Element().TextColorVal(Color);
        Line.Element().WrappingVal(SnAPI::UI::ETextWrapping::NoWrap);
        return Line;
    };
    auto ConfigurePanelCard = [&](SnAPI::UI::UIPanel& Panel, const float Padding, const float Gap) {
        Panel.ElementStyle().Apply("hud.card");
        Panel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        Panel.Padding().Set(Padding);
        Panel.Gap().Set(Gap);
    };
    auto AddSliderControl = [&](auto& Panel,
                                const std::string_view Label,
                                const SnAPI::UI::PropertyKey Key,
                                const float MinValue,
                                const float MaxValue,
                                const float StepValue,
                                const uint32_t DecimalPlaces,
                                const std::string_view UnitSuffix) {
        auto Row = Panel.Add(SnAPI::UI::UIPanel("Hud.SliderRow"));
        auto& RowPanel = Row.Element();
        RowPanel.ElementStyle().Apply("hud.flat_row");
        RowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        RowPanel.Padding().Set(0.0f);
        RowPanel.Gap().Set(8.0f);
        RowPanel.Width().Set(SnAPI::UI::Sizing::Auto());
        RowPanel.HAlign().Set(SnAPI::UI::EAlignment::Start);

        auto Slider = Row.Add(SnAPI::UI::UISlider(Label));
        auto& SliderElement = Slider.Element();
        SliderElement.MinValueVal(MinValue)
            .MaxValueVal(MaxValue)
            .StepVal(StepValue)
            .DecimalPlacesVal(DecimalPlaces)
            .AllowTextInputVal(true);
        SliderElement.Width().Set(SnAPI::UI::Sizing::Fixed(324.0f));
        SliderElement.Height().Set(SnAPI::UI::Sizing::Fixed(42.0f));

        auto ValueLine = Row.Add(SnAPI::UI::UIText("--"));
        ValueLine.Element().TextColorVal(kAccentColor);
        ValueLine.Element().WrappingVal(SnAPI::UI::ETextWrapping::NoWrap);
        ValueLine.Element().Width().Set(SnAPI::UI::Sizing::Fixed(90.0f));
        ValueLine.Element().HAlign().Set(SnAPI::UI::EAlignment::Start);
        ValueLine.Element().VAlign().Set(SnAPI::UI::EAlignment::Center);

        if (auto* SliderWidget = ResolveUiElement(*Context, Slider.Handle()))
        {
            auto Target = SliderWidget->Value();
            auto Source = HudVmProperty<float>(OutHud, Key);
            Target.BindTo(Source, SnAPI::UI::EBindMode::TwoWay);
        }

        if (auto* ValueText = ResolveUiElement(*Context, ValueLine.Handle()))
        {
            auto Target = ValueText->Text();
            auto Source = HudVmProperty<float>(OutHud, Key);
            const uint32_t Digits = DecimalPlaces;
            const std::string Suffix(UnitSuffix);
            Target.BindTo(Source, [Digits, Suffix](const float Value) {
                return FormatFloat(Value, static_cast<int>(Digits)) + Suffix;
            });
        }
    };

    auto StatusPanel = Tabs.Add(SnAPI::UI::UIPanel("Hud.Runtime"));
    ConfigurePanelCard(StatusPanel.Element(), 12.0f, 4.0f);

    auto Title = AddTextLine(StatusPanel, "SNAPI Multiplayer Example", kTitleColor);
    OutHud.TitleText = Title.Handle();

    auto Subtitle = AddTextLine(StatusPanel, "Traversal Sandbox Runtime", kAccentColor);
    OutHud.SubtitleText = Subtitle.Handle();

    auto FpsLine = AddTextLine(StatusPanel, "fps: --", kPrimaryTextColor);
    OutHud.FpsText = FpsLine.Handle();

    auto ModeLine = AddTextLine(StatusPanel, "mode: --", kSecondaryTextColor);
    OutHud.ModeText = ModeLine.Handle();

    auto CubeLine = AddTextLine(StatusPanel, "cubes: --", kSecondaryTextColor);
    OutHud.CubeText = CubeLine.Handle();

    auto SimLine = AddTextLine(StatusPanel, "simulation: --", kSecondaryTextColor);
    OutHud.SimulationText = SimLine.Handle();

    auto PerfGraph = StatusPanel.Add(SnAPI::UI::UIRealtimeGraph("Performance"));
    auto& PerfGraphElement = PerfGraph.Element();
    PerfGraphElement.Width().Set(SnAPI::UI::Sizing::Fixed(420.0f));
    PerfGraphElement.Height().Set(SnAPI::UI::Sizing::Fixed(168.0f));
    PerfGraphElement.HAlign().Set(SnAPI::UI::EAlignment::Start);
    PerfGraphElement.ContentPadding().Set(8.0f);
    PerfGraphElement.SampleCapacity().Set(240u);
    PerfGraphElement.AutoRange().Set(false);
    PerfGraphElement.MinValue().Set(0.0f);
    PerfGraphElement.MaxValue().Set(240.0f);
    PerfGraphElement.GridLinesX().Set(8u);
    PerfGraphElement.GridLinesY().Set(5u);
    PerfGraphElement.LineThickness().Set(1.5f);
    PerfGraphElement.ShowLegend().Set(true);
    PerfGraphElement.ValuePrecision().Set(1u);
    OutHud.PerformanceGraph = PerfGraph.Handle();

    if (auto* GraphWidget = ResolveUiElement(*Context, OutHud.PerformanceGraph))
    {
        OutHud.FpsSeries = GraphWidget->AddSeries("FPS", SnAPI::UI::Color{92, 232, 255, 255});
        OutHud.FrameTimeSeries = GraphWidget->AddSeries("Frame ms", SnAPI::UI::Color{255, 184, 96, 255});
        (void)GraphWidget->SetSeriesRange(OutHud.FpsSeries, 0.0f, 240.0f);
        (void)GraphWidget->SetSeriesRange(OutHud.FrameTimeSeries, 0.0f, 50.0f);
    }

    auto RuntimeHint = AddTextLine(StatusPanel, "Press 1/2/3/4 to switch tabs", kMutedTextColor);
    OutHud.HintText = RuntimeHint.Handle();

    auto ControlsPanel = Tabs.Add(SnAPI::UI::UIPanel("Hud.Controls"));
    ConfigurePanelCard(ControlsPanel.Element(), 12.0f, 3.0f);

    AddTextLine(ControlsPanel, "Player Controls", kTitleColor);
    AddTextLine(ControlsPanel, "Move: WASD / Left Stick", kPrimaryTextColor);
    AddTextLine(ControlsPanel, "Jump: Space / Gamepad South", kPrimaryTextColor);
    AddTextLine(ControlsPanel, "1/2/3/4: Switch tabs", kAccentColor);
    AddTextLine(ControlsPanel, "Close window to exit", kMutedTextColor);
    AddTextLine(ControlsPanel, "Editor (scroll container + text input)", kAccentColor);

    auto NotesScroll = ControlsPanel.Add(SnAPI::UI::UIScrollContainer{});
    auto& NotesScrollElement = NotesScroll.Element();
    NotesScrollElement.Height().Set(SnAPI::UI::Sizing::Fixed(190.0f));
    NotesScrollElement.Width().Set(SnAPI::UI::Sizing::Fixed(432.0f));
    NotesScrollElement.Padding().Set(6.0f);
    NotesScrollElement.Gap().Set(0.0f);
    NotesScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    NotesScrollElement.ShowHorizontalScrollbar().Set(true);
    NotesScrollElement.ShowVerticalScrollbar().Set(true);

    auto NotesInput = NotesScroll.Add(SnAPI::UI::UITextInput{
        "// Multiplayer notes\n"
        "int maxPlayers = 32;\n"
        "float netTick = 30.0f;\n"
        "bool interpolation = true;\n"
    });
    auto& NotesInputElement = NotesInput.Element();
    NotesInputElement.Multiline().Set(true);
    NotesInputElement.WordWrap().Set(false);
    NotesInputElement.Resizable().Set(true);
    NotesInputElement.EnableSpellCheck().Set(true);
    NotesInputElement.EnableSyntaxHighlight().Set(true);
    NotesInputElement.SyntaxLanguage().Set(std::string("cpp"));
    NotesInputElement.Placeholder().Set(std::string("Type gameplay or networking notes here..."));
    NotesInputElement.Width().Set(SnAPI::UI::Sizing::Fixed(392.0f));
    NotesInputElement.Height().Set(SnAPI::UI::Sizing::Fixed(148.0f));

    auto RenderingPanel = Tabs.Add(SnAPI::UI::UIPanel("Hud.Rendering"));
    ConfigurePanelCard(RenderingPanel.Element(), 12.0f, 3.0f);

    AddTextLine(RenderingPanel, "Rendering", kTitleColor);
    auto WindowLine = AddTextLine(RenderingPanel, "window: --", kSecondaryTextColor);
    OutHud.WindowText = WindowLine.Handle();
    auto LightingLine = AddTextLine(RenderingPanel, "lighting: --", kSecondaryTextColor);
    OutHud.LightingText = LightingLine.Handle();
    AddTextLine(RenderingPanel, "Real-time tuning", kAccentColor);
    AddSliderControl(RenderingPanel,
                     "SSAO Intensity",
                     MultiplayerHud::SsaoIntensityKey,
                     0.0f,
                     4.0f,
                     0.05f,
                     2,
                     "");
    AddSliderControl(RenderingPanel,
                     "SSAO Radius",
                     MultiplayerHud::SsaoRadiusKey,
                     0.1f,
                     8.0f,
                     0.05f,
                     2,
                     "");
    AddSliderControl(RenderingPanel,
                     "SSR Distance",
                     MultiplayerHud::SsrMaxDistanceKey,
                     4.0f,
                     300.0f,
                     1.0f,
                     1,
                     "");
    AddSliderControl(RenderingPanel,
                     "SSR Steps",
                     MultiplayerHud::SsrMaxStepsKey,
                     8.0f,
                     256.0f,
                     1.0f,
                     0,
                     "");
    AddSliderControl(RenderingPanel,
                     "Shadow Bias",
                     MultiplayerHud::ShadowBiasKey,
                     0.0001f,
                     0.02f,
                     0.0001f,
                     4,
                     "");
    AddSliderControl(RenderingPanel,
                     "Shadow Softness",
                     MultiplayerHud::ShadowSoftnessKey,
                     0.2f,
                     8.0f,
                     0.05f,
                     2,
                     "");
    AddTextLine(RenderingPanel, "Keyboard shortcuts", kAccentColor);
    AddTextLine(RenderingPanel, "F: Toggle fullscreen + VSync", kPrimaryTextColor);
    AddTextLine(RenderingPanel, "Z: Toggle soft shadows", kPrimaryTextColor);
    AddTextLine(RenderingPanel, "C: Toggle contact hardening", kPrimaryTextColor);

    auto NetworkingPanel = Tabs.Add(SnAPI::UI::UIPanel("Hud.Networking"));
    ConfigurePanelCard(NetworkingPanel.Element(), 12.0f, 3.0f);

    AddTextLine(NetworkingPanel, "Networking", kTitleColor);
    auto NetLine = AddTextLine(NetworkingPanel, "networking: --", kSecondaryTextColor);
    OutHud.NetworkingText = NetLine.Handle();
    AddTextLine(NetworkingPanel, "Mode switches:", kAccentColor);
    AddTextLine(NetworkingPanel, "--local", kPrimaryTextColor);
    AddTextLine(NetworkingPanel, "--server --port <p>", kPrimaryTextColor);
    AddTextLine(NetworkingPanel, "--client --connect <ip>", kPrimaryTextColor);

    auto BindText = [&](const SnAPI::UI::ElementHandle<SnAPI::UI::UIText>& Handle, const SnAPI::UI::PropertyKey Key) {
        if (auto* TextElement = ResolveUiElement(*Context, Handle))
        {
            auto Target = TextElement->Text();
            auto Source = HudVmProperty<std::string>(OutHud, Key);
            Target.BindTo(Source, SnAPI::UI::EBindMode::OneWay);
        }
    };

    BindText(OutHud.TitleText, MultiplayerHud::TitleKey);
    BindText(OutHud.SubtitleText, MultiplayerHud::SubtitleKey);
    BindText(OutHud.FpsText, MultiplayerHud::FpsKey);
    BindText(OutHud.ModeText, MultiplayerHud::ModeKey);
    BindText(OutHud.CubeText, MultiplayerHud::CubeKey);
    BindText(OutHud.SimulationText, MultiplayerHud::SimulationKey);
    BindText(OutHud.WindowText, MultiplayerHud::WindowKey);
    BindText(OutHud.LightingText, MultiplayerHud::LightingKey);
    BindText(OutHud.NetworkingText, MultiplayerHud::NetworkingKey);
    BindText(OutHud.HintText, MultiplayerHud::HintKey);

    {
        auto TabsActive = TabsElement.ActiveTab();
        auto VmActive = HudVmProperty<std::size_t>(OutHud, MultiplayerHud::ActiveTabKey);
        TabsActive.BindTo(VmActive, SnAPI::UI::EBindMode::TwoWay);
    }

    float SsaoIntensity = 1.0f;
    float SsaoRadius = 2.0f;
    float SsrMaxDistance = 90.0f;
    float SsrMaxSteps = 64.0f;
    float ShadowBias = SnAPI::Graphics::DirectionalLightContract::DefaultShadowBias;
    float ShadowSoftness = SnAPI::Graphics::DirectionalLightContract::DefaultSoftnessFactor;

    if (auto* GraphicsApi = Graph.Renderer().Graphics())
    {
        if (auto* SsaoPass =
                dynamic_cast<SnAPI::Graphics::SSAOPass*>(GraphicsApi->GetRenderPass(SnAPI::Graphics::ERenderPassType::SSAO)))
        {
            SsaoIntensity = SsaoPass->GetIntensity();
            SsaoRadius = SsaoPass->GetRadius();
        }

        if (auto* SsrPass =
                dynamic_cast<SnAPI::Graphics::SSRPass*>(GraphicsApi->GetRenderPass(SnAPI::Graphics::ERenderPassType::SSR)))
        {
            SsrMaxDistance = SsrPass->GetMaxDistance();
            SsrMaxSteps = static_cast<float>(SsrPass->GetMaxSteps());
        }
    }

    if (const auto* LightManager = Graph.Renderer().LightManager())
    {
        const auto DirectionalLights = LightManager->GetDirectionalLights();
        if (const auto FirstLight = DirectionalLights.begin(); FirstLight != DirectionalLights.end())
        {
            ShadowBias = (*FirstLight)->GetShadowBias();
            ShadowSoftness = (*FirstLight)->GetSoftnessFactor();
        }
    }

    HudVmProperty<std::string>(OutHud, MultiplayerHud::TitleKey).Set("SNAPI Multiplayer Example");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::SubtitleKey)
        .Set(std::string("mode: ") + ModeLabel(Mode) + " | target_hz=" + FormatFloat(Parsed.FixedHz, 1));
    HudVmProperty<std::string>(OutHud, MultiplayerHud::FpsKey).Set("fps: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::ModeKey).Set("mode: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::CubeKey).Set("cubes: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::SimulationKey).Set("simulation: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::WindowKey).Set("window: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::LightingKey).Set("lighting: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::NetworkingKey).Set("networking: --");
    HudVmProperty<std::string>(OutHud, MultiplayerHud::HintKey).Set("Press 1/2/3/4 to switch tabs");
    HudVmProperty<std::size_t>(OutHud, MultiplayerHud::ActiveTabKey).Set(size_t{0});
    HudVmProperty<float>(OutHud, MultiplayerHud::SsaoIntensityKey).Set(SsaoIntensity);
    HudVmProperty<float>(OutHud, MultiplayerHud::SsaoRadiusKey).Set(SsaoRadius);
    HudVmProperty<float>(OutHud, MultiplayerHud::SsrMaxDistanceKey).Set(SsrMaxDistance);
    HudVmProperty<float>(OutHud, MultiplayerHud::SsrMaxStepsKey).Set(SsrMaxSteps);
    HudVmProperty<float>(OutHud, MultiplayerHud::ShadowBiasKey).Set(ShadowBias);
    HudVmProperty<float>(OutHud, MultiplayerHud::ShadowSoftnessKey).Set(ShadowSoftness);

    OutHud.Initialized = true;
    return true;
}

void SetMultiplayerHudTab(const World& Graph, MultiplayerHud& Hud, const size_t TabIndex)
{
    if (!Hud.Initialized)
    {
        return;
    }

    (void)Graph;
    HudVmProperty<std::size_t>(Hud, MultiplayerHud::ActiveTabKey).Set(TabIndex);
}

void UpdateMultiplayerHud(World& Graph,
                          const ERunMode Mode,
                          const Args& Parsed,
                          MultiplayerHud& Hud,
                          const float DeltaSeconds,
                          const std::size_t ActiveCubeCount,
                          const std::size_t TotalCubeCount,
                          const NetSession* Session)
{
    if (!Hud.Initialized || !Graph.UI().IsInitialized())
    {
        return;
    }

    auto* Context = Graph.UI().Context();
    if (!Context)
    {
        return;
    }

    if (auto* Window = Graph.Renderer().Window())
    {
        const UiViewportTransform UiTransform = ResolveUiViewportTransform(Graph);
        if (UiTransform.IsValid())
        {
            (void)Graph.UI().SetViewportSize(UiTransform.UiWidth, UiTransform.UiHeight);
        }
        else
        {
            const auto WindowSize = Window->Size();
            if (WindowSize.x() > 1.0f && WindowSize.y() > 1.0f)
            {
                (void)Graph.UI().SetViewportSize(WindowSize.x(), WindowSize.y());
            }
        }
    }

    const float SafeDelta = std::max(DeltaSeconds, 0.0001f);
    const int Fps = static_cast<int>(std::round(1.0f / SafeDelta));

    const auto ClampHudFloat = [](const float Value, const float MinValue, const float MaxValue) {
        return std::clamp(std::isfinite(Value) ? Value : MinValue, MinValue, MaxValue);
    };

    const float SsaoIntensity =
        ClampHudFloat(HudVmProperty<float>(Hud, MultiplayerHud::SsaoIntensityKey).Get(), 0.0f, 4.0f);
    const float SsaoRadius = ClampHudFloat(HudVmProperty<float>(Hud, MultiplayerHud::SsaoRadiusKey).Get(), 0.1f, 8.0f);
    const float SsrMaxDistance =
        ClampHudFloat(HudVmProperty<float>(Hud, MultiplayerHud::SsrMaxDistanceKey).Get(), 4.0f, 300.0f);
    const float SsrMaxStepsFloat =
        ClampHudFloat(HudVmProperty<float>(Hud, MultiplayerHud::SsrMaxStepsKey).Get(), 8.0f, 256.0f);
    const auto SsrMaxSteps = static_cast<uint32_t>(std::round(SsrMaxStepsFloat));
    const float ShadowBias =
        ClampHudFloat(HudVmProperty<float>(Hud, MultiplayerHud::ShadowBiasKey).Get(), 0.0001f, 0.02f);
    const float ShadowSoftness =
        ClampHudFloat(HudVmProperty<float>(Hud, MultiplayerHud::ShadowSoftnessKey).Get(), 0.2f, 8.0f);

    if (!std::isfinite(HudVmProperty<float>(Hud, MultiplayerHud::SsrMaxStepsKey).Get()) ||
        std::abs(HudVmProperty<float>(Hud, MultiplayerHud::SsrMaxStepsKey).Get() - static_cast<float>(SsrMaxSteps)) >
            0.0001f)
    {
        HudVmProperty<float>(Hud, MultiplayerHud::SsrMaxStepsKey).Set(static_cast<float>(SsrMaxSteps));
    }

    if (auto* GraphicsApi = Graph.Renderer().Graphics())
    {
        if (auto* SsaoPass =
                dynamic_cast<SnAPI::Graphics::SSAOPass*>(GraphicsApi->GetRenderPass(SnAPI::Graphics::ERenderPassType::SSAO)))
        {
            SsaoPass->SetIntensity(SsaoIntensity);
            SsaoPass->SetRadius(SsaoRadius);
        }

        if (auto* SsrPass =
                dynamic_cast<SnAPI::Graphics::SSRPass*>(GraphicsApi->GetRenderPass(SnAPI::Graphics::ERenderPassType::SSR)))
        {
            SsrPass->SetMaxDistance(SsrMaxDistance);
            SsrPass->SetMaxSteps(SsrMaxSteps);
        }
    }

    bool SoftShadowsEnabled = false;
    bool ContactHardeningEnabled = false;
    if (const auto* LightManager = Graph.Renderer().LightManager())
    {
        const auto DirectionalLights = LightManager->GetDirectionalLights();
        if (const auto FirstLight = DirectionalLights.begin(); FirstLight != DirectionalLights.end())
        {
            (*FirstLight)->SetShadowBias(ShadowBias);
            (*FirstLight)->SetSoftnessFactor(ShadowSoftness);
            SoftShadowsEnabled = (*FirstLight)->HasFeature(SnAPI::Graphics::DirectionalLightContract::Feature::SoftShadows);
            ContactHardeningEnabled = (*FirstLight)->HasFeature(SnAPI::Graphics::DirectionalLightContract::Feature::ContactHardening);
        }
    }

    std::string NetworkingText = "networking: disabled";
    if (Session)
    {
        const auto Dumps = Session->DumpConnections(Clock::now());
        const auto ReadyConnections = std::ranges::count_if(Dumps, [](const NetConnectionDump& Dump) {
            return Dump.HandshakeComplete;
        });
        NetworkingText = "networking: conns=" + std::to_string(Dumps.size()) + " ready=" + std::to_string(ReadyConnections);
    }

    HudVmProperty<std::string>(Hud, MultiplayerHud::FpsKey)
        .Set("fps: " + std::to_string(Fps) + " | dt: " + FormatFloat(SafeDelta * 1000.0f) + " ms");
    HudVmProperty<std::string>(Hud, MultiplayerHud::ModeKey)
        .Set(std::string("mode: ") + ModeLabel(Mode) +
             " | hz: " + FormatFloat(Parsed.FixedHz, 1) +
             " | max_steps: " + std::to_string(Parsed.MaxFixedSteps));
    HudVmProperty<std::string>(Hud, MultiplayerHud::CubeKey)
        .Set("cubes: " + std::to_string(ActiveCubeCount) + " / " + std::to_string(TotalCubeCount));
    HudVmProperty<std::string>(Hud, MultiplayerHud::SimulationKey)
        .Set(std::string("simulation: interp=") + (Parsed.DisableInterpolation ? "off" : "on") +
             " | reset=" + (Parsed.ResetSimulation ? "on" : "off") +
             " | substeps=" + (Parsed.MaxSubStepping ? std::to_string(*Parsed.MaxSubStepping) : std::string("auto")));

    const auto* Window = Graph.Renderer().Window();
    const std::string Fullscreen = (Window && Window->FullScreen()) ? "fullscreen" : "windowed";
    const std::string VSync = Window ? VSyncModeLabel(Window->VSyncMode()) : "Unknown";
    HudVmProperty<std::string>(Hud, MultiplayerHud::WindowKey).Set("window: " + Fullscreen + " | vsync: " + VSync);
    HudVmProperty<std::string>(Hud, MultiplayerHud::LightingKey)
        .Set(std::string("lighting: soft=") + (SoftShadowsEnabled ? "on" : "off") +
             " | contact=" + (ContactHardeningEnabled ? "on" : "off") +
             " | bias=" + FormatFloat(ShadowBias, 4) +
             " | softness=" + FormatFloat(ShadowSoftness, 2) +
             " | cubes=" + (Parsed.CubeShadows ? "on" : "off"));
    HudVmProperty<std::string>(Hud, MultiplayerHud::NetworkingKey).Set(NetworkingText);

    if (auto* GraphWidget = ResolveUiElement(*Context, Hud.PerformanceGraph))
    {
        if (Hud.FpsSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
        {
            (void)GraphWidget->PushSample(Hud.FpsSeries, static_cast<float>(Fps));
        }
        if (Hud.FrameTimeSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
        {
            (void)GraphWidget->PushSample(Hud.FrameTimeSeries, SafeDelta * 1000.0f);
        }
    }
}
#endif

int RunMode(const Args& Parsed, const ERunMode Mode)
{
    RegisterExampleTypes();

    const bool NetworkingEnabled = ModeHasNetworking(Mode);
    const bool WindowEnabled = ModeHasWindow(Mode);
    const bool ServerRole = ModeUsesServerRole(Mode);

    SessionListener Listener(ModeLabel(Mode));

#if defined(SNAPI_GF_ENABLE_UI)
    MultiplayerEditorTheme HudTheme{};
#endif

    GameRuntime Runtime;
    if (auto InitResult = Runtime.Init(MakeRuntimeSettings(Parsed,
                                                            ServerRole,
                                                            NetworkingEnabled ? &Listener : nullptr,
                                                            NetworkingEnabled,
                                                            WindowEnabled));
        !InitResult)
    {
        std::cerr << "Failed to initialize runtime: " << InitResult.error().Message << "\n";
        return 1;
    }

#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER && \
    defined(SNAPI_PROFILER_ENABLE_REALTIME_STREAM) && SNAPI_PROFILER_ENABLE_REALTIME_STREAM
    ConfigureProfilerStreamForMultiplayerExample();
#endif

    auto& Graph = Runtime.World();

#if defined(SNAPI_GF_ENABLE_UI)
    if (WindowEnabled && Graph.UI().IsInitialized())
    {
        HudTheme.Initialize();
        if (auto* UiContext = Graph.UI().Context())
        {
            UiContext->SetActiveTheme(&HudTheme);
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    NetSession* Session = nullptr;
    if (NetworkingEnabled)
    {
        Session = Graph.Networking().Session();
        if (!Session)
        {
            std::cerr << "Runtime networking session is not available\n";
            return 1;
        }
        Listener.SessionRef = Session;
    }
#else
    NetSession* Session = nullptr;
    (void)NetworkingEnabled;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (!Graph.Physics().IsInitialized())
    {
        std::cerr << "Runtime physics scene is not available\n";
        return 1;
    }
    SpawnPhysicsGround(Graph);

    PlayerActor Player = CreatePlayerActor(Graph, WindowEnabled, Parsed.DisableInterpolation);
    if (Player.Handle.IsNull() || !Player.Transform || !Player.Body || !Player.Movement)
    {
        std::cerr << "Failed to create controllable player actor\n";
        return 1;
    }
#if defined(SNAPI_GF_ENABLE_INPUT)
    if (WindowEnabled && !Player.Input)
    {
        std::cerr << "Failed to attach InputComponent to player\n";
        return 1;
    }
#endif
#else
    std::cerr << "This example requires physics support in this build\n";
    return 1;
#endif

    CameraComponent* Camera = nullptr;
    if (WindowEnabled)
    {
        CameraComponent* FollowCamera = nullptr;
        if (!Graph.Renderer().IsInitialized() || !Graph.Renderer().HasOpenWindow())
        {
            std::cerr << "Renderer window was not initialized\n";
            return 1;
        }

        FollowCamera = CreateViewCamera(Graph, Player.Handle);
        //Camera = CreateStationaryObserverCamera(Graph);
        if (!FollowCamera && !Camera)
        {
            std::cerr << "Failed to create debug cameras\n";
            return 1;
        }
        if (!Camera)
        {
            Camera = FollowCamera;
        }
        else if (FollowCamera)
        {
            FollowCamera->SetActive(false);
            Camera->SetActive(true);
        }

        (void)Graph.Renderer().LoadDefaultFont("/usr/share/fonts/TTF/Arial.TTF", 20);
    }

    const bool BuildLocalCubePool = (Mode != ERunMode::Client);
    std::vector<CubeSlot> CubeSlots;
    if (BuildLocalCubePool)
    {
        CubeSlots.reserve(static_cast<size_t>(std::max(Parsed.CubeCount, 1)));
    }
    CubeSharedMaterials SharedCubeMaterials{};
    if (BuildLocalCubePool && WindowEnabled && Graph.Renderer().IsInitialized())
    {
        SharedCubeMaterials = BuildSharedCubeMaterialInstances(Graph, Parsed.CubeShadows);
    }
    std::mt19937 Rng(static_cast<std::uint32_t>(Clock::now().time_since_epoch().count()));
    if (BuildLocalCubePool)
    {
        for (int i = 0; i < Parsed.CubeCount; ++i)
        {
            CubeSlot Slot{};
            if (CreateCubeSlot(Graph,
                               i,
                               Slot,
                               Parsed.CubeShadows,
                               Parsed.DisableInterpolation,
                               SharedCubeMaterials.GBuffer,
                               SharedCubeMaterials.Shadow))
            {
                CubeSlots.push_back(Slot);
            }
        }
    }

    if (Mode == ERunMode::Server)
    {
        std::cout << "Server listening on " << Parsed.Bind << ":" << Parsed.Port
                  << ", reset=" << (Parsed.ResetSimulation ? "on" : "off")
                  << ", fixed_hz=" << Parsed.FixedHz
                  << ", max_fixed_steps=" << Parsed.MaxFixedSteps
                  << ", cube_shadows=" << (Parsed.CubeShadows ? "on" : "off")
                  << ", substeps=" << (Parsed.MaxSubStepping ? std::to_string(*Parsed.MaxSubStepping) : std::string("auto")) << "\n";
    }
    else if (Mode == ERunMode::Local)
    {
        std::cout << "Local mode: networking disabled, cubes=" << Parsed.CubeCount
                  << ", interp=" << (Parsed.DisableInterpolation ? "off" : "on")
                  << ", reset=" << (Parsed.ResetSimulation ? "on" : "off")
                  << ", fixed_hz=" << Parsed.FixedHz
                  << ", max_fixed_steps=" << Parsed.MaxFixedSteps
                  << ", cube_shadows=" << (Parsed.CubeShadows ? "on" : "off")
                  << ", substeps=" << (Parsed.MaxSubStepping ? std::to_string(*Parsed.MaxSubStepping) : std::string("auto")) << "\n";
    }

    float SpawnAccumulator = 0.0f;
    constexpr float CubeLifetimeSeconds = 5.0f;
    constexpr float MinInactiveSeconds = 1.0f / 60.0f;
    const float SpawnIntervalSeconds = CubeLifetimeSeconds / static_cast<float>(std::max(Parsed.CubeCount, 1));
    const float SpawnHalfExtent = std::max(10.0f, std::sqrt(static_cast<float>(std::max(Parsed.CubeCount, 1))) * 1.5f);

    const auto Start = Clock::now();
    auto Previous = Start;
    auto NextLog = Start;
    auto NextPerfLog = Start + std::chrono::seconds(1);
    std::uint32_t FramesSincePerfLog = 0;

#if defined(SNAPI_GF_ENABLE_UI)
    MultiplayerHud Hud{};
    if (WindowEnabled)
    {
        if (!BuildMultiplayerHud(Graph, Mode, Parsed, Hud))
        {
            std::cerr << "Warning: failed to build HUD UI; continuing without UI overlay\n";
        }
    }
#endif

    
    bool Running = true;
    while (Running)
    {
        if (WindowEnabled && !Graph.Renderer().HasOpenWindow())
        {
            break;
        }

        const auto Now = Clock::now();
        const float DeltaSeconds = std::chrono::duration<float>(Now - Previous).count();
        Previous = Now;
        const float TimeSeconds = std::chrono::duration<float>(Now - Start).count();

        if (Graph.IsServer())
        {
            constexpr int MaxActivationsPerFrame = 64;
            if (Parsed.ResetSimulation)
            {
                for (auto& Slot : CubeSlots)
                {
                    if (!Slot.Active)
                    {
                        continue;
                    }
                    if ((TimeSeconds - Slot.SpawnedAtSeconds) >= CubeLifetimeSeconds)
                    {
                        DeactivateCubeSlot(Slot, TimeSeconds);
                    }
                }
            }

            SpawnAccumulator += std::max(0.0f, DeltaSeconds);
            const float MaxSpawnBacklog = SpawnIntervalSeconds * static_cast<float>(MaxActivationsPerFrame);
            if (SpawnAccumulator > MaxSpawnBacklog)
            {
                SpawnAccumulator = MaxSpawnBacklog;
            }

            int ActivationsThisFrame = 0;
            while (SpawnAccumulator >= SpawnIntervalSeconds && ActivationsThisFrame < MaxActivationsPerFrame)
            {
                auto SlotIt = std::ranges::find_if(CubeSlots, [&](const CubeSlot& Slot) {
                    if (Slot.Active)
                    {
                        return false;
                    }
                    if (!Parsed.ResetSimulation)
                    {
                        return true;
                    }
                    return (TimeSeconds - Slot.SpawnedAtSeconds) >= MinInactiveSeconds;
                });
                if (SlotIt == CubeSlots.end())
                {
                    SpawnAccumulator = 0.0f;
                    break;
                }
                SpawnAccumulator -= SpawnIntervalSeconds;
                ActivateCubeSlot(*SlotIt, TimeSeconds, Rng, SpawnHalfExtent);
                ++ActivationsThisFrame;
            }
        }

        std::size_t ActiveCubes = static_cast<std::size_t>(std::ranges::count_if(CubeSlots, [](const CubeSlot& Slot) {
            return Slot.Active;
        }));
        std::size_t TotalCubes = CubeSlots.size();
        if (!BuildLocalCubePool)
        {
            ActiveCubes = CountActiveRenderedCubes(Graph);
            TotalCubes = ActiveCubes;
        }

#if defined(SNAPI_GF_ENABLE_UI)
        if (WindowEnabled)
        {
            UpdateMultiplayerHud(Graph,
                                 Mode,
                                 Parsed,
                                 Hud,
                                 DeltaSeconds,
                                 ActiveCubes,
                                 TotalCubes,
                                 Session);

        }
#endif

#if !defined(SNAPI_GF_ENABLE_UI)
        if (WindowEnabled)
        {
            const float SafeDelta = std::max(DeltaSeconds, 0.0001f);
            const int Fps = static_cast<int>(std::round(1.0f / SafeDelta));
            const std::string FpsText = "FPS: " + std::to_string(Fps);
            (void)Graph.Renderer().QueueText(FpsText, 16.0f, 16.0f);
        }
#endif

        Runtime.Update(DeltaSeconds);

#if defined(SNAPI_GF_ENABLE_INPUT)
        const SnAPI::Input::InputSnapshot* InputSnapshot = nullptr;
        if (WindowEnabled && Runtime.World().Input().IsInitialized())
        {
            InputSnapshot = Runtime.World().Input().Snapshot();
        }
        if (InputSnapshot)
        {
            // Window-only debug toggles.
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::F))
            {
                Runtime.World().Renderer().Window()->FullScreen(!Runtime.World().Renderer().Window()->FullScreen());
                const bool IsVSyncEnabled =
                    Runtime.World().Renderer().Window()->VSyncMode() != SnAPI::Graphics::EWindowVSyncMode::Off;
                Runtime.World().Renderer().Window()->VSyncMode(
                    IsVSyncEnabled ? SnAPI::Graphics::EWindowVSyncMode::Off : SnAPI::Graphics::EWindowVSyncMode::On);
            }
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::Z))
            {
                const auto DirLights = Runtime.World().Renderer().LightManager()->GetDirectionalLights();
                if (const auto FirstLight = DirLights.begin(); FirstLight != DirLights.end())
                {
                    (*FirstLight)
                        ->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::SoftShadows,
                                     !(*FirstLight)->HasFeature(
                                         SnAPI::Graphics::DirectionalLightContract::Feature::SoftShadows));
                    (*FirstLight)->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::ContactHardening,
                                              true);
                }
            }
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::C))
            {
                const auto DirLights = Runtime.World().Renderer().LightManager()->GetDirectionalLights();
                if (const auto FirstLight = DirLights.begin(); FirstLight != DirLights.end())
                {
                    (*FirstLight)
                        ->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::ContactHardening,
                                     !(*FirstLight)->HasFeature(
                                         SnAPI::Graphics::DirectionalLightContract::Feature::ContactHardening));
                }
            }
#if defined(SNAPI_GF_ENABLE_UI)
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::Num1) ||
                InputSnapshot->KeyReleased(SnAPI::Input::EKey::Numpad1))
            {
                SetMultiplayerHudTab(Runtime.World(), Hud, size_t{0});
            }
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::Num2) ||
                InputSnapshot->KeyReleased(SnAPI::Input::EKey::Numpad2))
            {
                SetMultiplayerHudTab(Runtime.World(), Hud, size_t{1});
            }
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::Num3) ||
                InputSnapshot->KeyReleased(SnAPI::Input::EKey::Numpad3))
            {
                SetMultiplayerHudTab(Runtime.World(), Hud, size_t{2});
            }
            if (InputSnapshot->KeyReleased(SnAPI::Input::EKey::Num4) ||
                InputSnapshot->KeyReleased(SnAPI::Input::EKey::Numpad4))
            {
                SetMultiplayerHudTab(Runtime.World(), Hud, size_t{3});
            }
#endif
        }
#endif
        if (Session && Now >= NextLog)
        {
            for (const auto Dumps = Session->DumpConnections(Now); const auto& Dump : Dumps)
            {
                PrintConnectionDump(ModeLabel(Mode), Dump, TimeSeconds);
            }
            NextLog = Now + std::chrono::seconds(1);
        }

        if (WindowEnabled)
        {
            PollRendererEvents(Graph, Camera, Running);

            ++FramesSincePerfLog;
            if (Now >= NextPerfLog)
            {
                std::cout << "[" << ModeLabel(Mode) << "Perf] fps=" << FramesSincePerfLog
                          << " active_cubes=" << ActiveCubes
                          << " total_cubes=" << TotalCubes
                          << " interp=" << (Parsed.DisableInterpolation ? "off" : "on")
                          << " fixed_hz=" << Parsed.FixedHz
                          << " substeps=" << (Parsed.MaxSubStepping ? std::to_string(*Parsed.MaxSubStepping) : std::string("auto")) << "\n";

                FramesSincePerfLog = 0;
                NextPerfLog = Now + std::chrono::seconds(1);
            }
        }
    }

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (Mode == ERunMode::Client && Session)
    {
        if (const auto Connection = Graph.Networking().PrimaryConnection())
        {
            Session->CloseConnection(*Connection);
        }
    }
#endif

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    InitializeWorkingDirectory((argc > 0) ? argv[0] : nullptr);

    Args Parsed;
    if (!ParseArgs(argc, argv, Parsed))
    {
        PrintUsage(argv[0]);
        return 1;
    }

    if (Parsed.Server)
    {
        return RunMode(Parsed, ERunMode::Server);
    }

    if (Parsed.Local)
    {
        return RunMode(Parsed, ERunMode::Local);
    }

    return RunMode(Parsed, ERunMode::Client);
}

#endif // SNAPI_GF_ENABLE_RENDERER
