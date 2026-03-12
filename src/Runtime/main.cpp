#include "GameProjectRuntime.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace
{
enum class ERunMode : std::uint8_t
{
    Local = 0,
    Server,
    Client,
    Listen,
};

struct RuntimeOptions
{
    std::string ProjectFilePath{};
    ERunMode Mode = ERunMode::Listen;
    std::string BindAddress = "0.0.0.0";
    std::string HostAddress = "127.0.0.1";
    std::uint16_t Port = 7777;
    bool Headless = false;

    [[nodiscard]] bool WantsWindow() const
    {
        return !Headless && Mode != ERunMode::Server;
    }
};

std::atomic_bool g_keepRunning{true};

void HandleTerminationSignal(const int)
{
    g_keepRunning.store(false);
}

void PrintUsage(const char* Executable)
{
    std::cerr << "Usage: " << Executable << " [options] <project.snproj.json>\n"
              << "Options:\n"
              << "  --project <path>   Project file path (alternative to positional arg)\n"
              << "  --local            Run without networking\n"
              << "  --server           Run as a dedicated server\n"
              << "  --client           Run as a network client\n"
              << "  --listen           Run as a listen server (default)\n"
              << "  --bind <address>   Local bind address for networking\n"
              << "  --host <address>   Remote host address for client/listen connect\n"
              << "  --port <port>      Network port (default: 7777)\n"
              << "  --headless         Disable window, renderer UI bootstrap, and input bootstrap\n"
              << "  --help             Show this help text\n";
}

[[nodiscard]] std::optional<std::uint16_t> ParsePort(const std::string_view Value)
{
    try
    {
        const unsigned long Parsed = std::stoul(std::string(Value));
        if (Parsed > static_cast<unsigned long>(std::numeric_limits<std::uint16_t>::max()))
        {
            return std::nullopt;
        }
        return static_cast<std::uint16_t>(Parsed);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<RuntimeOptions> ParseArgs(const int argc, char** argv, std::string& OutError)
{
    RuntimeOptions Options{};

    for (int Index = 1; Index < argc; ++Index)
    {
        const std::string_view Arg = argv[Index];
        if (Arg == "--project")
        {
            if (Index + 1 >= argc)
            {
                OutError = "Missing value for --project";
                return std::nullopt;
            }
            Options.ProjectFilePath = argv[++Index];
            continue;
        }

        if (Arg == "--local")
        {
            Options.Mode = ERunMode::Local;
            continue;
        }
        if (Arg == "--server")
        {
            Options.Mode = ERunMode::Server;
            continue;
        }
        if (Arg == "--client")
        {
            Options.Mode = ERunMode::Client;
            continue;
        }
        if (Arg == "--listen")
        {
            Options.Mode = ERunMode::Listen;
            continue;
        }
        if (Arg == "--headless")
        {
            Options.Headless = true;
            continue;
        }
        if (Arg == "--bind")
        {
            if (Index + 1 >= argc)
            {
                OutError = "Missing value for --bind";
                return std::nullopt;
            }
            Options.BindAddress = argv[++Index];
            continue;
        }
        if (Arg == "--host")
        {
            if (Index + 1 >= argc)
            {
                OutError = "Missing value for --host";
                return std::nullopt;
            }
            Options.HostAddress = argv[++Index];
            continue;
        }
        if (Arg == "--port")
        {
            if (Index + 1 >= argc)
            {
                OutError = "Missing value for --port";
                return std::nullopt;
            }
            const auto ParsedPort = ParsePort(argv[++Index]);
            if (!ParsedPort)
            {
                OutError = "Invalid --port value";
                return std::nullopt;
            }
            Options.Port = *ParsedPort;
            continue;
        }
        if (!Arg.empty() && Arg.front() == '-')
        {
            OutError = "Unknown option: " + std::string(Arg);
            return std::nullopt;
        }
        if (!Options.ProjectFilePath.empty())
        {
            OutError = "Multiple project file arguments were provided";
            return std::nullopt;
        }

        Options.ProjectFilePath = std::string(Arg);
    }

    if (Options.ProjectFilePath.empty())
    {
        OutError = "A project file path is required";
        return std::nullopt;
    }

    return Options;
}

[[nodiscard]] std::string RuntimeWindowTitle(const RuntimeOptions& Options)
{
    const std::filesystem::path ProjectPath(Options.ProjectFilePath);
    const std::string ProjectName =
        ProjectPath.stem().empty() ? std::string("SnAPI.GameFramework.Runtime") : ProjectPath.stem().string();

    switch (Options.Mode)
    {
    case ERunMode::Server:
        return ProjectName + " [Server]";
    case ERunMode::Client:
        return ProjectName + " [Client]";
    case ERunMode::Listen:
        return ProjectName + " [Listen]";
    case ERunMode::Local:
    default:
        return ProjectName + " [Local]";
    }
}

[[nodiscard]] SnAPI::GameFramework::GameRuntimeSettings BuildRuntimeSettings(const RuntimeOptions& Options)
{
    using namespace SnAPI::GameFramework;

    GameRuntimeSettings Settings{};
    Settings.WorldName = RuntimeWindowTitle(Options);
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.EnableLateTick = true;
    Settings.Tick.EnableEndFrame = true;
    Settings.Tick.MaxFpsWhenVSyncOff = 120.0f;

    GameRuntimeGameplaySettings Gameplay{};
    if (Options.Mode == ERunMode::Server)
    {
        Gameplay.AutoCreateLocalPlayer = false;
        Gameplay.AutoCreateReplicatedLocalPlayer = false;
        Gameplay.RegisterDefaultLocalPlayerService = false;
    }
    Settings.Gameplay = Gameplay;

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (Options.Mode != ERunMode::Local)
    {
        GameRuntimeNetworkingSettings Networking{};
        switch (Options.Mode)
        {
        case ERunMode::Server:
            Networking.Role = SnAPI::Networking::ESessionRole::Server;
            Networking.AutoConnect = false;
            Networking.BindPort = Options.Port;
            break;
        case ERunMode::Client:
            Networking.Role = SnAPI::Networking::ESessionRole::Client;
            Networking.AutoConnect = true;
            Networking.BindPort = 0;
            break;
        case ERunMode::Listen:
            Networking.Role = SnAPI::Networking::ESessionRole::ServerAndClient;
            Networking.AutoConnect = true;
            Networking.BindPort = Options.Port;
            break;
        case ERunMode::Local:
        default:
            break;
        }

        Networking.BindAddress = Options.BindAddress;
        Networking.ConnectAddress = Options.HostAddress;
        Networking.ConnectPort = Options.Port;
        Settings.Networking = Networking;
    }
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Settings.Physics = GameRuntimePhysicsSettings{};
#endif

#if defined(SNAPI_GF_ENABLE_INPUT)
#if (defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3) || \
    (defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) && SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) || \
    (defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) && SNAPI_INPUT_ENABLE_BACKEND_LIBUSB)
    if (Options.WantsWindow())
    {
        GameRuntimeInputSettings InputSettings{};
#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
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
    if (Options.WantsWindow())
    {
        GameRuntimeRendererSettings RendererSettings{};
        RendererSettings.CreateGraphicsApi = true;
        RendererSettings.CreateWindow = true;
        RendererSettings.WindowTitle = RuntimeWindowTitle(Options);
        RendererSettings.WindowWidth = 1920.0f;
        RendererSettings.WindowHeight = 1080.0f;
        RendererSettings.Resizable = true;
        RendererSettings.Visible = true;
        RendererSettings.FullScreen = true;
        RendererSettings.CreateDefaultLighting = true;
        RendererSettings.RegisterDefaultPassGraph = true;
        RendererSettings.CreateDefaultMaterials = true;
        RendererSettings.PreloadDefaultFont = true;
        RendererSettings.CreateDefaultEnvironmentProbe = true;
        Settings.Renderer = RendererSettings;
    }
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    if (Options.WantsWindow())
    {
        GameRuntimeUiSettings UiSettings{};
        UiSettings.ViewportWidth = 1920.0f;
        UiSettings.ViewportHeight = 1080.0f;
        Settings.UI = UiSettings;
    }
#endif

    return Settings;
}
} // namespace

int main(int argc, char** argv)
{
    for (int Index = 1; Index < argc; ++Index)
    {
        if (std::string_view(argv[Index]) == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    std::string ParseError{};
    const auto ParsedOptions = ParseArgs(argc, argv, ParseError);
    if (!ParsedOptions)
    {
        std::cerr << ParseError << '\n';
        PrintUsage(argv[0]);
        return 1;
    }

    std::signal(SIGINT, HandleTerminationSignal);
    std::signal(SIGTERM, HandleTerminationSignal);

    SnAPI::GameFramework::GameProjectRuntime RuntimeApp{};
    SnAPI::GameFramework::GameProjectRuntimeSettings Settings{};
    Settings.ProjectFilePath = ParsedOptions->ProjectFilePath;
    Settings.Runtime = BuildRuntimeSettings(*ParsedOptions);

    if (const auto InitResult = RuntimeApp.Initialize(Settings); !InitResult)
    {
        std::cerr << "Failed to initialize SnAPI.GameFramework.Runtime: "
                  << InitResult.error().Message << '\n';
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    auto LastTick = Clock::now();

    while (g_keepRunning.load() && RuntimeApp.IsInitialized())
    {
        const auto Now = Clock::now();
        float DeltaSeconds = std::chrono::duration<float>(Now - LastTick).count();
        LastTick = Now;
        if (!(DeltaSeconds > 0.0f))
        {
            DeltaSeconds = 1.0f / 60.0f;
        }

        if (!RuntimeApp.Update(DeltaSeconds))
        {
            break;
        }
    }

    RuntimeApp.Shutdown();
    return 0;
}
