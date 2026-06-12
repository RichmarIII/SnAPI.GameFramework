# First Play Session

This tutorial builds a tiny playable session using the current framework flow.

It is intentionally modest:

- one `GameRuntime`
- one `World`
- one root level
- one pawn node
- input, physics, and renderer enabled

The goal is not to build a full game. The goal is to connect the real subsystems in the right order.

## What You Will Learn

- how to bootstrap a runtime with the correct frame loop
- how to create content after runtime startup
- how `PawnBase`, input, physics, and camera pieces fit together
- why the world owns everything

## 1. Start A Runtime

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "FirstPlaySession";
Settings.Tick.EnableFixedTick = true;
Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
```

Enable the subsystems you want.

```cpp
GameRuntimePhysicsSettings Physics{};
Physics.TickInFixedTick = true;
Physics.TickInVariableTick = false;
Settings.Physics = Physics;

GameRuntimeInputSettings Input{};
Input.CreateDesc.EnableKeyboard = true;
Input.CreateDesc.EnableMouse = true;
Input.CreateDesc.EnableGamepad = true;
Settings.Input = Input;

GameRuntimeRendererSettings Renderer{};
Renderer.CreateRendererRuntime = true;
Renderer.CreateWindow = true;
Renderer.WindowTitle = "First Play Session";
Renderer.WindowWidth = 1280.0f;
Renderer.WindowHeight = 720.0f;
Renderer.CreateDefaultLighting = true;
Renderer.ApplyDefaultFeatureProfile = true;
Renderer.CreateDefaultMaterials = true;
Settings.Renderer = Renderer;
```

Then initialize:

```cpp
if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

## 2. Build A Root Level And Ground

```cpp
auto LevelHandle = Runtime.World().CreateLevel("MainLevel");
if (!LevelHandle)
{
    return;
}

auto* MainLevel = NodeCast<Level>(LevelHandle->Borrowed());
if (!MainLevel)
{
    return;
}
```

Create a floor node.

```cpp
auto GroundHandle = MainLevel->CreateNode<BaseNode>("Ground");
if (!GroundHandle)
{
    return;
}

auto* Ground = GroundHandle->Borrowed();
if (!Ground)
{
    return;
}

if (auto Transform = Ground->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, -0.5f, 0.0f);
    Transform->Scale = Vec3(20.0f, 1.0f, 20.0f);
}

if (auto Collider = Ground->Add<ColliderComponent>())
{
    auto& C = Collider->EditSettings();
    C.Shape = SnAPI::Physics::EShapeType::Box;
    C.HalfExtent = Vec3(10.0f, 0.5f, 10.0f);
}

if (auto Body = Ground->Add<RigidBodyComponent>())
{
    Body->EditSettings().BodyType = SnAPI::Physics::EBodyType::Static;
}

if (auto Mesh = Ground->Add<StaticMeshComponent>())
{
    Mesh->EditSettings().MeshPath = "primitive://box";
}
```

## 3. Spawn A Pawn

For a newcomer build, `PawnBase` is useful because it knows how to ensure a baseline component set.

```cpp
auto PawnHandle = MainLevel->CreateNode<PawnBase>("PlayerPawn");
if (!PawnHandle)
{
    return;
}

auto* Pawn = PawnHandle->Borrowed();
if (!Pawn)
{
    return;
}

if (auto Transform = Pawn->Component<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 2.0f, 0.0f);
}
```

Depending on your build configuration, `PawnBase::OnCreate()` will ensure the usual movement/input/render-related defaults when those systems are available.

## 4. Make Input Explicit Anyway

For learning purposes, it is still useful to wire the movement path consciously.

```cpp
(void)Pawn->Add<InputIntentComponent>();
(void)Pawn->Add<InputComponent>();
(void)Pawn->Add<CharacterMovementController>();
```

That gives you the canonical flow:

- input snapshot
- `InputComponent`
- `InputIntentComponent`
- `CharacterMovementController`
- sibling `RigidBodyComponent`

## 5. Add A Camera If Your Pawn Does Not Already Supply One

```cpp
auto CameraHandle = MainLevel->CreateNode<BaseNode>("FollowCamera");
if (!CameraHandle)
{
    return;
}

auto* Camera = CameraHandle->Borrowed();
if (!Camera)
{
    return;
}

if (auto Transform = Camera->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 3.0f, 8.0f);
}

if (auto CameraComponentResult = Camera->Add<CameraComponent>())
{
    CameraComponentResult->EditSettings().Active = true;
}
```

Later you can make this a proper follow camera using a spring arm or possession-driven camera activation.

## 6. Run The Session

```cpp
while (Runtime.Update(1.0f / 60.0f))
{
}
```

That loop is intentionally boring, which is exactly what you want.

All the interesting work happens inside the runtime and world frame phases.

## 7. What To Observe

When this is working, you should understand these truths:

- runtime owns frame order
- world owns content
- physics owns the scene
- renderer presents from end-frame
- input is world-scoped, not node-scoped

## Good Extensions

1. Add a `PlayerStart` node and move spawn logic there.
2. Replace the free camera with possession-aware camera activation.
3. Add `WorldRenderSettings` and a post-process asset set.
4. Serialize the resulting level and reload it in a second run.

Continue with [Space Station Partitions](space_station_partitions.md) or [Bouncy Basement](bouncy_basement.md).
