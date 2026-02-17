#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
    #include <SDL3/SDL.h>
#endif

using namespace SnAPI::GameFramework;

#if defined(SNAPI_GF_ENABLE_INPUT)

#if (defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3) || \
    (defined(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) && SNAPI_INPUT_ENABLE_BACKEND_HIDAPI) || \
    (defined(SNAPI_INPUT_ENABLE_BACKEND_LIBUSB) && SNAPI_INPUT_ENABLE_BACKEND_LIBUSB)
TEST_CASE("GameRuntime initializes world input subsystem when configured")
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings{};
    Settings.WorldName = "RuntimeInputWorld";
    Settings.RegisterBuiltins = true;
    Settings.Input = GameRuntimeInputSettings{};

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.World().Input().IsInitialized());
    REQUIRE(Runtime.World().Input().Context() != nullptr);

    Runtime.Update(0.016f);

    Runtime.Shutdown();
    REQUIRE(Runtime.WorldPtr() == nullptr);
}
#endif

#if defined(SNAPI_INPUT_ENABLE_BACKEND_SDL3) && SNAPI_INPUT_ENABLE_BACKEND_SDL3
TEST_CASE("World tick pumps input subsystem and updates snapshot")
{
    World WorldRef{"WorldInputPump"};

    InputBootstrapSettings Settings{};
    Settings.Backend = SnAPI::Input::EInputBackend::SDL3;
    Settings.RegisterSdl3Backend = true;
    Settings.CreateDesc.EnableKeyboard = false;
    Settings.CreateDesc.EnableMouse = true;
    Settings.CreateDesc.EnableGamepad = false;
    Settings.CreateDesc.EnableTextInput = false;
    REQUIRE(WorldRef.Input().Initialize(Settings));

    auto* Snapshot = WorldRef.Input().Snapshot();
    REQUIRE(Snapshot != nullptr);

    SDL_Event UpEvent{};
    UpEvent.type = SDL_EVENT_MOUSE_BUTTON_UP;
    UpEvent.button.button = SDL_BUTTON_LEFT;
    REQUIRE(SDL_PushEvent(&UpEvent));
    WorldRef.Tick(0.016f);

    SDL_Event DownEvent{};
    DownEvent.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    DownEvent.button.button = SDL_BUTTON_LEFT;
    REQUIRE(SDL_PushEvent(&DownEvent));
    WorldRef.Tick(0.016f);

    Snapshot = WorldRef.Input().Snapshot();
    REQUIRE(Snapshot != nullptr);
    CHECK(Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Left));
    CHECK(Snapshot->MouseButtonPressed(SnAPI::Input::EMouseButton::Left));
}
#endif

#endif // SNAPI_GF_ENABLE_INPUT

