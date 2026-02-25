#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace
{
using Clock = std::chrono::steady_clock;

enum class ETickMode
{
    EcsOnly
};

struct BenchmarkOptions
{
    std::uint64_t NodeCount = 1'000'000;
    std::uint32_t WarmupFrames = 0;
    std::uint32_t MeasuredFrames = 1;
    float DeltaSeconds = 1.0f / 60.0f;
    float FixedDeltaSeconds = 1.0f / 60.0f;
    ETickMode Mode = ETickMode::EcsOnly;
    std::filesystem::path OutputPath = "benchmarks/node_component_tick_ecs.txt";
};

struct ComponentSpec
{
    const char* Name = "";
    bool (*Add)(BaseNode&, std::string&) = nullptr;
};

struct RunResult
{
    std::uint64_t CreatedNodes = 0;
    std::uint64_t CreatedComponents = 0;
    double PopulateMs = 0.0;
    double WarmupMs = 0.0;
    double TickTotalMs = 0.0;
    double FixedTotalMs = 0.0;
    double LateTotalMs = 0.0;
    double EndFrameTotalMs = 0.0;
    double FrameTotalMs = 0.0;
};

double ToMilliseconds(const Clock::duration Duration)
{
    return std::chrono::duration<double, std::milli>(Duration).count();
}

template<typename T>
bool AddComponentToNode(BaseNode& Node, std::string& OutError)
{
    auto AddResult = Node.Add<T>();
    if (!AddResult)
    {
        OutError = AddResult.error().Message;
        return false;
    }
    return true;
}

std::vector<ComponentSpec> BuildComponentSpecs()
{
    std::vector<ComponentSpec> Specs{};
    Specs.reserve(16);

    Specs.push_back({"TransformComponent", &AddComponentToNode<TransformComponent>});
    Specs.push_back({"FollowTargetComponent", &AddComponentToNode<FollowTargetComponent>});
    Specs.push_back({"RelevanceComponent", &AddComponentToNode<RelevanceComponent>});
    Specs.push_back({"ScriptComponent", &AddComponentToNode<ScriptComponent>});

#if defined(SNAPI_GF_ENABLE_AUDIO)
    Specs.push_back({"AudioSourceComponent", &AddComponentToNode<AudioSourceComponent>});
    Specs.push_back({"AudioListenerComponent", &AddComponentToNode<AudioListenerComponent>});
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs.push_back({"ColliderComponent", &AddComponentToNode<ColliderComponent>});
    Specs.push_back({"RigidBodyComponent", &AddComponentToNode<RigidBodyComponent>});
    Specs.push_back({"CharacterMovementController", &AddComponentToNode<CharacterMovementController>});
#if defined(SNAPI_GF_ENABLE_INPUT)
    Specs.push_back({"InputComponent", &AddComponentToNode<InputComponent>});
#endif
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    Specs.push_back({"CameraComponent", &AddComponentToNode<CameraComponent>});
    Specs.push_back({"StaticMeshComponent", &AddComponentToNode<StaticMeshComponent>});
    Specs.push_back({"SkeletalMeshComponent", &AddComponentToNode<SkeletalMeshComponent>});
#endif

    return Specs;
}

std::string JoinComponentNames(const std::vector<ComponentSpec>& Specs)
{
    std::ostringstream Stream{};
    for (std::size_t Index = 0; Index < Specs.size(); ++Index)
    {
        if (Index != 0)
        {
            Stream << ", ";
        }
        Stream << Specs[Index].Name;
    }
    return Stream.str();
}

std::string UtcTimestampNow()
{
    const std::time_t Now = std::time(nullptr);
    std::tm UtcTime{};
#if defined(_WIN32)
    gmtime_s(&UtcTime, &Now);
#else
    gmtime_r(&Now, &UtcTime);
#endif
    std::ostringstream Stream{};
    Stream << std::put_time(&UtcTime, "%Y-%m-%dT%H:%M:%SZ");
    return Stream.str();
}

bool ParseUnsigned(const std::string& Value, std::uint64_t& Out)
{
    try
    {
        std::size_t Consumed = 0;
        Out = std::stoull(Value, &Consumed);
        return Consumed == Value.size();
    }
    catch (...)
    {
        return false;
    }
}

bool ParseFloat(const std::string& Value, float& Out)
{
    try
    {
        std::size_t Consumed = 0;
        Out = std::stof(Value, &Consumed);
        return Consumed == Value.size();
    }
    catch (...)
    {
        return false;
    }
}

std::string_view TickModeName(const ETickMode Mode)
{
    switch (Mode)
    {
    case ETickMode::EcsOnly:
        return "ecs";
    }
    return "unknown";
}

bool ParseTickMode(const std::string_view Value, ETickMode& OutMode)
{
    if (Value == "ecs")
    {
        OutMode = ETickMode::EcsOnly;
        return true;
    }
    return false;
}

void PrintUsage(const char* ProgramName)
{
    std::cerr << "Usage: " << ProgramName
              << " [--nodes <count>] [--warmup <frames>] [--frames <frames>]"
              << " [--dt <seconds>] [--fixed-dt <seconds>]"
              << " [--mode <ecs>] [--output <path>]\n";
}

bool ParseArgs(const int Argc, char** Argv, BenchmarkOptions& OutOptions)
{
    for (int Index = 1; Index < Argc; ++Index)
    {
        const std::string_view Arg = Argv[Index];
        if (Arg == "--nodes")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            std::uint64_t Parsed = 0;
            if (!ParseUnsigned(Argv[++Index], Parsed))
            {
                return false;
            }
            OutOptions.NodeCount = Parsed;
            continue;
        }
        if (Arg == "--warmup")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            std::uint64_t Parsed = 0;
            if (!ParseUnsigned(Argv[++Index], Parsed) || Parsed > std::numeric_limits<std::uint32_t>::max())
            {
                return false;
            }
            OutOptions.WarmupFrames = static_cast<std::uint32_t>(Parsed);
            continue;
        }
        if (Arg == "--frames")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            std::uint64_t Parsed = 0;
            if (!ParseUnsigned(Argv[++Index], Parsed) || Parsed > std::numeric_limits<std::uint32_t>::max())
            {
                return false;
            }
            OutOptions.MeasuredFrames = static_cast<std::uint32_t>(Parsed);
            continue;
        }
        if (Arg == "--dt")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            float Parsed = 0.0f;
            if (!ParseFloat(Argv[++Index], Parsed))
            {
                return false;
            }
            OutOptions.DeltaSeconds = Parsed;
            continue;
        }
        if (Arg == "--fixed-dt")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            float Parsed = 0.0f;
            if (!ParseFloat(Argv[++Index], Parsed))
            {
                return false;
            }
            OutOptions.FixedDeltaSeconds = Parsed;
            continue;
        }
        if (Arg == "--mode")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            ETickMode Parsed = ETickMode::EcsOnly;
            if (!ParseTickMode(Argv[++Index], Parsed))
            {
                return false;
            }
            OutOptions.Mode = Parsed;
            continue;
        }
        if (Arg == "--output")
        {
            if (Index + 1 >= Argc)
            {
                return false;
            }
            OutOptions.OutputPath = Argv[++Index];
            continue;
        }
        if (Arg == "--help" || Arg == "-h")
        {
            PrintUsage(Argv[0]);
            return false;
        }

        std::cerr << "Unknown argument: " << Arg << "\n";
        return false;
    }

    if (OutOptions.NodeCount == 0 || OutOptions.MeasuredFrames == 0)
    {
        return false;
    }
    if (OutOptions.DeltaSeconds <= 0.0f || OutOptions.FixedDeltaSeconds <= 0.0f)
    {
        return false;
    }

    return true;
}

