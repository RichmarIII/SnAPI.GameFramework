# Renderer Integration and Mesh Components

This guide explains how SnAPI.GameFramework integrates with SnAPI.Renderer in the current post-refactor architecture.

By the end, you should understand:

1. when renderer integration is compiled in
2. how to bootstrap renderer through `GameRuntimeSettings`
3. how `World` frame lifecycle drives renderer submit/present
4. how camera/static/skeletal components work together
5. why mesh assets are now separated from per-instance render state (`MeshRenderObject`)

## 1. Build-Time Integration Rules

Renderer support is compile-time gated by `SNAPI_GF_ENABLE_RENDERER`.

In this repository, that define is added when CMake can locate a SnAPI.Renderer source tree and create `SnAPI.Renderer` target.

CMake resolution order:

1. `-DSNAPI_GF_RENDERER_SOURCE_DIR=/path/to/SnAPI.Renderer`
2. fallback local path `/mnt/Apps/Dev/Repositories/SnAPI.Renderer`
3. if neither is present, renderer integration is skipped and related components are not compiled

Typical dev configure command:

```bash
cmake -S . -B build/debug \
  -DSNAPI_GF_BUILD_EXAMPLES=ON \
  -DSNAPI_GF_BUILD_TESTS=ON \
  -DSNAPI_GF_RENDERER_SOURCE_DIR=/mnt/Apps/Dev/Repositories/SnAPI.Renderer
```

## 2. Bootstrap Renderer from `GameRuntimeSettings`

`GameRuntime` is the simplest way to start renderer-backed worlds.

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "RendererWorld";
Settings.RegisterBuiltins = true;

Settings.Tick.EnableFixedTick = true;
Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;

GameRuntimeRendererSettings Renderer{};
Renderer.WindowTitle = "SnAPI.GameFramework Renderer";
Renderer.WindowWidth = 1600.0f;
Renderer.WindowHeight = 900.0f;
Renderer.CreateWindow = true;
Renderer.CreateDefaultLighting = true;
Renderer.RegisterDefaultPassGraph = true;
Renderer.CreateDefaultMaterials = true;
Renderer.EnableSsao = true;
Renderer.EnableSsr = true;
Renderer.EnableBloom = true;
Renderer.EnableAtmosphere = true;

Settings.Renderer = Renderer;

const auto InitResult = Runtime.Init(Settings);
if (!InitResult)
{
    // handle init error
}
```

Important behavior:

- if `Settings.Renderer` is `std::nullopt`, the world runs without renderer backend
- if set, `GameRuntime::Init(...)` initializes world renderer subsystem
- `GameRuntime::Shutdown()` cleanly shuts renderer down

## 3. Runtime Lifecycle: Who Calls Present?

`World::EndFrame()` triggers renderer work when renderer is enabled:

- `NodeGraph::EndFrame()` runs first
- then `RendererSystem::EndFrame()` runs

`RendererSystem::EndFrame()` is responsible for:

- optional swapchain resize handling
- `BeginFrame(...)` / pass execution / `EndFrame(...)` on graphics API
- camera previous-frame save (`SaveFrameState()`)
- render-object previous-frame save (`SaveFrameState()`)

This is why temporal effects and motion vectors stay frame-consistent without manual per-object bookkeeping in gameplay code.

## 4. Camera Setup with `CameraComponent`

`CameraComponent` owns a renderer `CameraBase` and can auto-sync from `TransformComponent`.

```cpp
auto CameraNodeResult = Runtime.World().CreateNode<BaseNode>("MainCamera");
auto* CameraNode = CameraNodeResult ? CameraNodeResult->Borrowed() : nullptr;
if (!CameraNode)
{
    return;
}

auto CameraTransform = CameraNode->Add<TransformComponent>();
CameraTransform->Position = Vec3{0.0f, 2.0f, 8.0f};

auto Camera = CameraNode->Add<CameraComponent>();
Camera->EditSettings().FovDegrees = 60.0f;
Camera->EditSettings().NearClip = 0.05f;
Camera->EditSettings().FarClip = 5000.0f;
Camera->EditSettings().Aspect = 16.0f / 9.0f;
Camera->EditSettings().SyncFromTransform = true;
Camera->SetActive(true);
```

`CameraComponent` resolves world renderer through `Owner()->World()->Renderer()` and updates active camera ownership automatically.

## 5. Static Mesh Rendering (`StaticMeshComponent`)

```cpp
auto CubeNodeResult = Runtime.World().CreateNode<BaseNode>("Cube");
auto* CubeNode = CubeNodeResult ? CubeNodeResult->Borrowed() : nullptr;
if (!CubeNode)
{
    return;
}

auto CubeTransform = CubeNode->Add<TransformComponent>();
CubeTransform->Position = Vec3{0.0f, 0.5f, 0.0f};
CubeTransform->Scale = Vec3{1.0f, 1.0f, 1.0f};

