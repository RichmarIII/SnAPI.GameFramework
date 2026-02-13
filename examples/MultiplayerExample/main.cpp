#if !defined(GL_GLEXT_PROTOTYPES)
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <algorithm>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;
using namespace SnAPI::Networking;

namespace
{

/**
 * @brief Parsed command-line configuration for multiplayer example runtime mode.
 * @remarks Determines server/client role, transport endpoints, and cube spawn count.
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
    int CubeCount = 16; // Maximum concurrent cubes on server.
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

const char* DisconnectReasonToString(EDisconnectReason Reason)
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
 * @remarks Used to make transport/replication behavior visible during manual testing.
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
                         float TimeSeconds)
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
        << "  " << Exe << " --server [--bind <addr>] [--port <port>] [--count <max-live>] [--no-reset-sim]\n"
        << "  " << Exe << " --client [--host <addr>] [--bind <addr>] [--port <port>] [--no-interp]\n"
        << "  " << Exe << " --local [--count <max-live>] [--no-interp] [--no-reset-sim]\n";
}

bool ParseArgs(int argc, char** argv, Args& Out)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view Arg = argv[i];
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
    if (ModeCount != 1)
    {
        return false;
    }

    return true;
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
    Config.Pacing.MaxBytesPerSecond = 9'125'000;
    Config.Pacing.BurstBytes = 100 * 1024 * 1024 ;
    Config.Pacing.MaxBytesPerPump = 256 * 1024 * 1024;
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
                                        bool ServerMode,
                                        SessionListener* Listener,
                                        bool EnableNetworking)
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
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 4;
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
    Settings.Physics = PhysicsSettings;
#endif

    return Settings;
}

struct CubeSlot
{
    NodeHandle Handle{};
    TransformComponent* Transform = nullptr;
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    RigidBodyComponent* Body = nullptr;
    Vec3 ParkPosition{};
#endif
    bool Active = false;
    float SpawnedAtSeconds = 0.0f;
};

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
        Transform->Position = Vec3(0.0f, -1.0f, 0.0f);
    }

    if (auto Collider = GroundNode->Add<ColliderComponent>())
    {
        auto& Settings = Collider->EditSettings();
        Settings.Shape = SnAPI::Physics::EShapeType::Box;
        Settings.HalfExtent = Vec3(64.0f, 1.0f, 64.0f);
        Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
        Settings.Mask = kCollisionMaskAll;
        Settings.Friction = 0.65f;
        Settings.Restitution = 0.80f;
    }

    if (auto RigidBody = GroundNode->Add<RigidBodyComponent>())
    {
        auto& Settings = RigidBody->EditSettings();
        Settings.BodyType = SnAPI::Physics::EBodyType::Static;
        Settings.SyncToPhysics = true;
        (void)RigidBody->RecreateBody();
    }
}

bool CreateCubeSlot(World& Graph, int CubeIndex, CubeSlot& OutSlot)
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
    OutSlot.ParkPosition = Vec3(ParkX, -1000.0f, ParkZ);
    // Delay first replication of this slot until first activation to avoid a large
    // reliable spawn burst when a client initially joins.
    Transform->Replicated(false);
    Transform->Position = OutSlot.ParkPosition;
    Transform->Scale = Vec3(0.0f, 0.0f, 0.0f);

    if (auto Collider = Node->Add<ColliderComponent>())
    {
        auto& Settings = Collider->EditSettings();
        Settings.Shape = SnAPI::Physics::EShapeType::Box;
        Settings.HalfExtent = Vec3(0.5f, 0.5f, 0.5f);
        Settings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldDynamic);
        Settings.Mask = CollisionMaskFlags(ECollisionFilterBits::WorldStatic); // ground only (keeps high-count stress tests stable)
        Settings.Friction = 0.55f;
        Settings.Restitution = 0.62f;
    }

    if (auto RigidBody = Node->Add<RigidBodyComponent>())
    {
        auto& Settings = RigidBody->EditSettings();
        Settings.BodyType = SnAPI::Physics::EBodyType::Dynamic;
        Settings.Mass = 1.0f;
        Settings.LinearDamping = 0.05f;
        Settings.AngularDamping = 0.08f;
        Settings.EnableCcd = true;
        Settings.StartActive = false;
        (void)RigidBody->RecreateBody();
        (void)RigidBody->Teleport(OutSlot.ParkPosition, Vec3{}, true);
        OutSlot.Body = &*RigidBody;
    }

    OutSlot.Handle = Node->Handle();
    OutSlot.Transform = Transform;
    OutSlot.Active = false;
    OutSlot.SpawnedAtSeconds = 0.0f;
    return true;
}

void ActivateCubeSlot(CubeSlot& Slot, float TimeSeconds, std::mt19937& Rng, float SpawnHalfExtent)
{
    if (!Slot.Transform || !Slot.Body)
    {
        return;
    }

    std::uniform_real_distribution<float> SpawnXZ(-SpawnHalfExtent, SpawnHalfExtent);
    std::uniform_real_distribution<float> SpawnY(8.0f, 12.0f);
    std::uniform_real_distribution<float> InitialVX(-2.5f, 2.5f);
    std::uniform_real_distribution<float> InitialVY(0.0f, 3.0f);
    std::uniform_real_distribution<float> InitialVZ(-2.5f, 2.5f);
    std::uniform_real_distribution<float> InitialAngular(-6.0f, 6.0f);

    if (!Slot.Transform->Replicated())
    {
        Slot.Transform->Replicated(true);
    }

    const Vec3 SpawnPosition = Vec3(SpawnXZ(Rng), SpawnY(Rng), SpawnXZ(Rng));
    Slot.Transform->Scale = Vec3(1.0f, 1.0f, 1.0f);

    (void)Slot.Body->Teleport(SpawnPosition, Vec3{}, true);
    (void)Slot.Body->SetVelocity(Vec3(InitialVX(Rng), InitialVY(Rng), InitialVZ(Rng)),
                                 Vec3(InitialAngular(Rng), InitialAngular(Rng), InitialAngular(Rng)));

    Slot.Active = true;
    Slot.SpawnedAtSeconds = TimeSeconds;
}

void DeactivateCubeSlot(CubeSlot& Slot, float TimeSeconds)
{
    if (!Slot.Transform || !Slot.Body)
    {
        Slot.Active = false;
        Slot.SpawnedAtSeconds = TimeSeconds;
        return;
    }

    (void)Slot.Body->Teleport(Slot.ParkPosition, Vec3{}, true);
    Slot.Transform->Scale = Vec3(0.0f, 0.0f, 0.0f);
    Slot.Transform->Position = Slot.ParkPosition;
    Slot.Active = false;
    Slot.SpawnedAtSeconds = TimeSeconds;
}
#endif

float WrapRadians(float Value)
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    while (Value > kPi)
    {
        Value -= kTwoPi;
    }
    while (Value < -kPi)
    {
        Value += kTwoPi;
    }
    return Value;
}

float SmoothAngleRadians(float Current, float Target, float Alpha)
{
    const float Delta = WrapRadians(Target - Current);
    return WrapRadians(Current + Delta * Alpha);
}

float RadiansToDegrees(float Radians)
{
    return Radians * (180.0f / 3.14159265358979323846f);
}

struct Mat4
{
    std::array<float, 16> Data{};
};

Mat4 IdentityMatrix()
{
    Mat4 Result{};
    Result.Data = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return Result;
}

Mat4 Multiply(const Mat4& A, const Mat4& B)
{
    Mat4 Result{};
    for (int Column = 0; Column < 4; ++Column)
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            Result.Data[Column * 4 + Row] =
                A.Data[0 * 4 + Row] * B.Data[Column * 4 + 0] +
                A.Data[1 * 4 + Row] * B.Data[Column * 4 + 1] +
                A.Data[2 * 4 + Row] * B.Data[Column * 4 + 2] +
                A.Data[3 * 4 + Row] * B.Data[Column * 4 + 3];
        }
    }
    return Result;
}

Mat4 TranslationMatrix(const Vec3& Translation)
{
    Mat4 Result = IdentityMatrix();
    Result.Data[12] = Translation.x();
    Result.Data[13] = Translation.y();
    Result.Data[14] = Translation.z();
    return Result;
}

Mat4 ScaleMatrix(const Vec3& Scale)
{
    Mat4 Result{};
    Result.Data = {
        Scale.x(), 0.0f,    0.0f,    0.0f,
        0.0f,    Scale.y(), 0.0f,    0.0f,
        0.0f,    0.0f,    Scale.z(), 0.0f,
        0.0f,    0.0f,    0.0f,    1.0f
    };
    return Result;
}

Mat4 RotationXMatrix(float Radians)
{
    const float C = std::cos(Radians);
    const float S = std::sin(Radians);
    Mat4 Result = IdentityMatrix();
    Result.Data[5] = C;
    Result.Data[6] = S;
    Result.Data[9] = -S;
    Result.Data[10] = C;
    return Result;
}

Mat4 RotationYMatrix(float Radians)
{
    const float C = std::cos(Radians);
    const float S = std::sin(Radians);
    Mat4 Result = IdentityMatrix();
    Result.Data[0] = C;
    Result.Data[2] = -S;
    Result.Data[8] = S;
    Result.Data[10] = C;
    return Result;
}

Mat4 RotationZMatrix(float Radians)
{
    const float C = std::cos(Radians);
    const float S = std::sin(Radians);
    Mat4 Result = IdentityMatrix();
    Result.Data[0] = C;
    Result.Data[1] = S;
    Result.Data[4] = -S;
    Result.Data[5] = C;
    return Result;
}

Mat4 PerspectiveMatrix(float FovDegrees, float Aspect, float Near, float Far)
{
    const float FovRadians = FovDegrees * 3.14159265358979323846f / 180.0f;
    const float F = 1.0f / std::tan(FovRadians * 0.5f);
    Mat4 Result{};
    Result.Data = {
        F / Aspect, 0.0f, 0.0f,                             0.0f,
        0.0f,       F,    0.0f,                             0.0f,
        0.0f,       0.0f, (Far + Near) / (Near - Far),    -1.0f,
        0.0f,       0.0f, (2.0f * Far * Near) / (Near - Far), 0.0f
    };
    return Result;
}

Mat4 BuildViewMatrix()
{
    Mat4 View = IdentityMatrix();
    View = Multiply(View, TranslationMatrix(Vec3(0.0f, -2.0f, -30.0f)));
    View = Multiply(View, RotationXMatrix(20.0f * 3.14159265358979323846f / 180.0f));
    return View;
}

Mat4 BuildModelMatrix(const Vec3& Position, const Vec3& Rotation, const Vec3& Scale)
{
    Mat4 Model = IdentityMatrix();
    Model = Multiply(Model, TranslationMatrix(Position));
    // Keep render Euler composition aligned with RigidBodyComponent's
    // Euler<->quaternion conversion convention (ZYX).
    Model = Multiply(Model, RotationZMatrix(Rotation.z()));
    Model = Multiply(Model, RotationYMatrix(Rotation.y()));
    Model = Multiply(Model, RotationXMatrix(Rotation.x()));
    Model = Multiply(Model, ScaleMatrix(Scale));
    return Model;
}

void AppendMatrix(std::vector<float>& Buffer, const Mat4& Matrix)
{
    Buffer.insert(Buffer.end(), Matrix.Data.begin(), Matrix.Data.end());
}

struct InstancedCubeRenderer
{
    GLuint Program = 0;
    GLuint VertexBuffer = 0;
    GLuint InstanceBuffer = 0;
    GLint ViewProjectionUniform = -1;
    GLint LightDirectionUniform = -1;
    GLint CubeColorUniform = -1;
    bool Ready = false;
};

GLuint CompileShader(GLenum Type, const char* Source)
{
    const GLuint Shader = glCreateShader(Type);
    if (Shader == 0)
    {
        return 0;
    }

    glShaderSource(Shader, 1, &Source, nullptr);
    glCompileShader(Shader);

    GLint CompileStatus = GL_FALSE;
    glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);
    if (CompileStatus == GL_TRUE)
    {
        return Shader;
    }

    GLint LogLength = 0;
    glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
    if (LogLength > 0)
    {
        std::string Log(static_cast<size_t>(LogLength), '\0');
        glGetShaderInfoLog(Shader, LogLength, nullptr, Log.data());
        std::cerr << "Shader compile failed: " << Log << "\n";
    }
    glDeleteShader(Shader);
    return 0;
}

GLuint LinkProgram(GLuint VertexShader, GLuint FragmentShader)
{
    const GLuint Program = glCreateProgram();
    if (Program == 0)
    {
        return 0;
    }

    glAttachShader(Program, VertexShader);
    glAttachShader(Program, FragmentShader);
    glBindAttribLocation(Program, 0, "aPosition");
    glBindAttribLocation(Program, 1, "aNormal");
    glBindAttribLocation(Program, 2, "aModelCol0");
    glBindAttribLocation(Program, 3, "aModelCol1");
    glBindAttribLocation(Program, 4, "aModelCol2");
    glBindAttribLocation(Program, 5, "aModelCol3");
    glLinkProgram(Program);

    GLint LinkStatus = GL_FALSE;
    glGetProgramiv(Program, GL_LINK_STATUS, &LinkStatus);
    if (LinkStatus == GL_TRUE)
    {
        return Program;
    }

    GLint LogLength = 0;
    glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
    if (LogLength > 0)
    {
        std::string Log(static_cast<size_t>(LogLength), '\0');
        glGetProgramInfoLog(Program, LogLength, nullptr, Log.data());
        std::cerr << "Program link failed: " << Log << "\n";
    }
    glDeleteProgram(Program);
    return 0;
}

bool InitInstancedCubeRenderer(InstancedCubeRenderer& Renderer)
{
    const char* VertexShaderSource = R"(
        #version 120
        attribute vec3 aPosition;
        attribute vec3 aNormal;
        attribute vec4 aModelCol0;
        attribute vec4 aModelCol1;
        attribute vec4 aModelCol2;
        attribute vec4 aModelCol3;

        uniform mat4 uViewProjection;

        varying vec3 vNormal;

        void main()
        {
            mat4 Model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
            vec4 WorldPosition = Model * vec4(aPosition, 1.0);
            vNormal = normalize(mat3(Model) * aNormal);
            gl_Position = uViewProjection * WorldPosition;
        }
    )";

    const char* FragmentShaderSource = R"(
        #version 120
        uniform vec3 uLightDirection;
        uniform vec3 uCubeColor;
        varying vec3 vNormal;

        void main()
        {
            vec3 Normal = normalize(vNormal);
            float NdotL = max(dot(Normal, normalize(uLightDirection)), 0.0);
            float Diffuse = 0.75 * NdotL;
            float Ambient = 0.25;
            vec3 Lit = uCubeColor * (Ambient + Diffuse);
            gl_FragColor = vec4(Lit, 1.0);
        }
    )";

    const GLuint VertexShader = CompileShader(GL_VERTEX_SHADER, VertexShaderSource);
    if (VertexShader == 0)
    {
        return false;
    }

    const GLuint FragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShaderSource);
    if (FragmentShader == 0)
    {
        glDeleteShader(VertexShader);
        return false;
    }

    Renderer.Program = LinkProgram(VertexShader, FragmentShader);
    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader);
    if (Renderer.Program == 0)
    {
        return false;
    }

    Renderer.ViewProjectionUniform = glGetUniformLocation(Renderer.Program, "uViewProjection");
    Renderer.LightDirectionUniform = glGetUniformLocation(Renderer.Program, "uLightDirection");
    Renderer.CubeColorUniform = glGetUniformLocation(Renderer.Program, "uCubeColor");
    if (Renderer.ViewProjectionUniform < 0 || Renderer.LightDirectionUniform < 0 || Renderer.CubeColorUniform < 0)
    {
        glDeleteProgram(Renderer.Program);
        Renderer.Program = 0;
        return false;
    }

    constexpr float CubeVertices[] = {
        // +Z
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,

        // -Z
         0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
         0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
         0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

        // -X
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f,

        // +X
         0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f,

        // +Y
        -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,

        // -Y
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
         0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f
    };

    glGenBuffers(1, &Renderer.VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Renderer.VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CubeVertices), CubeVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &Renderer.InstanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Renderer.InstanceBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, nullptr, GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    Renderer.Ready = (Renderer.VertexBuffer != 0 && Renderer.InstanceBuffer != 0);
    if (!Renderer.Ready)
    {
        if (Renderer.VertexBuffer != 0)
        {
            glDeleteBuffers(1, &Renderer.VertexBuffer);
            Renderer.VertexBuffer = 0;
        }
        if (Renderer.InstanceBuffer != 0)
        {
            glDeleteBuffers(1, &Renderer.InstanceBuffer);
            Renderer.InstanceBuffer = 0;
        }
        if (Renderer.Program != 0)
        {
            glDeleteProgram(Renderer.Program);
            Renderer.Program = 0;
        }
    }

    return Renderer.Ready;
}

void ShutdownInstancedCubeRenderer(InstancedCubeRenderer& Renderer)
{
    if (Renderer.InstanceBuffer != 0)
    {
        glDeleteBuffers(1, &Renderer.InstanceBuffer);
        Renderer.InstanceBuffer = 0;
    }
    if (Renderer.VertexBuffer != 0)
    {
        glDeleteBuffers(1, &Renderer.VertexBuffer);
        Renderer.VertexBuffer = 0;
    }
    if (Renderer.Program != 0)
    {
        glDeleteProgram(Renderer.Program);
        Renderer.Program = 0;
    }
    Renderer.Ready = false;
}

void DrawInstancedCubes(const InstancedCubeRenderer& Renderer,
                        const Mat4& ViewProjection,
                        const std::vector<float>& InstanceMatrices,
                        const float ColorR,
                        const float ColorG,
                        const float ColorB)
{
    if (!Renderer.Ready || InstanceMatrices.empty())
    {
        return;
    }

    const GLsizei InstanceCount = static_cast<GLsizei>(InstanceMatrices.size() / 16u);

    glUseProgram(Renderer.Program);
    glUniformMatrix4fv(Renderer.ViewProjectionUniform, 1, GL_FALSE, ViewProjection.Data.data());
    glUniform3f(Renderer.LightDirectionUniform, 20.0f, 28.0f, 18.0f);
    glUniform3f(Renderer.CubeColorUniform, ColorR, ColorG, ColorB);

    glBindBuffer(GL_ARRAY_BUFFER, Renderer.VertexBuffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, reinterpret_cast<const void*>(0));
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(float) * 6,
                          reinterpret_cast<const void*>(sizeof(float) * 3));

    glBindBuffer(GL_ARRAY_BUFFER, Renderer.InstanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(InstanceMatrices.size() * sizeof(float)),
                 InstanceMatrices.data(),
                 GL_STREAM_DRAW);

    constexpr GLsizei MatrixStride = sizeof(float) * 16;
    for (GLuint Column = 0; Column < 4; ++Column)
    {
        const GLuint AttributeIndex = 2 + Column;
        glEnableVertexAttribArray(AttributeIndex);
        glVertexAttribPointer(AttributeIndex,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              MatrixStride,
                              reinterpret_cast<const void*>(static_cast<std::uintptr_t>(sizeof(float) * Column * 4)));
        glVertexAttribDivisor(AttributeIndex, 1);
    }

    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, InstanceCount);

    for (GLuint Column = 0; Column < 4; ++Column)
    {
        const GLuint AttributeIndex = 2 + Column;
        glVertexAttribDivisor(AttributeIndex, 0);
        glDisableVertexAttribArray(AttributeIndex);
    }
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

bool HasInstancingSupport()
{
    int Major = 0;
    int Minor = 0;
    if (const GLubyte* Version = glGetString(GL_VERSION); Version != nullptr)
    {
        (void)std::sscanf(reinterpret_cast<const char*>(Version), "%d.%d", &Major, &Minor);
    }

    if (Major > 3 || (Major == 3 && Minor >= 3))
    {
        return true;
    }

    return glfwExtensionSupported("GL_ARB_draw_instanced")
        && glfwExtensionSupported("GL_ARB_instanced_arrays");
}

void DrawCube(float Size)
{
    const float Half = Size * 0.5f;
    glBegin(GL_QUADS);
    // Front
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-Half, -Half, Half);
    glVertex3f(Half, -Half, Half);
    glVertex3f(Half, Half, Half);
    glVertex3f(-Half, Half, Half);
    // Back
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(Half, -Half, -Half);
    glVertex3f(-Half, -Half, -Half);
    glVertex3f(-Half, Half, -Half);
    glVertex3f(Half, Half, -Half);
    // Left
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-Half, -Half, -Half);
    glVertex3f(-Half, -Half, Half);
    glVertex3f(-Half, Half, Half);
    glVertex3f(-Half, Half, -Half);
    // Right
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(Half, -Half, Half);
    glVertex3f(Half, -Half, -Half);
    glVertex3f(Half, Half, -Half);
    glVertex3f(Half, Half, Half);
    // Top
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-Half, Half, Half);
    glVertex3f(Half, Half, Half);
    glVertex3f(Half, Half, -Half);
    glVertex3f(-Half, Half, -Half);
    // Bottom
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-Half, -Half, -Half);
    glVertex3f(Half, -Half, -Half);
    glVertex3f(Half, -Half, Half);
    glVertex3f(-Half, -Half, Half);
    glEnd();
}

void DrawGroundPlane(float HalfSize, float Y)
{
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-HalfSize, Y, -HalfSize);
    glVertex3f(HalfSize, Y, -HalfSize);
    glVertex3f(HalfSize, Y, HalfSize);
    glVertex3f(-HalfSize, Y, HalfSize);
    glEnd();
}

void SetupCamera(int Width, int Height)
{
    const float Aspect = (Height > 0) ? static_cast<float>(Width) / static_cast<float>(Height) : 1.0f;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const float Near = 1.0f;
    const float Far = 200.0f;
    const float FovRadians = 60.0f * 3.1415926f / 180.0f;
    const float Top = std::tan(FovRadians * 0.5f) * Near;
    const float Right = Top * Aspect;
    glFrustum(-Right, Right, -Top, Top, Near, Far);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, -2.0f, -30.0f);
    glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
}

void RenderWorldScene(GLFWwindow* Window,
                      World& Graph,
                      float DeltaSeconds,
                      bool UseInterpolation,
                      std::unordered_map<Uuid, Vec3, UuidHash>& SmoothedPositions,
                      std::unordered_map<Uuid, Vec3, UuidHash>& SmoothedRotations,
                      std::unordered_set<Uuid, UuidHash>& SeenNodeIds,
                      const InstancedCubeRenderer* CubeRenderer)
{
    int Width = 0;
    int Height = 0;
    glfwGetFramebufferSize(Window, &Width, &Height);
    glViewport(0, 0, Width, Height);

    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SetupCamera(Width, Height);
    const GLfloat LightPosition[] = {20.0f, 28.0f, 18.0f, 1.0f};
    const GLfloat LightDiffuse[] = {0.90f, 0.90f, 0.85f, 1.0f};
    const GLfloat LightSpecular[] = {0.20f, 0.20f, 0.20f, 1.0f};
    const GLfloat AmbientModel[] = {0.22f, 0.22f, 0.25f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, LightPosition);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, LightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, LightSpecular);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, AmbientModel);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    glColor3f(0.30f, 0.45f, 0.32f);
    DrawGroundPlane(64.0f, 0.0f);

    const float Aspect = (Height > 0) ? static_cast<float>(Width) / static_cast<float>(Height) : 1.0f;
    const Mat4 Projection = PerspectiveMatrix(60.0f, Aspect, 1.0f, 200.0f);
    const Mat4 View = BuildViewMatrix();
    const Mat4 ViewProjection = Multiply(Projection, View);
    std::vector<float> AwakeInstanceMatrices;
    std::vector<float> SleepingInstanceMatrices;
    if (CubeRenderer && CubeRenderer->Ready)
    {
        const std::size_t ReserveCount = SmoothedPositions.size() * 16u;
        AwakeInstanceMatrices.reserve(ReserveCount);
        SleepingInstanceMatrices.reserve(ReserveCount / 4u);
    }

    constexpr float PositionLerpSpeed = 16.0f;
    constexpr float RotationLerpSpeed = 20.0f;
    const float PositionAlpha = UseInterpolation
        ? std::clamp(DeltaSeconds * PositionLerpSpeed, 0.0f, 1.0f)
        : 1.0f;
    const float RotationAlpha = UseInterpolation
        ? std::clamp(DeltaSeconds * RotationLerpSpeed, 0.0f, 1.0f)
        : 1.0f;
    SeenNodeIds.clear();

    Graph.NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        auto TransformResult = Node.Component<TransformComponent>();
        if (!TransformResult)
        {
            return;
        }
        if (Node.Name() == "Ground")
        {
            return;
        }
        const auto& Transform = *TransformResult;
        const float MaxScaleAxis = std::max({std::abs(Transform.Scale.x()),
                                             std::abs(Transform.Scale.y()),
                                             std::abs(Transform.Scale.z())});
        if (MaxScaleAxis <= 0.0001f)
        {
            return;
        }
        SeenNodeIds.insert(Node.Id());

        auto [It, Inserted] = SmoothedPositions.emplace(Node.Id(), Transform.Position);
        Vec3& Smoothed = It->second;
        if (Inserted || !UseInterpolation)
        {
            Smoothed = Transform.Position;
        }
        else
        {
            Smoothed.x() += (Transform.Position.x() - Smoothed.x()) * PositionAlpha;
            Smoothed.y() += (Transform.Position.y() - Smoothed.y()) * PositionAlpha;
            Smoothed.z() += (Transform.Position.z() - Smoothed.z()) * PositionAlpha;
        }

        auto [RotIt, RotInserted] = SmoothedRotations.emplace(Node.Id(), Transform.Rotation);
        Vec3& SmoothedRotation = RotIt->second;
        if (RotInserted || !UseInterpolation)
        {
            SmoothedRotation = Transform.Rotation;
        }
        else
        {
            SmoothedRotation.x() = SmoothAngleRadians(SmoothedRotation.x(), Transform.Rotation.x(), RotationAlpha);
            SmoothedRotation.y() = SmoothAngleRadians(SmoothedRotation.y(), Transform.Rotation.y(), RotationAlpha);
            SmoothedRotation.z() = SmoothAngleRadians(SmoothedRotation.z(), Transform.Rotation.z(), RotationAlpha);
        }

        bool Sleeping = false;
        if (auto RigidBodyResult = Node.Component<RigidBodyComponent>())
        {
            Sleeping = RigidBodyResult->IsSleeping();
        }

        if (CubeRenderer && CubeRenderer->Ready)
        {
            const Mat4 Model = BuildModelMatrix(Smoothed, SmoothedRotation, Transform.Scale);
            if (Sleeping)
            {
                AppendMatrix(SleepingInstanceMatrices, Model);
            }
            else
            {
                AppendMatrix(AwakeInstanceMatrices, Model);
            }
        }
        else
        {
            glPushMatrix();
            glTranslatef(Smoothed.x(), Smoothed.y(), Smoothed.z());
            glRotatef(RadiansToDegrees(SmoothedRotation.z()), 0.0f, 0.0f, 1.0f);
            glRotatef(RadiansToDegrees(SmoothedRotation.y()), 0.0f, 1.0f, 0.0f);
            glRotatef(RadiansToDegrees(SmoothedRotation.x()), 1.0f, 0.0f, 0.0f);
            glScalef(Transform.Scale.x(), Transform.Scale.y(), Transform.Scale.z());
            if (Sleeping)
            {
                glColor3f(0.92f, 0.22f, 0.24f);
            }
            else
            {
                glColor3f(0.40f, 0.80f, 0.90f);
            }
            DrawCube(1.0f);
            glPopMatrix();
        }
    });

    if (CubeRenderer && CubeRenderer->Ready)
    {
        DrawInstancedCubes(*CubeRenderer, ViewProjection, AwakeInstanceMatrices, 0.40f, 0.80f, 0.90f);
        DrawInstancedCubes(*CubeRenderer, ViewProjection, SleepingInstanceMatrices, 0.92f, 0.22f, 0.24f);
    }

    for (auto It = SmoothedPositions.begin(); It != SmoothedPositions.end();)
    {
        if (SeenNodeIds.contains(It->first))
        {
            ++It;
        }
        else
        {
            It = SmoothedPositions.erase(It);
        }
    }

    for (auto It = SmoothedRotations.begin(); It != SmoothedRotations.end();)
    {
        if (SeenNodeIds.contains(It->first))
        {
            ++It;
        }
        else
        {
            It = SmoothedRotations.erase(It);
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
                                                            NetworkingEnabled));
        !InitResult)
    {
        std::cerr << "Failed to initialize runtime: " << InitResult.error().Message << "\n";
        return 1;
    }

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
#else
    std::cerr << "This example requires physics support in this build\n";
    return 1;
#endif

    std::vector<CubeSlot> CubeSlots;
    CubeSlots.reserve(static_cast<size_t>(std::max(Parsed.CubeCount, 1)));
    std::mt19937 Rng(static_cast<std::uint32_t>(Clock::now().time_since_epoch().count()));
    for (int i = 0; i < Parsed.CubeCount; ++i)
    {
        CubeSlot Slot{};
        if (CreateCubeSlot(Graph, i, Slot))
        {
            CubeSlots.push_back(Slot);
        }
    }

#if defined(SNAPI_GF_ENABLE_AUDIO)
    AudioSourceComponent* ReplicatedAudio = nullptr;
    if (NetworkingEnabled && ServerRole)
    {
        if (auto AudioNodeResult = Graph.CreateNode("ReplicatedAudio"))
        {
            if (auto* AudioNode = AudioNodeResult->Borrowed())
            {
                AudioNode->Replicated(true);
                if (auto Transform = AudioNode->Add<TransformComponent>())
                {
                    Transform->Replicated(true);
                    Transform->Position = Vec3(0.0f, 0.0f, 0.0f);
                }

                if (auto Audio = AudioNode->Add<AudioSourceComponent>())
                {
                    ReplicatedAudio = &*Audio;
                    ReplicatedAudio->Replicated(true);
                    auto& Settings = ReplicatedAudio->EditSettings();
                    Settings.SoundPath = "sound.wav";
                    Settings.Streaming = false;
                    Settings.AutoPlay = false;
                    Settings.Looping = false;
                    Settings.Volume = 1.0f;
                    Settings.SpatialGain = 1.0f;
                    Settings.MinDistance = 1.0f;
                    Settings.MaxDistance = 64.0f;
                    Settings.Rolloff = 1.0f;
                }
            }
        }
    }
#endif

    GLFWwindow* Window = nullptr;
    InstancedCubeRenderer CubeRenderer{};
    if (WindowEnabled)
    {
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW\n";
            return 1;
        }

        const char* Title = (Mode == ERunMode::Local)
            ? "MultiplayerExample (Local)"
            : "MultiplayerExample";
        Window = glfwCreateWindow(1024, 768, Title, nullptr, nullptr);
        if (!Window)
        {
            glfwTerminate();
            std::cerr << "Failed to create window\n";
            return 1;
        }
        glfwMakeContextCurrent(Window);
        glfwSwapInterval(1);
        glEnable(GL_DEPTH_TEST);

        if (HasInstancingSupport() && InitInstancedCubeRenderer(CubeRenderer))
        {
            std::cout << "[" << ModeLabel(Mode) << "] Rendering mode: instanced cubes\n";
        }
        else
        {
            std::cout << "[" << ModeLabel(Mode) << "] Rendering mode: fallback immediate cubes\n";
            ShutdownInstancedCubeRenderer(CubeRenderer);
        }
    }

    if (Mode == ERunMode::Server)
    {
        std::cout << "Server listening on " << Parsed.Bind << ":" << Parsed.Port
                  << ", reset=" << (Parsed.ResetSimulation ? "on" : "off") << "\n";
    }
    else if (Mode == ERunMode::Local)
    {
        std::cout << "Local mode: networking disabled, cubes=" << Parsed.CubeCount
                  << ", interp=" << (Parsed.DisableInterpolation ? "off" : "on")
                  << ", reset=" << (Parsed.ResetSimulation ? "on" : "off") << "\n";
    }

    float SpawnAccumulator = 0.0f;
    constexpr float CubeLifetimeSeconds = 5.0f;
    constexpr float MinInactiveSeconds = 1.0f / 60.0f;
    constexpr int MaxActivationsPerFrame = 64;
    const float SpawnIntervalSeconds = CubeLifetimeSeconds / static_cast<float>(std::max(Parsed.CubeCount, 1));
    const float SpawnHalfExtent = std::max(10.0f, std::sqrt(static_cast<float>(std::max(Parsed.CubeCount, 1))) * 1.5f);
    constexpr float TargetFrameSeconds = 1.0f / 60.0f;

    const auto Start = Clock::now();
    auto Previous = Start;
    auto NextLog = Start;
    auto NextPerfLog = Start + std::chrono::seconds(1);
    std::uint32_t FramesSincePerfLog = 0;
    const bool UseInterpolation = !Parsed.DisableInterpolation;
    std::unordered_map<Uuid, Vec3, UuidHash> SmoothedPositions;
    std::unordered_map<Uuid, Vec3, UuidHash> SmoothedRotations;
    std::unordered_set<Uuid, UuidHash> SeenNodeIds;

#if defined(SNAPI_GF_ENABLE_AUDIO)
    float NextSoundTime = 0.0f;
#endif
    while (!Window || !glfwWindowShouldClose(Window))
    {
        const auto FrameBegin = Clock::now();
        const auto Now = FrameBegin;
        const float DeltaSeconds = std::chrono::duration<float>(Now - Previous).count();
        Previous = Now;
        const float TimeSeconds = std::chrono::duration<float>(Now - Start).count();

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
                auto SlotIt = std::find_if(CubeSlots.begin(), CubeSlots.end(), [&](const CubeSlot& Slot) {
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

        Runtime.Update(DeltaSeconds);

#if defined(SNAPI_GF_ENABLE_AUDIO)
        if (ReplicatedAudio && Session && Graph.IsServer() && TimeSeconds >= NextSoundTime)
        {
            const auto Dumps = Session->DumpConnections(Now);
            const bool HasReadyConnection = std::ranges::any_of(Dumps, [](const NetConnectionDump& Dump) {
                return Dump.HandshakeComplete;
            });
            if (HasReadyConnection)
            {
                NextSoundTime = TimeSeconds + 5.0f;
            }
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

        if (Window)
        {
            RenderWorldScene(Window,
                             Graph,
                             DeltaSeconds,
                             UseInterpolation,
                             SmoothedPositions,
                             SmoothedRotations,
                             SeenNodeIds,
                             CubeRenderer.Ready ? &CubeRenderer : nullptr);

            ++FramesSincePerfLog;
            if (Now >= NextPerfLog)
            {
                std::cout << "[" << ModeLabel(Mode) << "Perf] fps=" << FramesSincePerfLog
                          << " rendered_nodes=" << SeenNodeIds.size()
                          << " interp=" << (UseInterpolation ? "on" : "off") << "\n";
                FramesSincePerfLog = 0;
                NextPerfLog = Now + std::chrono::seconds(1);
            }

            glfwSwapBuffers(Window);
            glfwPollEvents();
        }
        else
        {
            const auto FrameEnd = Clock::now();
            const float FrameWorkSeconds = std::chrono::duration<float>(FrameEnd - FrameBegin).count();
            const float SleepSeconds = TargetFrameSeconds - FrameWorkSeconds;
            if (SleepSeconds > 0.0f)
            {
                std::this_thread::sleep_for(std::chrono::duration<float>(SleepSeconds));
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

    if (Window)
    {
        ShutdownInstancedCubeRenderer(CubeRenderer);
        glfwDestroyWindow(Window);
        glfwTerminate();
    }
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
