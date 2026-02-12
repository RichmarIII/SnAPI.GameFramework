#include <GLFW/glfw3.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
 
#include <algorithm>

#include "GameFramework.hpp"

#include "NetSession.h"
#include "Transport/UdpTransportAsio.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::Networking;

namespace
{

struct Args
{
    bool Server = false;
    bool Client = false;
    std::string Host = "127.0.0.1";
    std::string Bind = "0.0.0.0";
    std::uint16_t Port = 7777;
    int CubeCount = 16;
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
        << "  " << Exe << " --server [--bind <addr>] [--port <port>] [--count <n>]\n"
        << "  " << Exe << " --client [--host <addr>] [--bind <addr>] [--port <port>]\n";
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

    if (Out.Server == Out.Client)
    {
        return false;
    }

    return true;
}

struct ReplicatedTransformComponent final : public IComponent
{
    static constexpr const char* kTypeName =
        "SnAPI::GameFramework::Examples::ReplicatedTransformComponent";

    Vec3 Position{};
    Vec3 Rotation{};
    Vec3 Scale{1.0f, 1.0f, 1.0f};
};

SNAPI_REFLECT_TYPE(ReplicatedTransformComponent, (TTypeBuilder<ReplicatedTransformComponent>(ReplicatedTransformComponent::kTypeName)
    .Field("Position", &ReplicatedTransformComponent::Position, EFieldFlagBits::Replication)
    .Field("Rotation", &ReplicatedTransformComponent::Rotation, EFieldFlagBits::Replication)
    .Field("Scale", &ReplicatedTransformComponent::Scale, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

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
    Config.Threading.UseInternalThreads = false;
    Config.Pacing.MaxBytesPerSecond = 1024 * 1024;
    Config.Pacing.BurstBytes = 1024 * 1024;
    Config.Pacing.MaxBytesPerPump = 1024 * 1024;
    Config.KeepAlive.Timeout = Milliseconds{20000};
    return Config;
}

std::shared_ptr<UdpTransportAsio> MakeUdpTransport(const NetEndpoint& Local)
{
    UdpTransportConfig TransportConfig{};
    TransportConfig.MaxDatagramBytes = 2048;
    TransportConfig.NonBlocking = true;
    auto Transport = std::make_shared<UdpTransportAsio>(TransportConfig);
    if (!Transport->Open(Local))
    {
        return nullptr;
    }
    return Transport;
}

struct MovingCube
{
    ReplicatedTransformComponent* Transform = nullptr;
    Vec3 Center{};
    float Phase = 0.0f;
};

void DrawCube(float Size)
{
    const float Half = Size * 0.5f;
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-Half, -Half, Half);
    glVertex3f(Half, -Half, Half);
    glVertex3f(Half, Half, Half);
    glVertex3f(-Half, Half, Half);
    // Back
    glVertex3f(Half, -Half, -Half);
    glVertex3f(-Half, -Half, -Half);
    glVertex3f(-Half, Half, -Half);
    glVertex3f(Half, Half, -Half);
    // Left
    glVertex3f(-Half, -Half, -Half);
    glVertex3f(-Half, -Half, Half);
    glVertex3f(-Half, Half, Half);
    glVertex3f(-Half, Half, -Half);
    // Right
    glVertex3f(Half, -Half, Half);
    glVertex3f(Half, -Half, -Half);
    glVertex3f(Half, Half, -Half);
    glVertex3f(Half, Half, Half);
    // Top
    glVertex3f(-Half, Half, Half);
    glVertex3f(Half, Half, Half);
    glVertex3f(Half, Half, -Half);
    glVertex3f(-Half, Half, -Half);
    // Bottom
    glVertex3f(-Half, -Half, -Half);
    glVertex3f(Half, -Half, -Half);
    glVertex3f(Half, -Half, Half);
    glVertex3f(-Half, -Half, Half);
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

int RunServer(const Args& Parsed)
{
    RegisterExampleTypes();

    NetSession Session(MakeNetConfig());
    Session.Role(ESessionRole::Server);
    SessionListener Listener("Server", &Session);
    Session.AddListener(&Listener);

    auto Transport = MakeUdpTransport(NetEndpoint{Parsed.Bind, Parsed.Port});
    if (!Transport)
    {
        std::cerr << "Failed to open UDP transport\n";
        return 1;
    }
    Session.RegisterTransport(Transport);

    World Graph("ServerWorld");
    if (!Graph.Networking().AttachSession(Session))
    {
        std::cerr << "Failed to attach world networking\n";
        return 1;
    }

    std::vector<MovingCube> Cubes;
#if defined(SNAPI_GF_ENABLE_AUDIO)
    AudioSourceComponent* ReplicatedAudio = nullptr;
#endif
    const int Grid = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(Parsed.CubeCount))));
    const float Spacing = 4.0f;
    const float Offset = (Grid - 1) * Spacing * 0.5f;