bool WriteResultsFile(const BenchmarkOptions& Options,
                      const std::filesystem::path& OutputPath,
                      const ETickMode Mode,
                      const RunResult& Results,
                      const std::vector<ComponentSpec>& ComponentSpecs)
{
    std::error_code Error{};
    const auto Parent = OutputPath.parent_path();
    if (!Parent.empty())
    {
        std::filesystem::create_directories(Parent, Error);
        if (Error)
        {
            std::cerr << "Failed to create output directory '" << Parent.string()
                      << "': " << Error.message() << "\n";
            return false;
        }
    }

    std::ofstream Output(OutputPath);
    if (!Output.is_open())
    {
        std::cerr << "Failed to open output file: " << OutputPath << "\n";
        return false;
    }

    const double MeasuredFrames = static_cast<double>(Options.MeasuredFrames);
    const double AvgTickMs = Results.TickTotalMs / MeasuredFrames;
    const double AvgFixedMs = Results.FixedTotalMs / MeasuredFrames;
    const double AvgLateMs = Results.LateTotalMs / MeasuredFrames;
    const double AvgEndFrameMs = Results.EndFrameTotalMs / MeasuredFrames;
    const double AvgFrameMs = Results.FrameTotalMs / MeasuredFrames;
    const double ApproxFps = (AvgFrameMs > 0.0) ? (1000.0 / AvgFrameMs) : 0.0;

    Output << std::fixed << std::setprecision(4);
    Output << "benchmark_name=NodeComponentTickBenchmark\n";
    Output << "timestamp_utc=" << UtcTimestampNow() << "\n";
    Output << "tick_mode=" << TickModeName(Mode) << "\n";
    Output << "ecs_runtime_tick_enabled=true\n";
    Output << "node_count_requested=" << Options.NodeCount << "\n";
    Output << "node_count_created=" << Results.CreatedNodes << "\n";
    Output << "component_types_count=" << ComponentSpecs.size() << "\n";
    Output << "component_types=" << JoinComponentNames(ComponentSpecs) << "\n";
    Output << "total_components_created=" << Results.CreatedComponents << "\n";
    Output << "warmup_frames=" << Options.WarmupFrames << "\n";
    Output << "measured_frames=" << Options.MeasuredFrames << "\n";
    Output << "delta_seconds=" << Options.DeltaSeconds << "\n";
    Output << "fixed_delta_seconds=" << Options.FixedDeltaSeconds << "\n";
    Output << "populate_ms=" << Results.PopulateMs << "\n";
    Output << "warmup_total_ms=" << Results.WarmupMs << "\n";
    Output << "tick_total_ms=" << Results.TickTotalMs << "\n";
    Output << "fixed_tick_total_ms=" << Results.FixedTotalMs << "\n";
    Output << "late_tick_total_ms=" << Results.LateTotalMs << "\n";
    Output << "end_frame_total_ms=" << Results.EndFrameTotalMs << "\n";
    Output << "frame_total_ms=" << Results.FrameTotalMs << "\n";
    Output << "tick_avg_ms=" << AvgTickMs << "\n";
    Output << "fixed_tick_avg_ms=" << AvgFixedMs << "\n";
    Output << "late_tick_avg_ms=" << AvgLateMs << "\n";
    Output << "end_frame_avg_ms=" << AvgEndFrameMs << "\n";
    Output << "frame_avg_ms=" << AvgFrameMs << "\n";
    Output << "approx_fps=" << ApproxFps << "\n";

    return true;
}

