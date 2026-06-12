#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;

namespace
{
std::uint32_t ParseFrameCount(const int Argc, char** Argv)
{
    for (int Index = 1; Index + 1 < Argc; ++Index)
    {
        if (std::strcmp(Argv[Index], "--frames") == 0)
        {
            return static_cast<std::uint32_t>(std::max(0, std::atoi(Argv[Index + 1])));
        }
    }
    return 120u;
}

bool BuildDemoScene(GameRuntime& Runtime)
{
    auto LevelResult = Runtime.World().CreateLevel("MultiplayerExampleLevel");
    if (!LevelResult)
    {
        std::cerr << "Failed to create example level: " << LevelResult.error().Message << '\n';
        return false;
    }

    auto* LevelPtr = NodeCast<Level>(LevelResult->Borrowed());
    if (!LevelPtr)
    {
        std::cerr << "Failed to resolve example level\n";
        return false;
    }

    auto CameraNodeResult = LevelPtr->CreateNode("MainCamera");
    if (!CameraNodeResult)
    {
        std::cerr << "Failed to create camera node: " << CameraNodeResult.error().Message << '\n';
        return false;
    }
    auto* CameraNode = CameraNodeResult->Borrowed();
    auto CameraTransform = CameraNode->Add<TransformComponent>();
    auto Camera = CameraNode->Add<CameraComponent>();
    if (!CameraTransform || !Camera)
    {
        std::cerr << "Failed to create camera components\n";
        return false;
    }
    CameraTransform->Position = Vec3(0.0f, 1.5f, 5.0f);
    Camera->EditSettings().FovDegrees = 65.0f;
    Camera->SetActive(true);

    auto LightNodeResult = LevelPtr->CreateNode("KeyLight");
    if (!LightNodeResult)
    {
        std::cerr << "Failed to create light node: " << LightNodeResult.error().Message << '\n';
        return false;
    }
    auto Light = LightNodeResult->Borrowed()->Add<DirectionalLightComponent>();
    if (!Light)
    {
        std::cerr << "Failed to create light component\n";
        return false;
    }
    Light->EditSettings().Direction = Vec3(-0.35f, -1.0f, -0.4f);
    Light->EditSettings().Intensity = 3.0f;

    auto CubeNodeResult = LevelPtr->CreateNode("NetworkedCubePreview");
    if (!CubeNodeResult)
    {
        std::cerr << "Failed to create preview node: " << CubeNodeResult.error().Message << '\n';
        return false;
    }
    auto* CubeNode = CubeNodeResult->Borrowed();
    auto CubeTransform = CubeNode->Add<TransformComponent>();
    auto CubeMesh = CubeNode->Add<StaticMeshComponent>();
    if (!CubeTransform || !CubeMesh)
    {
        std::cerr << "Failed to create preview mesh components\n";
        return false;
    }
    CubeTransform->Scale = Vec3(1.5f, 1.5f, 1.5f);
    CubeMesh->EditSettings().MeshPath = "primitive://box";
    if (!CubeMesh->ReloadMesh())
    {
        std::cerr << "Failed to load primitive preview mesh\n";
        return false;
    }

    return true;
}
} // namespace

int main(int Argc, char** Argv)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER)
    std::cerr << "MultiplayerExample requires SNAPI_GF_ENABLE_RENDERER\n";
    return 1;
#else
    RegisterBuiltinTypes();

    GameRuntimeSettings Settings{};
    Settings.WorldName = "MultiplayerExampleWorld";

    RendererBootstrapSettings RendererSettings{};
    RendererSettings.WindowTitle = "SnAPI Multiplayer Example";
    RendererSettings.WindowWidth = 1280.0f;
    RendererSettings.WindowHeight = 720.0f;
    RendererSettings.CreateDefaultLighting = false;
    RendererSettings.ApplyDefaultFeatureProfile = true;
    Settings.Renderer = RendererSettings;

    GameRuntime Runtime{};
    if (auto InitResult = Runtime.Init(Settings); !InitResult)
    {
        std::cerr << "Runtime init failed: " << InitResult.error().Message << '\n';
        return 1;
    }

    if (!BuildDemoScene(Runtime))
    {
        Runtime.Shutdown();
        return 1;
    }

    const std::uint32_t FrameCount = ParseFrameCount(Argc, Argv);
    constexpr float DeltaSeconds = 1.0f / 60.0f;
    for (std::uint32_t Frame = 0; Frame < FrameCount; ++Frame)
    {
        if (!Runtime.Update(DeltaSeconds))
        {
            break;
        }
    }

    Runtime.Shutdown();
    return 0;
#endif
}
