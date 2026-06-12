# Renderer Integration

Renderer support is optional, world-owned, and tightly tied to end-of-frame submission.

The short version is:

- `RendererSystem` is the world-owned renderer adapter
- camera and mesh components bridge gameplay objects into renderer objects
- UI viewport binding is explicit
- some viewport creation is lazy, especially through `UIRenderViewport`

## 1. Bootstrap Through `GameRuntime`

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "RenderWorld";

GameRuntimeRendererSettings Renderer{};
Renderer.CreateRendererRuntime = true;
Renderer.CreateWindow = true;
Renderer.WindowTitle = "SnAPI.GameFramework";
Renderer.WindowWidth = 1280.0f;
Renderer.WindowHeight = 720.0f;
Renderer.CreateDefaultLighting = true;
Renderer.ApplyDefaultFeatureProfile = true;
Renderer.CreateDefaultMaterials = true;
Renderer.CreateDefaultEnvironmentProbe = true;
Settings.Renderer = Renderer;

if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

Key fact: the renderer subsystem initializes during `GameRuntime::Init()`, before the gameplay host starts.

## 2. The World Submits Renderer Work In `EndFrame`

Renderer frame submission does not happen during node constructors or random gameplay callbacks.

It happens from `World::EndFrame()`.

That is also where the world:

- builds bound UI render packets
- queues them into renderer viewports
- calls `RendererSystem::EndFrame()`

If you hand-roll a world loop and skip `EndFrame()`, renderer work will not behave correctly.

## 3. Add A Camera

```cpp
auto CameraHandle = WorldInstance.CreateNode<BaseNode>("MainCamera");
if (!CameraHandle)
{
    return;
}

auto* CameraNode = CameraHandle->Borrowed();
if (!CameraNode)
{
    return;
}

if (auto Transform = CameraNode->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 2.0f, 8.0f);
}

if (auto Camera = CameraNode->Add<CameraComponent>())
{
    auto& Settings = Camera->EditSettings();
    Settings.FovDegrees = 60.0f;
    Settings.NearClip = 0.05f;
    Settings.FarClip = 5000.0f;
    Settings.Aspect = 16.0f / 9.0f;
    Settings.SyncFromTransform = true;
    Settings.Active = true;
}
```

`CameraComponent` owns one renderer camera instance and can derive its pose from the owner's `TransformComponent`.

## 4. Add A Static Mesh

```cpp
auto CubeHandle = WorldInstance.CreateNode<BaseNode>("Cube");
if (!CubeHandle)
{
    return;
}

auto* Cube = CubeHandle->Borrowed();
if (!Cube)
{
    return;
}

if (auto Transform = Cube->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 0.5f, 0.0f);
}

if (auto Mesh = Cube->Add<StaticMeshComponent>())
{
    auto& MeshSettings = Mesh->EditSettings();
    MeshSettings.MeshPath = "primitive://box";
    MeshSettings.Visible = true;
    MeshSettings.CastShadows = true;
    MeshSettings.SyncFromTransform = true;
    MeshSettings.RetainInScene = true;
}
```

Important detail:

- the current `StaticMeshComponent` path reliably supports built-in primitive tokens like `primitive://box`
- non-primitive `MeshPath` values are best treated as compatibility/change keys unless you are intentionally using the asset-driven path

## 5. Add A Skeletal Mesh

`SkeletalMeshComponent` is more asset-driven than `StaticMeshComponent`.

Its settings still expose `MeshPath`, but the current implementation treats asset-driven data as the primary load path and uses `MeshPath` mainly for compatibility and change detection.

Common settings:

- `Visible`
- `CastShadows`
- `SyncFromTransform`
- `RetainInScene`
- `AutoPlayAnimations`
- `LoopAnimations`
- `AnimationName`

## 6. Understand The Current Render Object Model

The old docs tended to blur asset data and per-instance renderer state. The current model is clearer.

- mesh assets are shared data
- per-instance render state lives on renderer objects
- components bridge node state into those renderer objects

Why that split matters:

- many nodes can share one source mesh
- each instance can still have unique transform and pass membership
- visibility and shadow participation are per-instance decisions

## 7. World Render Settings

`WorldRenderSettings` is a node-level convenience container for post-processing and atmospheric settings.

It is world-facing render configuration, not a camera replacement.

Typical responsibilities:

- referencing SSAO/SSR/Bloom/ToneMap parameter nodes
- applying world-scoped render configuration once the renderer is ready

Because viewport creation can be lazy in editor flows, render-setting nodes should not assume all viewports already exist at constructor time.

## 8. `UIRenderViewport` Is Layout-Driven

When you embed a renderer viewport in UI, the viewport does not exist immediately when the UI element is constructed.

It is created and synchronized from `Arrange()` and `Paint()`.

That means these assumptions are wrong:

- "my node constructor can immediately find every viewport"
- "a viewport-backed UI element guarantees a swapchain during creation"
- "editor scene nodes should set viewport-dependent state before layout runs"

The framework now uses deferred bootstrap behavior to avoid exactly those bugs.

## 9. Common Mistakes

### Assuming renderer init implies viewport init

Renderer subsystem initialization and viewport creation are different stages.

### Forgetting `EndFrame()` in a manual loop

That skips renderer submission.

### Treating `WorldRenderSettings` like a camera

They solve different problems.

## What To Read Next

- [Physics System and Components](physics.md)
- [Postcard Renderer](postcard_renderer.md)
- [Tool World vs Game World](tool_world_vs_game_world.md)
