# UI System

This tutorial explains how `SnAPI.GameFramework` integrates `SnAPI.UI` as a world-owned subsystem.

By the end, you will understand:

1. How to initialize `UISystem` through `GameRuntime`.
2. Where UI ticks in world frame flow.
3. How to feed input events into the UI context.
4. How to build per-frame UI render packets.

## Mental Model

- `World` owns one `UISystem` (`World::UI()`).
- `UISystem` owns one `SnAPI::UI::UIContext`.
- `World::Tick(...)` pumps input first, then ticks UI, then runs node/component `Tick(...)`.
- Renderer-facing code can pull UI packets through `UISystem::BuildRenderPackets(...)`.

## 1) Initialize UI Through GameRuntime

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

int main()
{
    GameRuntime Runtime{};
    GameRuntimeSettings Settings{};
    Settings.WorldName = "UiWorld";

    GameRuntimeUiSettings Ui{};
    Ui.ViewportWidth = 1920.0f;
    Ui.ViewportHeight = 1080.0f;
    Ui.DpiScaleOverride = 1.0f;
    Settings.UI = Ui;

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

- `Settings.UI = std::nullopt` keeps UI disabled.
- `ViewportWidth` / `ViewportHeight` must be finite and greater than zero.
- `DpiScaleOverride = std::nullopt` keeps default/environment DPI behavior from `UIContext::CreateDefault()`.

## 2) Build UI Content Through `UIContext`

```cpp
auto* UiContext = Runtime.World().UI().Context();
if (!UiContext)
{
    return;
}

auto Root = UiContext->Root();
auto Hud = Root.Add(SnAPI::UI::UIPanel("HudRoot"));
Hud.Element().Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
Hud.Element().Padding().Set(8.0f);
Hud.Element().Gap().Set(6.0f);

auto StatusText = Hud.Add(SnAPI::UI::UIText("Connected"));
StatusText.Element().ColorVal(SnAPI::UI::Color{255, 255, 255, 255});
```

`UISystem` exposes the raw `UIContext` so UI trees, styles, themes, and resources can be managed directly using SnAPI.UI APIs.

## 3) Push Input Into UI

`UISystem` does not pull platform events itself.
Your platform/window layer forwards events:

```cpp
SnAPI::UI::PointerEvent Pointer{};
Pointer.Position = SnAPI::UI::UIPoint{MouseX, MouseY};
Pointer.LeftDown = IsLeftDown;
Runtime.World().UI().PushInput(Pointer);

SnAPI::UI::KeyEvent Key{};
Key.KeyCode = KeyCode;
Key.Down = IsDown;
Runtime.World().UI().PushInput(Key);

SnAPI::UI::TextInputEvent Text{};
Text.Codepoint = Codepoint;
Runtime.World().UI().PushInput(Text);
```

Those events are consumed during `World::Tick(...)` via `UISystem::Tick(...)`.

## 4) Build Frame Render Packets

```cpp
SnAPI::UI::RenderPacketList UiPackets{};
auto BuildResult = Runtime.World().UI().BuildRenderPackets(UiPackets);
if (BuildResult)
{
    for (const SnAPI::UI::RenderPacket& Packet : UiPackets.Packets())
    {
        // Submit Packet to your renderer bridge.
    }
}
```

This keeps the UI module backend-agnostic: SnAPI.UI produces packetized draw intent, and your renderer layer decides GPU submission details.