    for (int i = 0; i < Parsed.CubeCount; ++i)
    {
        const float X = (i % Grid) * Spacing - Offset;
        const float Z = (i / Grid) * Spacing - Offset;

        auto NodeResult = Graph.CreateNode("Cube_" + std::to_string(i));
        if (!NodeResult)
        {
            continue;
        }
        auto* Node = NodeResult->Borrowed();
        if (!Node)
        {
            continue;
        }
        Node->Replicated(true);
        auto ComponentResult = Node->Add<ReplicatedTransformComponent>();
        if (!ComponentResult)
        {
            continue;
        }
        auto* Transform = &*ComponentResult;
        Transform->Replicated(true);
        Transform->Position = Vec3(X, 0.0f, Z);
        Transform->Scale = Vec3(1.0f, 1.0f, 1.0f);
        Cubes.push_back({Transform, Vec3(X, 0.0f, Z), static_cast<float>(i) * 0.35f});
    }

#if defined(SNAPI_GF_ENABLE_AUDIO)
    {
        auto AudioNodeResult = Graph.CreateNode("ReplicatedAudio");
        if (AudioNodeResult)
        {
            auto* AudioNode = AudioNodeResult->Borrowed();
            if (AudioNode)
            {
                AudioNode->Replicated(true);
                auto TransformResult = AudioNode->Add<ReplicatedTransformComponent>();
                if (TransformResult)
                {
                    auto* Transform = &*TransformResult;
                    Transform->Replicated(true);
                    Transform->Position = Vec3(0.0f, 0.0f, 0.0f);
                }

                auto AudioResult = AudioNode->Add<AudioSourceComponent>();
                if (AudioResult)
                {
                    ReplicatedAudio = &*AudioResult;
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

    std::cout << "Server listening on " << Parsed.Bind << ":" << Parsed.Port << "\n";

    const auto Start = Clock::now();
    auto Previous = Start;
    auto NextLog = Start;
#if defined(SNAPI_GF_ENABLE_AUDIO)
    float NextSoundTime = 0.0f;
#endif
    while (true)
    {
        const auto Now = Clock::now();
        const float DeltaSeconds = std::chrono::duration<float>(Now - Previous).count();
        Previous = Now;
        const float TimeSeconds = std::chrono::duration<float>(Now - Start).count();

        for (auto& Cube : Cubes)
        {
            if (!Cube.Transform)
            {
                continue;
            }
            const float Angle = TimeSeconds + Cube.Phase;
            const float Radius = 1.5f;
            Cube.Transform->Position = Vec3(
                Cube.Center.X + std::cos(Angle) * Radius,
                Cube.Center.Y,
                Cube.Center.Z + std::sin(Angle) * Radius);
        }

        Session.Pump(Now);
        Graph.Tick(DeltaSeconds);

#if defined(SNAPI_GF_ENABLE_AUDIO)
        if (ReplicatedAudio && TimeSeconds >= NextSoundTime)
        {
            const auto Dumps = Session.DumpConnections(Now);
            const bool HasReadyConnection = std::any_of(Dumps.begin(),
                                                        Dumps.end(),
                                                        [](const NetConnectionDump& Dump) {
                                                            return Dump.HandshakeComplete;
                                                        });
            if (HasReadyConnection)
            {
                ReplicatedAudio->Play();
                std::cout << "[Server] Triggered replicated audio playback: sound.wav\n";
                NextSoundTime = TimeSeconds + 5.0f;
            }
        }
#endif

        if (Now >= NextLog)
        {
            const auto Dumps = Session.DumpConnections(Now);
            for (const auto& Dump : Dumps)
            {
                PrintConnectionDump("Server", Dump, TimeSeconds);
            }
            NextLog = Now + std::chrono::seconds(1);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

int RunClient(const Args& Parsed)
{
    RegisterExampleTypes();

    NetSession Session(MakeNetConfig());
    Session.Role(ESessionRole::Client);
    SessionListener Listener("Client", &Session);
    Session.AddListener(&Listener);

    auto Transport = MakeUdpTransport(NetEndpoint{Parsed.Bind, 0});
    if (!Transport)
    {
        std::cerr << "Failed to open UDP transport\n";
        return 1;
    }
    Session.RegisterTransport(Transport);
    Session.OpenConnection(Transport->Handle(), NetEndpoint{Parsed.Host, Parsed.Port});

    World Graph("ClientWorld");
    if (!Graph.Networking().AttachSession(Session))
    {
        std::cerr << "Failed to attach world networking\n";
        return 1;
    }

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    GLFWwindow* Window = glfwCreateWindow(1024, 768, "MultiplayerExample", nullptr, nullptr);
    if (!Window)
    {
        glfwTerminate();
        std::cerr << "Failed to create window\n";
        return 1;
    }
    glfwMakeContextCurrent(Window);
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);

    const auto Start = Clock::now();
    auto Previous = Start;
    auto NextLog = Start;
    while (!glfwWindowShouldClose(Window))
    {
        const auto Now = Clock::now();
        const float DeltaSeconds = std::chrono::duration<float>(Now - Previous).count();
        Previous = Now;
        const float TimeSeconds = std::chrono::duration<float>(Now - Start).count();
        Session.Pump(Now);
        Graph.Tick(DeltaSeconds);

        if (Now >= NextLog)
        {
            const auto Dumps = Session.DumpConnections(Now);
            for (const auto& Dump : Dumps)
            {
                PrintConnectionDump("Client", Dump, TimeSeconds);
            }
            NextLog = Now + std::chrono::seconds(1);
        }

        int Width = 0;
        int Height = 0;
        glfwGetFramebufferSize(Window, &Width, &Height);
        glViewport(0, 0, Width, Height);

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        SetupCamera(Width, Height);

        Graph.NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
            auto TransformResult = Node.Component<ReplicatedTransformComponent>();
            if (!TransformResult)
            {
                return;
            }
            const auto& Transform = *TransformResult;
            glPushMatrix();
            glTranslatef(Transform.Position.X, Transform.Position.Y, Transform.Position.Z);
            glScalef(Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z);
            glColor3f(0.4f, 0.8f, 0.9f);
            DrawCube(1.0f);
            glPopMatrix();
        });

        glfwSwapBuffers(Window);
        glfwPollEvents();
    }

    glfwDestroyWindow(Window);
    glfwTerminate();
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
        return RunServer(Parsed);
    }

    return RunClient(Parsed);
}
