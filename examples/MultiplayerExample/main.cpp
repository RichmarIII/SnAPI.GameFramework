#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "GameFramework.hpp"
#include "VulkanGraphicsAPI.hpp"
#include <MeshManager.hpp>
#include <CameraBase.hpp>
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
    float FixedHz = 60.0f;
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
            auto Dump = SessionRef->DumpConnection(Event.Handle, Clock::now());
            if (Dump)
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
        InputSettings.CreateDesc.EnableTextInput = false;
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

    return Settings;
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)
struct BodyRenderInterpolationState
{
    Vec3 PreviousPosition = Vec3::Zero();
    Vec3 CurrentPosition = Vec3::Zero();
    Quat PreviousRotation = Quat::Identity();
    Quat CurrentRotation = Quat::Identity();
    float SecondsSinceSample = 0.0f;
    float StepSeconds = 1.0f / 60.0f;
    bool HasSample = false;
};
#endif

struct CubeSlot
{
    NodeHandle Handle{};
    TransformComponent* Transform = nullptr;
    StaticMeshComponent* Mesh = nullptr;
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    RigidBodyComponent* Body = nullptr;
    Vec3 ParkPosition{};
    BodyRenderInterpolationState Interpolation{};
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

#if defined(SNAPI_GF_ENABLE_PHYSICS)
using PlayerRenderInterpolationState = BodyRenderInterpolationState;
#endif

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
        const auto ShadowMaterial = Graph.Renderer().DefaultShadowMaterial();
        if (ShadowMaterial)
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
        Transform->Scale = Vec3(128.0f, 2.0f, 128.0f);
    }

    if (auto Collider = GroundNode->Add<ColliderComponent>())
    {
        Collider->Replicated(true);
        auto& Settings = Collider->EditSettings();
        Settings.Shape = SnAPI::Physics::EShapeType::Box;
        Settings.HalfExtent = Vec3(64.0f, 1.0f, 64.0f);
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
    Settings.SyncFromPhysics = false;
    Settings.SyncToPhysics = true;
    if (auto RigidBody = Node->Add<RigidBodyComponent>(Settings))
    {
        (void)RigidBody->Teleport(OutSlot.ParkPosition, Quat::Identity(), true);
        OutSlot.Body = &*RigidBody;
    }

    OutSlot.Handle = Node->Handle();
    OutSlot.Transform = Transform;
    OutSlot.Active = false;
    OutSlot.SpawnedAtSeconds = 0.0f;
    OutSlot.Interpolation.PreviousPosition = OutSlot.ParkPosition;
    OutSlot.Interpolation.CurrentPosition = OutSlot.ParkPosition;
    OutSlot.Interpolation.PreviousRotation = Quat::Identity();
    OutSlot.Interpolation.CurrentRotation = Quat::Identity();
    OutSlot.Interpolation.SecondsSinceSample = OutSlot.Interpolation.StepSeconds;
    OutSlot.Interpolation.HasSample = true;
    return true;
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
    Slot.Interpolation.PreviousPosition = SpawnPosition;
    Slot.Interpolation.CurrentPosition = SpawnPosition;
    Slot.Interpolation.PreviousRotation = Quat::Identity();
    Slot.Interpolation.CurrentRotation = Quat::Identity();
    Slot.Interpolation.SecondsSinceSample = Slot.Interpolation.StepSeconds;
    Slot.Interpolation.HasSample = true;

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
    Slot.Interpolation.PreviousPosition = Slot.ParkPosition;
    Slot.Interpolation.CurrentPosition = Slot.ParkPosition;
    Slot.Interpolation.PreviousRotation = Quat::Identity();
    Slot.Interpolation.CurrentRotation = Quat::Identity();
    Slot.Interpolation.SecondsSinceSample = Slot.Interpolation.StepSeconds;
    Slot.Interpolation.HasSample = true;
    Slot.Active = false;
    Slot.SpawnedAtSeconds = TimeSeconds;
}

PlayerActor CreatePlayerActor(World& Graph, const bool EnableInput)
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
    // Player rendering uses explicit interpolation from body samples to avoid fixed-step snap jitter.
    BodySettings.SyncFromPhysics = false;
    BodySettings.SyncToPhysics = true;
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

