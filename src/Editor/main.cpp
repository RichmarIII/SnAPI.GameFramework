#include "Editor/GameEditor.h"

#include <chrono>
#include <iostream>

int main()
{
    using namespace SnAPI::GameFramework;
    using namespace SnAPI::GameFramework::Editor;

    GameEditor EditorApp{};
    GameEditorSettings Settings{};
    Settings.Runtime.WorldName = "SnAPI.GameFramework.Editor";
    Settings.Runtime.Tick.EnableFixedTick = true;
    Settings.Runtime.Tick.EnableLateTick = true;
    Settings.Runtime.Tick.MaxFpsWhenVSyncOff = 120;

#if defined(SNAPI_GF_ENABLE_RENDERER)
    GameRuntimeRendererSettings RendererSettings{};
    RendererSettings.CreateGraphicsApi = true;
    RendererSettings.CreateWindow = true;
    RendererSettings.WindowTitle = "SnAPI.GameFramework.Editor";
    RendererSettings.WindowWidth = 1920.0f;
    RendererSettings.WindowHeight = 1080.0f;
    RendererSettings.RegisterDefaultPassGraph = false;
    Settings.Runtime.Renderer = RendererSettings;
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    GameRuntimeUiSettings UiSettings{};
    UiSettings.ViewportWidth = 1920.0f;
    UiSettings.ViewportHeight = 1080.0f;
    Settings.Runtime.UI = UiSettings;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    GameRuntimePhysicsSettings PhysicsSettings{};
    Settings.Runtime.Physics = PhysicsSettings;
#endif

    if (auto InitResult = EditorApp.Initialize(Settings); !InitResult)
    {
        std::cerr << "Failed to initialize SnAPI.GameFramework.Editor: " << InitResult.error().Message << std::endl;
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    auto LastTick = Clock::now();

#if defined(SNAPI_GF_ENABLE_RENDERER)
    while (EditorApp.IsInitialized())
    {
        const auto Now = Clock::now();
        float DeltaSeconds = std::chrono::duration<float>(Now - LastTick).count();
        LastTick = Now;
        if (!(DeltaSeconds > 0.0f))
        {
            DeltaSeconds = 1.0f / 60.0f;
        }

        if (!EditorApp.Update(DeltaSeconds))
        {
            break;
        }
    }
#else
    (void)EditorApp.Update(1.0f / 60.0f);
#endif

    EditorApp.Shutdown();
    return 0;
}