void ConfigureWorldForMode(World& WorldInstance)
{
    WorldExecutionProfile Profile = WorldExecutionProfile::Runtime();
    Profile.RunGameplay = false;
    Profile.TickInput = false;
    Profile.TickUI = false;
    Profile.PumpNetworking = false;
    Profile.TickPhysicsSimulation = false;
    Profile.AllowPhysicsQueries = true;
    Profile.TickAudio = false;
    Profile.BuildUiRenderPackets = false;
    Profile.RenderFrame = false;
    Profile.TickEcsRuntime = true;
    Profile.RunNodeEndFrame = true;
    WorldInstance.SetExecutionProfile(Profile);
}

bool RunScenario(const BenchmarkOptions& Options,
                 const std::vector<ComponentSpec>& ComponentSpecs,
                 const std::filesystem::path& OutputPath,
                 RunResult& OutResults)
{
    std::cout << "\nScenario: " << TickModeName(Options.Mode) << "\n";
    std::cout << "  Output: " << OutputPath << "\n";

    World WorldInstance("NodeComponentTickBenchmarkWorld");
    ConfigureWorldForMode(WorldInstance);

    OutResults = RunResult{};
    const auto PopulateStart = Clock::now();
    try
    {
        for (std::uint64_t Index = 0; Index < Options.NodeCount; ++Index)
        {
            auto NodeResult = WorldInstance.CreateNode<BaseNode>("N");
            if (!NodeResult)
            {
                std::cerr << "Failed to create node " << Index << ": " << NodeResult.error().Message << "\n";
                return false;
            }

            auto* Node = NodeResult->Borrowed();
            if (!Node)
            {
                std::cerr << "Failed to resolve node " << Index << " after creation\n";
                return false;
            }

            for (const ComponentSpec& Spec : ComponentSpecs)
            {
                std::string ErrorMessage{};
                if (!Spec.Add(*Node, ErrorMessage))
                {
                    std::cerr << "Failed to add component '" << Spec.Name << "' on node " << Index
                              << ": " << ErrorMessage << "\n";
                    return false;
                }
                ++OutResults.CreatedComponents;
            }

            ++OutResults.CreatedNodes;
            if (((Index + 1) % 25'000) == 0 || (Index + 1) == Options.NodeCount)
            {
                const double Percent = static_cast<double>(Index + 1) * 100.0 / static_cast<double>(Options.NodeCount);
                std::cout << "  Populate progress: " << (Index + 1) << "/" << Options.NodeCount
                          << " (" << std::setprecision(2) << std::fixed << Percent << "%)\n";
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "Out-of-memory while creating benchmark scene.\n";
        return false;
    }

    const auto PopulateEnd = Clock::now();
    OutResults.PopulateMs = ToMilliseconds(PopulateEnd - PopulateStart);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Population complete in " << OutResults.PopulateMs << " ms\n";

    const auto WarmupStart = Clock::now();
    for (std::uint32_t Frame = 0; Frame < Options.WarmupFrames; ++Frame)
    {
        WorldInstance.Tick(Options.DeltaSeconds);
        WorldInstance.FixedTick(Options.FixedDeltaSeconds);
        WorldInstance.LateTick(Options.DeltaSeconds);
        WorldInstance.EndFrame();
    }
    const auto WarmupEnd = Clock::now();
    OutResults.WarmupMs = ToMilliseconds(WarmupEnd - WarmupStart);

    for (std::uint32_t Frame = 0; Frame < Options.MeasuredFrames; ++Frame)
    {
        const auto FrameStart = Clock::now();

        const auto TickStart = Clock::now();
        WorldInstance.Tick(Options.DeltaSeconds);
        const auto TickEnd = Clock::now();

        WorldInstance.FixedTick(Options.FixedDeltaSeconds);
        const auto FixedEnd = Clock::now();

        WorldInstance.LateTick(Options.DeltaSeconds);
        const auto LateEnd = Clock::now();

        WorldInstance.EndFrame();
        const auto EndFrameEnd = Clock::now();

        OutResults.TickTotalMs += ToMilliseconds(TickEnd - TickStart);
        OutResults.FixedTotalMs += ToMilliseconds(FixedEnd - TickEnd);
        OutResults.LateTotalMs += ToMilliseconds(LateEnd - FixedEnd);
        OutResults.EndFrameTotalMs += ToMilliseconds(EndFrameEnd - LateEnd);
        OutResults.FrameTotalMs += ToMilliseconds(EndFrameEnd - FrameStart);

        std::cout << "  Frame " << (Frame + 1) << "/" << Options.MeasuredFrames
                  << " total: " << ToMilliseconds(EndFrameEnd - FrameStart) << " ms\n";
    }

    if (!WriteResultsFile(Options, OutputPath, Options.Mode, OutResults, ComponentSpecs))
    {
        return false;
    }

    const double MeasuredFrames = static_cast<double>(Options.MeasuredFrames);
    std::cout << "Measured averages (ms):"
              << " Tick=" << (OutResults.TickTotalMs / MeasuredFrames)
              << " Fixed=" << (OutResults.FixedTotalMs / MeasuredFrames)
              << " Late=" << (OutResults.LateTotalMs / MeasuredFrames)
              << " EndFrame=" << (OutResults.EndFrameTotalMs / MeasuredFrames)
              << " Frame=" << (OutResults.FrameTotalMs / MeasuredFrames) << "\n";
    std::cout << "Results written to " << OutputPath << "\n";

    return true;
}

} // namespace

int main(int Argc, char** Argv)
{
    BenchmarkOptions Options{};
    if (!ParseArgs(Argc, Argv, Options))
    {
        PrintUsage(Argv[0]);
        return 1;
    }

    RegisterBuiltinTypes();
    const std::vector<ComponentSpec> ComponentSpecs = BuildComponentSpecs();
    if (ComponentSpecs.empty())
    {
        std::cerr << "No runtime component types available for this build.\n";
        return 1;
    }

    std::cout << "NodeComponentTickBenchmark configuration\n";
    std::cout << "  Nodes: " << Options.NodeCount << "\n";
    std::cout << "  Component types per node: " << ComponentSpecs.size() << "\n";
    std::cout << "  Component list: " << JoinComponentNames(ComponentSpecs) << "\n";
    std::cout << "  Warmup frames: " << Options.WarmupFrames << "\n";
    std::cout << "  Measured frames: " << Options.MeasuredFrames << "\n";
    std::cout << "  Mode: " << TickModeName(Options.Mode) << "\n";
    std::cout << "  Output: " << Options.OutputPath << "\n";

    RunResult Result{};
    if (!RunScenario(Options, ComponentSpecs, Options.OutputPath, Result))
    {
        return 1;
    }

    return 0;
}