TransformComponent* ResolveTransform(NodeHandle Handle)
{
    auto* Node = Handle.Borrowed();
    if (!Node)
    {
        return nullptr;
    }

    auto TransformResult = Node->Component<TransformComponent>();
    if (!TransformResult)
    {
        return nullptr;
    }

    return &*TransformResult;
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)
Quat NormalizeQuatOrIdentity(const Quat& Rotation)
{
    Quat Out = Rotation;
    if (Out.squaredNorm() > static_cast<Quat::Scalar>(0))
    {
        Out.normalize();
    }
    else
    {
        Out = Quat::Identity();
    }
    return Out;
}

Quat FromPhysicsQuat(const SnAPI::Physics::Quat& Rotation)
{
    Quat Out = Quat::Identity();
    Out.x() = static_cast<Quat::Scalar>(Rotation.x());
    Out.y() = static_cast<Quat::Scalar>(Rotation.y());
    Out.z() = static_cast<Quat::Scalar>(Rotation.z());
    Out.w() = static_cast<Quat::Scalar>(Rotation.w());
    return NormalizeQuatOrIdentity(Out);
}

Quat NlerpShortestPath(const Quat& A, const Quat& B, const float Alpha)
{
    const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);
    Quat End = B;
    if (A.dot(B) < static_cast<Quat::Scalar>(0))
    {
        End.coeffs() = -End.coeffs();
    }

    Quat Out = Quat::Identity();
    Out.coeffs() = (A.coeffs() * static_cast<Quat::Scalar>(1.0f - ClampedAlpha))
                 + (End.coeffs() * static_cast<Quat::Scalar>(ClampedAlpha));
    return NormalizeQuatOrIdentity(Out);
}

void InitializeInterpolationState(BodyRenderInterpolationState& State, const float FixedHz)
{
    const float SafeFixedHz = std::max(FixedHz, 1.0f);
    State.StepSeconds = 1.0f / SafeFixedHz;
    State.SecondsSinceSample = State.StepSeconds;
    State.HasSample = false;
}

void SeedInterpolationState(BodyRenderInterpolationState& State, const Vec3& Position, const Quat& Rotation)
{
    State.PreviousPosition = Position;
    State.CurrentPosition = Position;
    State.PreviousRotation = Rotation;
    State.CurrentRotation = Rotation;
    State.SecondsSinceSample = State.StepSeconds;
    State.HasSample = true;
}

bool SampleBodyPoseWorld(World& Graph, const RigidBodyComponent* Body, Vec3& OutPosition, Quat& OutRotation)
{
    if (!Body || !Body->HasBody())
    {
        return false;
    }

    auto* Scene = Graph.Physics().Scene();
    if (!Scene)
    {
        return false;
    }

    const auto BodyTransform = Scene->Rigid().BodyTransform(Body->PhysicsBodyHandle());
    if (!BodyTransform)
    {
        return false;
    }

    const SnAPI::Physics::Vec3 PhysicsPosition = SnAPI::Physics::TransformPosition(*BodyTransform);
    const SnAPI::Physics::Quat PhysicsRotation = SnAPI::Physics::TransformRotation(*BodyTransform);
    OutPosition = Graph.Physics().PhysicsToWorldPosition(PhysicsPosition);
    OutRotation = FromPhysicsQuat(PhysicsRotation);
    return true;
}

void AdvanceInterpolationState(BodyRenderInterpolationState& State,
                               const Vec3& Position,
                               const Quat& Rotation,
                               const float DeltaSeconds)
{
    constexpr double PositionDeltaEpsilonSq = 1.0e-8;
    constexpr double RotationDeltaDotThreshold = 0.9999995;
    if (!State.HasSample)
    {
        SeedInterpolationState(State, Position, Rotation);
        return;
    }

    const double PositionDeltaSq = (Position - State.CurrentPosition).squaredNorm();
    const double RotationDot = std::abs(static_cast<double>(Rotation.dot(State.CurrentRotation)));
    const bool HasAdvancedSample =
        PositionDeltaSq > PositionDeltaEpsilonSq || RotationDot < RotationDeltaDotThreshold;

    if (HasAdvancedSample)
    {
        State.PreviousPosition = State.CurrentPosition;
        State.PreviousRotation = State.CurrentRotation;
        State.CurrentPosition = Position;
        State.CurrentRotation = Rotation;
        State.SecondsSinceSample = 0.0f;
    }
    else
    {
        State.SecondsSinceSample =
            std::min(State.StepSeconds, State.SecondsSinceSample + std::max(0.0f, DeltaSeconds));
    }
}

