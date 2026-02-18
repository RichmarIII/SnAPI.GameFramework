# Input System

This tutorial shows how `SnAPI.GameFramework` integrates `SnAPI.Input` as a world-owned subsystem.

By the end, you will understand:

1. How to initialize `InputSystem` through `GameRuntime`.
2. Where input is pumped in frame flow.
3. How to read normalized snapshot/event data.
4. How to access action maps and backend-specific device interfaces.

## Mental Model

- `World` owns one `InputSystem` (`World::Input()`).
- `InputSystem` owns one `SnAPI::Input::InputContext`.
- `World::Tick(...)` pumps input first, then runs other world subsystems (for example UI), then node/component `Tick(...)`.
- Your gameplay logic reads current-frame input state from `InputSystem`.

## 1) Initialize Input Through GameRuntime

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

int main()
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings{};
    Settings.WorldName = "InputWorld";

    GameRuntimeInputSettings Input{};
    Input.Backend = SnAPI::Input::EInputBackend::SDL3;
    Input.CreateDesc.EnableKeyboard = true;
    Input.CreateDesc.EnableMouse = true;
    Input.CreateDesc.EnableGamepad = true;
    Input.CreateDesc.EnableTextInput = true;
    Settings.Input = Input;

    auto InitResult = Runtime.Init(Settings);
    if (!InitResult)
    {
        return 1;
    }

    while (Runtime.IsInitialized())
    {
        Runtime.Update(1.0f / 60.0f);
    }
}
```

Notes:

- `Settings.Input = std::nullopt` leaves input disabled.
- Backend registration is controlled by `InputBootstrapSettings` (`RegisterSdl3Backend`, `RegisterHidApiBackend`, `RegisterLibUsbBackend`).

## 2) Read Normalized Input In Gameplay Tick

```cpp
struct PlayerNode final : BaseNode
{
    static constexpr const char* kTypeName = "PlayerNode";

    void Tick(float DeltaSeconds) override
    {
        (void)DeltaSeconds;

        if (!Owner() || !Owner()->World())
        {
            return;
        }

        const auto* Snapshot = Owner()->World()->Input().Snapshot();
        if (!Snapshot)
        {
            return;
        }

        if (Snapshot->KeyDown(SnAPI::Input::EKey::W))
        {
            // Move forward.
        }

        if (Snapshot->MouseButtonPressed(SnAPI::Input::EMouseButton::Left))
        {
            // Fire on edge-trigger.
        }
    }
};
```

`Snapshot` values are normalized:

- gamepad sticks are `[-1, 1]`
- triggers are `[0, 1]`
- keyboard/mouse buttons include `Down`, `Pressed`, `Released`

## 3) Consume Frame Events

```cpp
const auto* Events = WorldRef.Input().Events();
if (Events)
{
    for (const SnAPI::Input::InputEvent& Event : *Events)
    {
        if (Event.Type == SnAPI::Input::EInputEventType::DeviceConnected)
        {
            // React to hotplug.
        }
    }
}
```

Use snapshot for gameplay state polling and events for edge/callback style logic.

## 4) Action Maps

`InputContext` owns an `ActionMap` for rebind-friendly gameplay actions:

```cpp
auto* Actions = WorldRef.Input().Actions();
if (Actions)
{
    SnAPI::Input::ActionDescriptor Jump{};
    Jump.Id = 1;
    Jump.Name = "Jump";
    Jump.Kind = SnAPI::Input::EActionKind::Boolean;
    (void)Actions->RegisterAction(Jump);
}
```

Action maps let you keep gameplay logic backend-agnostic and binding-driven.

## 5) Raw Device Access (HID/USB)

For raw protocols/schemas:

- enumerate devices through `World::Input().Context()->Devices()`
- cast to `IHidDevice` or `IUsbDevice` where appropriate
- use `THidReportView<TSchema>` / `TUsbControlView<TSchema>` for type-safe packet layouts

This is useful for vendor-specific HID/USB hardware while keeping gameplay on normalized input paths.
