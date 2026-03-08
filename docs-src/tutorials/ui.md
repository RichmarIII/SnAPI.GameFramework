# UI System

`UISystem` gives each world a tree of `UIContext` objects instead of a single process-global UI singleton.

That design matters for:

- normal game HUDs
- embedded game viewports
- editor panels and overlays
- multiple context-to-viewport bindings

## 1. Bootstrap UI

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "UiWorld";

GameRuntimeUiSettings Ui{};
Ui.ViewportWidth = 1600.0f;
Ui.ViewportHeight = 900.0f;
Ui.DpiScaleOverride = 1.0f;
Settings.UI = Ui;

if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

The root context is created during `UISystem::Initialize(...)`.

## 2. Think In Contexts

`UISystem` owns:

- one root context
- optional child contexts
- explicit viewport bindings

Important APIs:

- `RootContextId()`
- `CreateContext(...)`
- `DestroyContext(...)`
- `Context(...)`
- `BindViewportContext(...)`
- `BuildRenderPackets(...)`
- `BuildBoundViewportRenderPackets(...)`

## 3. Build UI Through SnAPI.UI

```cpp
auto RootContextId = Runtime.World().UI().RootContextId();
auto* RootContext = Runtime.World().UI().Context(RootContextId);
if (!RootContext)
{
    return;
}

auto Root = RootContext->Root();
auto Hud = Root.Add(SnAPI::UI::UIPanel("HudRoot"));
Hud.Element().Padding().Set(12.0f);
Hud.Element().Gap().Set(8.0f);

auto Title = Hud.Add(SnAPI::UI::UIText("SnAPI.GameFramework"));
Title.Element().ColorVal(SnAPI::UI::Color{255, 255, 255, 255});
```

`UISystem` is not a second widget DSL. It is the owner of contexts and routing. Actual widgets still come from `SnAPI.UI`.

## 4. Feed Input To The UI System

`UISystem` does not magically pull platform events by itself unless a host like `GameRuntime` forwards them.

Manual routing looks like this:

```cpp
SnAPI::UI::PointerEvent Pointer{};
Pointer.Position = SnAPI::UI::UIPoint{MouseX, MouseY};
Pointer.LeftDown = IsLeftDown;
Runtime.World().UI().PushInput(Pointer);

SnAPI::UI::KeyEvent Key{};
Key.KeyCode = KeyCode;
Key.Down = IsPressed;
Runtime.World().UI().PushInput(Key);

SnAPI::UI::TextInputEvent Text{};
Text.Codepoint = Codepoint;
Runtime.World().UI().PushInput(Text);
```

If you use `GameRuntime` with both input and UI enabled, it can forward normalized input events into UI automatically.

## 5. Build Render Packets

For one context:

```cpp
SnAPI::UI::RenderPacketList Packets{};
(void)Runtime.World().UI().BuildRenderPackets(RootContextId, Packets);
```

For every bound viewport/context pair:

```cpp
std::vector<UISystem::ViewportPacketBatch> Batches{};
(void)Runtime.World().UI().BuildBoundViewportRenderPackets(Batches);
```

That second path is the one the world uses during `EndFrame()` when UI and renderer are both enabled.

## 6. UI Viewports Are Explicit

A renderer viewport and a UI context are linked explicitly, not by coincidence.

The important integration points are:

- `GameRuntime::BindViewportWithUI(...)`
- `UISystem::BindViewportContext(...)`
- `UIRenderViewport`

`UIRenderViewport` deserves special attention.

### `UIRenderViewport` is lazy

The actual renderer viewport is created lazily from layout and paint through `SyncViewport()`.

That means:

- the UI element can exist before the renderer viewport exists
- code that assumes the viewport exists during object construction is wrong
- editor bootstrap must allow layout to happen before render-dependent `OnCreate` work is flushed

That lazy behavior is real, and a lot of the old docs did not describe it correctly.

## 7. Multiple Contexts Are Useful

Common reasons to create child contexts:

- an in-world screen rendered into a panel
- a modal shell with separate routing
- an editor overlay or viewport-owned tool surface

Example skeleton:

```cpp
UISystem::ContextId ChildId = 0;
if (Runtime.World().UI().CreateContext(Runtime.World().UI().RootContextId(), ChildId))
{
    auto* ChildContext = Runtime.World().UI().Context(ChildId);
    if (ChildContext)
    {
        ChildContext->SetViewportSize(640.0f, 360.0f);
    }
}
```

## What To Read Next

- [Renderer Integration](renderer.md)
- [Tool World vs Game World](tool_world_vs_game_world.md)