void ApplyInterpolationState(TransformComponent& Transform,
                             const BodyRenderInterpolationState& State,
                             const bool DisableInterpolation)
{
    const float Alpha = DisableInterpolation ? 1.0f
                                             : std::clamp(State.SecondsSinceSample / std::max(State.StepSeconds, 0.0001f),
                                                          0.0f,
                                                          1.0f);
    const Vec3::Scalar PositionAlpha = static_cast<Vec3::Scalar>(Alpha);
    Transform.Position = State.PreviousPosition + ((State.CurrentPosition - State.PreviousPosition) * PositionAlpha);
    Transform.Rotation = NlerpShortestPath(State.PreviousRotation, State.CurrentRotation, Alpha);
}

void InitializePlayerRenderInterpolation(World& Graph,
                                         const PlayerActor& Player,
                                         PlayerRenderInterpolationState& State,
                                         const float FixedHz)
{
    InitializeInterpolationState(State, FixedHz);

    Vec3 Position = Vec3::Zero();
    Quat Rotation = Quat::Identity();
    if (!SampleBodyPoseWorld(Graph, Player.Body, Position, Rotation))
    {
        return;
    }

    SeedInterpolationState(State, Position, Rotation);
}

void UpdatePlayerRenderInterpolation(World& Graph,
                                     PlayerActor& Player,
                                     PlayerRenderInterpolationState& State,
                                     const float DeltaSeconds,
                                     const bool DisableInterpolation)
{
    if (!Player.Transform)
    {
        return;
    }

    Vec3 Position = Vec3::Zero();
    Quat Rotation = Quat::Identity();
    if (!SampleBodyPoseWorld(Graph, Player.Body, Position, Rotation))
    {
        return;
    }

    AdvanceInterpolationState(State, Position, Rotation, DeltaSeconds);
    ApplyInterpolationState(*Player.Transform, State, DisableInterpolation);
}

void InitializeCubeRenderInterpolation(World& Graph, CubeSlot& Slot, const float FixedHz)
{
    InitializeInterpolationState(Slot.Interpolation, FixedHz);

    Vec3 Position = Vec3::Zero();
    Quat Rotation = Quat::Identity();
    if (!SampleBodyPoseWorld(Graph, Slot.Body, Position, Rotation))
    {
        return;
    }

    SeedInterpolationState(Slot.Interpolation, Position, Rotation);

    if (Slot.Transform)
    {
        ApplyInterpolationState(*Slot.Transform, Slot.Interpolation, true);
    }
}

void UpdateCubeRenderInterpolation(World& Graph,
                                   CubeSlot& Slot,
                                   const float DeltaSeconds,
                                   const bool DisableInterpolation)
{
    if (!Slot.Active || !Slot.Transform || !Slot.Body)
    {
        return;
    }

    Vec3 Position = Vec3::Zero();
    Quat Rotation = Quat::Identity();
    if (!SampleBodyPoseWorld(Graph, Slot.Body, Position, Rotation))
    {
        return;
    }

    AdvanceInterpolationState(Slot.Interpolation, Position, Rotation, DeltaSeconds);
    ApplyInterpolationState(*Slot.Transform, Slot.Interpolation, DisableInterpolation);
}
#endif

void UpdateFollowCamera(CameraComponent* Camera, const TransformComponent* PlayerTransform)
{
    if (!Camera || !PlayerTransform)
    {
        return;
    }

    auto* RendererCamera = Camera->Camera();
    if (!RendererCamera)
    {
        return;
    }

    const Vec3 Target = PlayerTransform->Position + Vec3(0.0f, 1.1f, 0.0f);
    const Vec3 CameraPosition = Target + Vec3(0.0f, 4.0f, 8.0f);
    RendererCamera->Position(ToRendererVector3(CameraPosition));
    RendererCamera->LookAt(ToRendererVector3(Target));
}