auto CubeMesh = CubeNode->Add<StaticMeshComponent>();
auto& MeshSettings = CubeMesh->EditSettings();
MeshSettings.MeshPath = "assets/cube.obj";
MeshSettings.Visible = true;
MeshSettings.CastShadows = true;
MeshSettings.SyncFromTransform = true;
MeshSettings.RegisterWithRenderer = true;

// Optional if you edited path at runtime and want immediate reload:
CubeMesh->ReloadMesh();
```

What happens internally:

- mesh data is loaded once through `MeshManager` cache
- component creates one per-instance `MeshRenderObject`
- default GBuffer/Shadow material instances are populated through `RendererSystem`
- pass visibility (`Visible`) and shadow participation (`CastShadows`) are applied per render object
- registration is done through `RendererSystem::RegisterRenderObject(...)`

## 6. Skeletal/Rigid Animation Rendering (`SkeletalMeshComponent`)

```cpp
auto ActorNodeResult = Runtime.World().CreateNode<BaseNode>("AnimatedActor");
auto* ActorNode = ActorNodeResult ? ActorNodeResult->Borrowed() : nullptr;
if (!ActorNode)
{
    return;
}

auto ActorTransform = ActorNode->Add<TransformComponent>();
ActorTransform->Position = Vec3{2.0f, 0.0f, 0.0f};

auto Skeletal = ActorNode->Add<SkeletalMeshComponent>();
auto& SkeletalSettings = Skeletal->EditSettings();
SkeletalSettings.MeshPath = "assets/robot.glb";
SkeletalSettings.Visible = true;
SkeletalSettings.CastShadows = true;
SkeletalSettings.AutoPlayAnimations = true;
SkeletalSettings.LoopAnimations = true;
SkeletalSettings.AnimationName = "Idle"; // empty string = play all

// Runtime control:
Skeletal->PlayAnimation("Run", true, 0.0f);
// Skeletal->StopAnimations();
```

`SkeletalMeshComponent` advances rigid animation state on its `MeshRenderObject` each tick, then renderer consumes updated transforms in pass execution.

## 7. Post-Refactor Model: Mesh Asset vs Render Object

This is the key architecture update:

- `Mesh` is now asset data holder (vertices, indices, submeshes, materials, rigid parts, animation tracks)
- runtime instance state lives on `IRenderObject` implementations (currently `MeshRenderObject`)

Per-instance state now includes:

- world transform / previous-frame world transform
- per-submesh material instances and shadow material instances
- per-pass visibility flags
- shadow-cast and triangle-culling toggles
- animation playback state

Why this matters:

- many game objects can share one mesh asset + one vertex stream set
- you avoid accidental per-instance mesh duplication
- material/pass toggles become true instance-level controls

## 8. Material Sharing for High Instance Counts

For large crowds/prop fields, `StaticMeshComponent` supports overriding all submesh materials with shared instances:

```cpp
auto* Renderer = Runtime.World().Renderer().Graphics();
(void)Renderer; // acquire/create your own material instances as needed

// After you created shared material instances:
CubeMesh->SetSharedMaterialInstances(SharedGBufferMaterialInstance,
                                     SharedShadowMaterialInstance);
```

This is useful when you want many objects to intentionally share descriptor state and reduce VRAM churn.

## 9. Manual Renderer Access from Gameplay Code

You can interact with renderer subsystem directly:

```cpp
auto& RendererSystem = Runtime.World().Renderer();

if (RendererSystem.IsInitialized())
{
    RendererSystem.QueueText("Hello Renderer", 20.0f, 20.0f);

    if (!RendererSystem.HasOpenWindow())
    {
        // headless or window closed
    }
}
```

Useful APIs:

- `SetActiveCamera(...)`
- `QueueText(...)`
- `LoadDefaultFont(...)`
- `RecreateSwapChain()`
- `DefaultGBufferMaterial()` / `DefaultShadowMaterial()`

## 10. Multiplayer Example Rendering + Profiling Mode

`examples/MultiplayerExample` requires renderer integration and uses renderer-backed scene setup.

Profiler mode defaults to raw replay capture in this example. To switch to live stream mode:

```bash
SNAPI_MULTIPLAYER_PROFILER_MODE=stream ./build/debug/examples/MultiplayerExample/MultiplayerExample --local
```

## 11. Common Mistakes

- Renderer features not available at compile-time:
  - forgot to provide `SNAPI_GF_RENDERER_SOURCE_DIR` and local fallback path is absent
- Mesh never appears:
  - `MeshPath` invalid
  - `RegisterWithRenderer=false`
  - no active camera
- Object visible but no shadows:
  - `CastShadows=false`
  - shadow pass disabled in renderer bootstrap
- Flickering temporal motion vectors:
  - custom frame loop skips `World::EndFrame()`

## 12. What to Read Next

- [Physics System and Components](physics.md)
- [Networking Replication and RPC](networking.md)
- [Architecture](../architecture.md)