CameraComponent* CreateStationaryObserverCamera(World& Graph)
{
    return nullptr;
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

CameraComponent* CreateViewCamera(World& Graph)
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

    if (auto Transform = CameraNode->Add<TransformComponent>())
    {
        Transform->Position = Vec3(0.0f, kEarthSurfaceY + 8.0f, 50.0f);
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
    Settings.SyncFromTransform = false;
    Settings.FovDegrees = 60.0f;
    Settings.NearClip = 0.05f;
    Settings.FarClip = 800.0f;
    Settings.Aspect = 16.0f / 9.0f;

    return Camera;
}

void PollRendererEvents(World& Graph, CameraComponent* Camera, bool& Running)
{
    (void)Camera;

#if defined(SNAPI_GF_ENABLE_INPUT)
    if (Graph.Input().IsInitialized())
    {
        if (const auto* Events = Graph.Input().Events())
        {
            for (const auto& Event : *Events)
            {
                if (Event.Type == SnAPI::Input::EInputEventType::WindowCloseRequested)
                {
                    Running = false;
                    return;
                }
            }
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

int RunMode(const Args& Parsed, const ERunMode Mode)
{
    RegisterExampleTypes();

    const bool NetworkingEnabled = ModeHasNetworking(Mode);
    const bool WindowEnabled = ModeHasWindow(Mode);
    const bool ServerRole = ModeUsesServerRole(Mode);

    SessionListener Listener(ModeLabel(Mode));

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

    PlayerActor Player = CreatePlayerActor(Graph, WindowEnabled);
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
    CameraComponent* FollowCamera = nullptr;
    if (WindowEnabled)
    {
        if (!Graph.Renderer().IsInitialized() || !Graph.Renderer().HasOpenWindow())
        {
            std::cerr << "Renderer window was not initialized\n";
            return 1;
        }

        FollowCamera = CreateViewCamera(Graph);
        Camera = CreateStationaryObserverCamera(Graph);
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

    std::vector<CubeSlot> CubeSlots;
    CubeSlots.reserve(static_cast<size_t>(std::max(Parsed.CubeCount, 1)));
    CubeSharedMaterials SharedCubeMaterials{};
    if (WindowEnabled && Graph.Renderer().IsInitialized())
    {
        SharedCubeMaterials = BuildSharedCubeMaterialInstances(Graph, Parsed.CubeShadows);
    }
    std::mt19937 Rng(static_cast<std::uint32_t>(Clock::now().time_since_epoch().count()));
    for (int i = 0; i < Parsed.CubeCount; ++i)
    {
        CubeSlot Slot{};
        if (CreateCubeSlot(Graph,
                           i,
                           Slot,
                           Parsed.CubeShadows,
                           SharedCubeMaterials.GBuffer,
                           SharedCubeMaterials.Shadow))
        {
            CubeSlots.push_back(Slot);
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
    constexpr int MaxActivationsPerFrame = 64;
    const float SpawnIntervalSeconds = CubeLifetimeSeconds / static_cast<float>(std::max(Parsed.CubeCount, 1));
    const float SpawnHalfExtent = std::max(10.0f, std::sqrt(static_cast<float>(std::max(Parsed.CubeCount, 1))) * 1.5f);
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    PlayerRenderInterpolationState PlayerInterpolation{};
    InitializePlayerRenderInterpolation(Graph, Player, PlayerInterpolation, Parsed.FixedHz);
    for (auto& Slot : CubeSlots)
    {
        InitializeCubeRenderInterpolation(Graph, Slot, Parsed.FixedHz);
    }
#endif

    const auto Start = Clock::now();
    auto Previous = Start;
    auto NextLog = Start;
    auto NextPerfLog = Start + std::chrono::seconds(1);
    std::uint32_t FramesSincePerfLog = 0;

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

#if defined(SNAPI_GF_ENABLE_PHYSICS)
        Player.Transform = ResolveTransform(Player.Handle);
        UpdatePlayerRenderInterpolation(Graph, Player, PlayerInterpolation, DeltaSeconds, Parsed.DisableInterpolation);
#endif

        if (Graph.IsServer())
        {
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

#if defined(SNAPI_GF_ENABLE_PHYSICS)
        for (auto& Slot : CubeSlots)
        {
            UpdateCubeRenderInterpolation(Graph, Slot, DeltaSeconds, Parsed.DisableInterpolation);
        }
#endif

        if (WindowEnabled)
        {
#if defined(SNAPI_GF_ENABLE_PHYSICS)
            if (FollowCamera && FollowCamera->IsActive())
            {
                UpdateFollowCamera(FollowCamera, Player.Transform);
            }
#endif

            const float SafeDelta = std::max(DeltaSeconds, 0.0001f);
            const int Fps = static_cast<int>(std::round(1.0f / SafeDelta));
            const std::string FpsText = "FPS: " + std::to_string(Fps);
            (void)Graph.Renderer().QueueText(FpsText, 16.0f, 16.0f);
        }

        Runtime.Update(DeltaSeconds);

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
                const auto ActiveCubes = std::count_if(CubeSlots.begin(), CubeSlots.end(), [](const CubeSlot& Slot) {
                    return Slot.Active;
                });

                std::cout << "[" << ModeLabel(Mode) << "Perf] fps=" << FramesSincePerfLog
                          << " active_cubes=" << ActiveCubes
                          << " total_cubes=" << CubeSlots.size()
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
