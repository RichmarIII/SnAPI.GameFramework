# SnAPI.GameFramework

SnAPI.GameFramework is a high‑performance, single‑threaded game framework core built around **nodes**, **components**, and a **first‑class reflection system**. It is designed to handle tens of thousands of nodes with minimal overhead, while still offering a developer‑friendly, type‑safe API and automatic serialization through reflection.

The framework uses:
- **Node graphs** (nested graphs are first‑class nodes).
- **Component‑based composition** (Unity‑style behaviors).
- **Stable UUID handles** for cross‑graph/cross‑asset references.
- **Reflection‑driven serialization** (C++23, template‑centric).
- **World-owned subsystem adapters** (input, ui, networking, audio, physics, renderer).
- **End‑of‑frame deletion** to avoid dangling handles during a frame.
- **Relevance system** to cull work for inactive/out‑of‑budget nodes.

This repository targets C++23 and is intended to be multi‑platform.

---

## Table of Contents

- [Core Concepts](#core-concepts)
- [Design Philosophy](#design-philosophy)
- [Build & Dependencies](#build--dependencies)
- [Project Layout](#project-layout)
- [Quick Start](#quick-start)
- [Nodes and NodeGraphs](#nodes-and-nodegraphs)
- [Components](#components)
- [Renderer Integration (optional module)](#renderer-integration-optional-module)
- [Handles (UUID‑based)](#handles-uuid-based)
- [Reflection System](#reflection-system)
- [Serialization](#serialization)
- [AssetPipeline Integration](#assetpipeline-integration)
- [Scripting Interface](#scripting-interface)
- [Relevance System](#relevance-system)
- [Tests](#tests)
- [Example Application](#example-application)

---

## Core Concepts

**World → Level → NodeGraph → Node → Component**

- **World** is a root `NodeGraph` and a tick source.
- **Level** is a `NodeGraph` that lives inside a World (as a node).
- **NodeGraph** is itself a node; it can contain child nodes and other graphs.
- **Node** is an `INode` with a handle, name, parent/child links, and a component set.
- **Component** is an `IComponent` attached to a node to provide behavior or data.

**Reflection** is first‑class:
- Every reflectable type has a stable UUID TypeId (derived from its name).
- Reflection registers fields, methods, constructors, and base types.
- Serialization uses reflection to traverse fields and rebuild objects.

**Handles** are UUID‑only:
- Any reference to another object is a handle (not a raw pointer).
- Handles resolve via a **global registry** and remain valid across assets.

---

## Design Philosophy

- **Performance first**:
  - No per‑frame heap churn.
  - End‑of‑frame destruction to keep handles valid within a frame.
  - Optional relevance budget to avoid ticking irrelevant nodes.
  - Single‑threaded tick loop (with internal job hooks where safe).

- **Developer experience**:
  - Templated, type‑safe API with clear compile‑time errors.
  - Reflection is opt‑in and straightforward to register.
  - Serialization is automatic once reflection is registered.

- **Safety over raw pointers**:
  - No raw pointers are stored; handles resolve on demand.
  - Borrowed pointers are **transient** and must not be cached.

---

## Build & Dependencies

### Requirements
- CMake 3.24+
- C++23 compiler

### Dependencies (FetchContent)
- [stduuid](https://github.com/mariusbancila/stduuid) (UUIDs)
- [SnAPI.AssetPipeline](https://github.com/RichmarIII/SnAPI.AssetPipeline) (required)
- [SnAPI.Input](https://github.com/RichmarIII/SnAPI.Input) (required)
- [SnAPI.UI](https://github.com/RichmarIII/SnAPI.UI) (required)
- [SnAPI.Networking](https://github.com/RichmarIII/SnAPI.Networking) (required)
- [SnAPI.Audio](https://github.com/RichmarIII/SnAPI.Audio) (required)
- [SnAPI.Physics](https://github.com/RichmarIII/SnAPI.Physics) (required)
- [SnAPI.Renderer](https://github.com/RichmarIII/SnAPI.Renderer) (auto-enabled when source is available)
- Lua (optional, for scripting support)
- Catch2 (tests)

### Build
```bash
cmake -S . -B build
cmake --build build
```

### Options
```bash
-DSNAPI_GF_BUILD_TESTS=ON/OFF
-DSNAPI_GF_BUILD_EXAMPLES=ON/OFF
-DSNAPI_GF_ENABLE_LUA=ON/OFF
-DSNAPI_GF_ENABLE_SWIG=ON/OFF
-DSNAPI_GF_ENABLE_PROFILER=ON/OFF
-DSNAPI_GF_INPUT_SOURCE_DIR=/absolute/path/to/SnAPI.Input
-DSNAPI_GF_UI_SOURCE_DIR=/absolute/path/to/SnAPI.UI
-DSNAPI_GF_RENDERER_SOURCE_DIR=/absolute/path/to/SnAPI.Renderer
```

Renderer notes:

- Renderer integration is compiled in only when `SnAPI.Renderer` source is found and target `SnAPI.Renderer` is created.
- Resolution order is:
  - `SNAPI_GF_RENDERER_SOURCE_DIR`
  - local fallback `/mnt/Dev/CodeProjects/SnAPI.Renderer`
  - otherwise renderer module is skipped with a CMake warning.

### Realtime Profiler Stream (GameRuntime startup helper)

When profiler integration is enabled, `GameRuntime::Init(...)` auto-configures profiler UDP streaming.

- Default behavior: enabled in debug builds, disabled in release builds.
- Default endpoint: `127.0.0.1:17777` (compatible with `SnAPI.Profiler/tools/web`).

Environment overrides:

```bash
SNAPI_GF_PROFILER_STREAM_ENABLE=1|0
SNAPI_GF_PROFILER_STREAM_HOST=127.0.0.1
SNAPI_GF_PROFILER_STREAM_PORT=17777
SNAPI_GF_PROFILER_STREAM_MAX_SCOPES=256
SNAPI_GF_PROFILER_STREAM_MAX_RELATIONSHIPS=256
SNAPI_GF_PROFILER_STREAM_MAX_CATEGORIES=64
SNAPI_GF_PROFILER_STREAM_MAX_THREADS=32
SNAPI_GF_PROFILER_STREAM_SEND_FULL=1|0
SNAPI_GF_PROFILER_STREAM_MAX_UDP_PAYLOAD_BYTES=61440
SNAPI_GF_PROFILER_STREAM_ENABLE_CHUNKING=1|0
SNAPI_GF_PROFILER_STREAM_CHUNK_PAYLOAD_BYTES=14336

SNAPI_GF_PROFILER_EVENT_BUFFER_CAPACITY=16384
SNAPI_GF_PROFILER_MAX_SCOPE_DEPTH=256
SNAPI_GF_PROFILER_HISTORY_CAPACITY=240
SNAPI_GF_PROFILER_MAX_EVENTS_PER_THREAD_PER_FRAME=0
SNAPI_GF_PROFILER_PRESERVE_OVERFLOW_EVENTS=1|0

SNAPI_GF_PROFILER_TRACE_MODE=record|off
SNAPI_GF_PROFILER_TRACE_PATH=./snapi-profiler-replay.trace
SNAPI_GF_PROFILER_TRACE_CAPTURE_ONLY=1|0
```

Notes:

- `SNAPI_GF_PROFILER_TRACE_MODE=record` enables raw trace capture (`SNAPTRC1`).
- `SNAPI_GF_PROFILER_TRACE_CAPTURE_ONLY=1` disables in-process frame aggregation and realtime UDP stream to minimize runtime overhead while recording raw events.
- `examples/MultiplayerExample` defaults to raw replay capture; set `SNAPI_MULTIPLAYER_PROFILER_MODE=stream` to prefer live UDP streaming.
- `SNAPI_GF_PROFILER_PRESERVE_OVERFLOW_EVENTS=1` preserves ring overflow events for fidelity but can be very expensive under heavy scope volume (extra lock/vector work on overflow). Keep it `0` unless you specifically need overflow recovery.

### Local AssetPipeline (recommended for dev)
```bash
cmake -S . -B build \
  -DSNAPI_GF_ASSETPipeline_SOURCE_DIR=/mnt/Dev/CodeProjects/SnAPI.AssetPipeline
```

---

## Project Layout

- `include/` public headers (flat include tree)
- `src/` library implementation
- `tests/` unit tests (Catch2)
- `examples/` example apps (`FeatureShowcase`, `WorldPerfBenchmark`, `MultiplayerExample`)

---

## Quick Start

```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

int main()
{
    RegisterBuiltinTypes();

    World WorldInstance("GameWorld");
    auto LevelHandle = WorldInstance.CreateLevel("MainLevel").value();
    auto& LevelRef = *WorldInstance.LevelRef(LevelHandle);

    auto GraphHandle = LevelRef.CreateGraph("Gameplay").value();
    auto& GraphRef = *LevelRef.Graph(GraphHandle);

    auto PlayerHandle = GraphRef.CreateNode("Player").value();
    auto* Player = PlayerHandle.Borrowed();

    auto Transform = Player->Add<TransformComponent>();
    Transform->Position = Vec3(0.0f, 0.0f, 0.0f);

    WorldInstance.Tick(0.016f);
    WorldInstance.EndFrame();
}
```

---

## Nodes and NodeGraphs

### BaseNode / INode
Nodes provide:
- Name / Handle / Parent / Children
- Active flag
- Component list + type mask
- Tick hooks: `Tick`, `FixedTick`, `LateTick`

### NodeGraph
`NodeGraph` is a node that can own other nodes:

```cpp
NodeGraph Graph("Gameplay");
auto Handle = Graph.CreateNode("Enemy");
Graph.AttachChild(Parent, Handle);
```

### Creating nodes (compile-time and runtime)
```cpp
// Compile-time (fast path, fully typed)
auto EnemyHandle = Graph.CreateNode<EnemyNode>("Enemy").value();

// Runtime (reflection path)
const TypeId EnemyType = TypeIdFromName(EnemyNode::kTypeName);
auto AnyHandle = Graph.CreateNode(EnemyType, "Enemy").value();
```

Runtime creation requires that a default constructor was registered for the type.

### Update order
`NodeGraph::Tick` evaluates relevance, then ticks root nodes. Each node:
1. Ticks itself
2. Ticks its components
3. Ticks its direct children (depth-first)

This keeps update order simple and predictable, and avoids a central "graph
updates everything" pass.

### Custom node types
```cpp
class RotatorNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RotatorNode";
    float m_speed = 90.0f;

    void Tick(float DeltaSeconds) override
    {
        (void)DeltaSeconds;
        // Custom tick logic
    }
};

SNAPI_REFLECT_TYPE(RotatorNode, (TTypeBuilder<RotatorNode>(RotatorNode::kTypeName)
    .Base<BaseNode>()
    .Field("Speed", &RotatorNode::m_speed)
    .Constructor<>()
    .Register()));
```

Implementing `INode` directly is supported, but you lose the default behavior
and storage that `BaseNode` provides.

### World and Level
Both inherit from `NodeGraph`:

```cpp
World WorldInstance;
auto LevelHandle = WorldInstance.CreateLevel("LevelA");
auto& LevelRef = *WorldInstance.LevelRef(LevelHandle);
```

World and Level are graphs, so they can be saved, loaded, nested, and treated
like any other node graph.

---

## Components

Components derive from `IComponent`:

```cpp
class HealthComponent : public IComponent
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::HealthComponent";
    int m_health = 100;
};
```

Add and query components via nodes:

```cpp
auto Health = Node->Add<HealthComponent>();
auto Existing = Node->Component<HealthComponent>();
```

Component lifetime:
- Added instantly.
- Removed via `Remove<T>()`.
- Actually destroyed at **end of frame**.

### Custom components + reflection
```cpp
class DamageComponent : public IComponent
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DamageComponent";

    int m_amount = 10;
    float m_radius = 1.0f;
};

SNAPI_REFLECT_COMPONENT(DamageComponent, (TTypeBuilder<DamageComponent>(DamageComponent::kTypeName)
    .Field("Amount", &DamageComponent::m_amount)
    .Field("Radius", &DamageComponent::m_radius)
    .Constructor<>()
    .Register()));
```

### Custom serialization (optional)
If a component needs special serialization, register a custom serializer:
```cpp
ComponentSerializationRegistry::Instance().RegisterCustom<DamageComponent>(
    [](const void* Instance, cereal::BinaryOutputArchive& Ar, const TSerializationContext&) -> TExpected<void> {
        const auto* Comp = static_cast<const DamageComponent*>(Instance);
        Ar(Comp->m_amount, Comp->m_radius);
        return Ok();
    },
    [](void* Instance, cereal::BinaryInputArchive& Ar, const TSerializationContext&) -> TExpected<void> {
        auto* Comp = static_cast<DamageComponent*>(Instance);
        Ar(Comp->m_amount, Comp->m_radius);
        return Ok();
    });
```

---

## Renderer Integration (optional module)

When renderer integration is available (`SNAPI_GF_ENABLE_RENDERER`), `World` exposes a full renderer subsystem:

- `World::Renderer()` returns world-owned `RendererSystem`.
- `GameRuntimeSettings::Renderer` controls bootstrap.
- `World::EndFrame()` submits/presents renderer frame work.

### Bootstrap through `GameRuntime`

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "RenderWorld";
Settings.RegisterBuiltins = true;
Settings.Tick.EnableFixedTick = true;
Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;

GameRuntimeRendererSettings Renderer{};
Renderer.WindowTitle = "SnAPI.GameFramework";
Renderer.CreateWindow = true;
Renderer.CreateDefaultLighting = true;
Renderer.RegisterDefaultPassGraph = true;
Renderer.CreateDefaultMaterials = true;
Renderer.EnableSsao = true;
Renderer.EnableSsr = true;
Renderer.EnableBloom = true;
Renderer.EnableAtmosphere = true;

Settings.Renderer = Renderer;

auto Init = Runtime.Init(Settings);
if (!Init)
{
    // handle error
}
```

If `Settings.Renderer` is `std::nullopt`, runtime runs without renderer backend.

### Built-In Renderer Components

Available when renderer integration is compiled in:

- `CameraComponent`
  - Owns a renderer camera.
  - Can sync from `TransformComponent`.
  - Can mark itself as active world camera.
- `StaticMeshComponent`
  - Loads mesh asset data.
  - Creates per-instance render object state.
  - Supports pass/shadow toggles and shared material overrides.
- `SkeletalMeshComponent`
  - Same mesh/render-object model with rigid animation controls.

Minimal setup:

```cpp
auto Actor = WorldInstance.CreateNode<BaseNode>("Visual").value();
auto* Node = Actor.Borrowed();
if (!Node)
{
    return;
}

auto Transform = Node->Add<TransformComponent>();
Transform->Position = Vec3{0.0f, 0.0f, 0.0f};

auto Mesh = Node->Add<StaticMeshComponent>();
Mesh->EditSettings().MeshPath = "assets/cube.obj";
Mesh->EditSettings().Visible = true;
Mesh->EditSettings().CastShadows = true;
```

### Post-Refactor Render Architecture

Render-side instance ownership changed:

- `Mesh` is now an asset data holder (vertices/indices/material slots/animation tracks).
- `MeshRenderObject` (via `IRenderObject`) owns per-instance runtime state:
  - world + previous transforms
  - per-submesh material instances
  - per-pass visibility flags
  - shadow/culling toggles
  - per-instance animation playback state

This allows many components/nodes to share one mesh asset and one vertex stream set while preserving independent per-instance render behavior.

### RendererSystem Utility APIs

`RendererSystem` also supports:

- `SetActiveCamera(...)`
- `QueueText(...)` and `LoadDefaultFont(...)`
- `RecreateSwapChain()`
- `DefaultGBufferMaterial()` / `DefaultShadowMaterial()`

---

## Handles (UUID‑based)

All references are handles that store a UUID:

```cpp
NodeHandle Target;
auto* TargetNode = Target.Borrowed();  // transient pointer
```

Key rules:
- Handles remain valid across serialization and asset loads.
- A handle resolves **only once the target object is loaded and registered**.
- Borrowed pointers **must not be cached** (use handles instead).

There are two core handle types:
- `NodeHandle` for nodes (BaseNode and derived).
- `ComponentHandle` for components (IComponent and derived).

Handles resolve through a global registry. Object creation registers the UUID,
and destruction happens at end-of-frame to avoid dangling references during a
tick.

---

## Reflection System

Reflection is built on type traits + lazy auto-registration:
TypeIds are stable UUIDs derived from the fully qualified type name via
`TypeIdFromName`. Keep type names stable to preserve serialized data.

### 1) Provide a stable type name
```cpp
class MyNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::MyNode";
    int m_value = 0;
};
```

### 2) Register with `TTypeBuilder`
```cpp
SNAPI_REFLECT_TYPE(MyNode, (TTypeBuilder<MyNode>(MyNode::kTypeName)
    .Base<BaseNode>()
    .Field("Value", &MyNode::m_value)
    .Constructor<>()
    .Register()));
```

### 3) Components also register with the serialization registry
```cpp
SNAPI_REFLECT_COMPONENT(HealthComponent, (TTypeBuilder<HealthComponent>(HealthComponent::kTypeName)
    .Field("Health", &HealthComponent::m_health)
    .Constructor<>()
    .Register()));
```

### Notes
- Register types **once per TU** (use `SNAPI_REFLECT_TYPE` macros in a `.cpp`).
- Types are registered on first use (TypeRegistry::Find will auto-ensure a missing type id).
- Default constructors must be registered for types created by TypeId.
- Fields can be const‑qualified (setter will reject writes).
- Call `RegisterBuiltinTypes()` once at startup to register core types and default serializers.
- If you cannot add a `kTypeName` to a type, use `SNAPI_DEFINE_TYPE_NAME(Type, "Fully::Qualified::Name")`.

### Runtime queries and inheritance
```cpp
const auto* Info = TypeRegistry::Instance().FindByName("SnAPI::GameFramework::DamageComponent");
if (Info)
{
    for (const auto& Field : Info->Fields)
    {
        // Field.Name, Field.FieldType
    }
}

const TypeId BaseId = TypeIdFromName(BaseNode::kTypeName);
const auto Derived = TypeRegistry::Instance().Derived(BaseId);
```

`TypeRegistry::IsA` and `Derived` let you traverse inheritance chains. Use this
to discover all node/component subclasses for tools, editors, or scripting.

### Creating by reflection
If a type registered a default constructor, it can be created from its
`TypeInfo`:
```cpp
const TypeId NodeType = TypeIdFromName(MyNode::kTypeName);
auto Handle = Graph.CreateNode(NodeType, "MyNode");
```

This is the path used by serialization and scripting.

### Invoking methods
`MethodInfo::Invoke` uses `Variant` for arguments:
```cpp
auto* Info = TypeRegistry::Instance().Find(TypeIdFromName(MyNode::kTypeName));
if (Info && !Info->Methods.empty())
{
    std::array<Variant, 1> Args = { Variant::FromValue(42) };
    auto Result = Info->Methods[0].Invoke(NodePtr, Args);
}
```

---

## Serialization

Serialization is reflection‑driven:
- Nodes serialize fields via reflection (including base classes).
- Components serialize via reflection unless custom serializer is registered.
- Handles serialize as UUIDs, so cross‑graph references survive load.

Payload serialization is done with cereal binary archives. The helpers:
`SerializeNodeGraphPayload`, `SerializeLevelPayload`, and `SerializeWorldPayload`
encode the reflection data into byte blobs suitable for AssetPipeline packs.

If a node type has no registered fields, only its identity, name, active state,
parent/child links, components, and nested graphs are serialized.

If a field type is not a built‑in:
1. Register it in the `TypeRegistry`.
2. Ensure its fields are reflectable (or trivially copyable).
3. Use reflection-driven serialization for nested types, or register it as a
   value codec if it is trivially copyable.

Trivially copyable type example:
```cpp
struct TColor
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TColor";
    float m_r = 0.0f;
    float m_g = 0.0f;
    float m_b = 0.0f;
    float m_a = 1.0f;
};

SNAPI_REFLECT_TYPE(TColor, (TTypeBuilder<TColor>(TColor::kTypeName)
    .Field("R", &TColor::m_r)
    .Field("G", &TColor::m_g)
    .Field("B", &TColor::m_b)
    .Field("A", &TColor::m_a)
    .Constructor<>()
    .Register()));

ValueCodecRegistry::Instance().Register<TColor>();
```

### Making a custom type fully serializable
```cpp
struct TDamageInfo
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TDamageInfo";
    int m_amount = 0;
    float m_radius = 0.0f;
};

SNAPI_REFLECT_TYPE(TDamageInfo, (TTypeBuilder<TDamageInfo>(TDamageInfo::kTypeName)
    .Field("Amount", &TDamageInfo::m_amount)
    .Field("Radius", &TDamageInfo::m_radius)
    .Constructor<>()
    .Register()));

class DamageComponent : public IComponent
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DamageComponent";
    TDamageInfo m_info{};
};

SNAPI_REFLECT_COMPONENT(DamageComponent, (TTypeBuilder<DamageComponent>(DamageComponent::kTypeName)
    .Field("Info", &DamageComponent::m_info)
    .Constructor<>()
    .Register()));
```

### Handle fixups
Handles serialize as UUIDs. When objects are loaded, they register their UUIDs
with the global registry. Any handle that points to a loaded UUID becomes valid
automatically, even if the reference crosses graphs or assets.

### Built‑ins already supported
`bool`, `int`, `uint`, `uint64`, `float`, `double`, `std::string`, `Uuid`, `Vec3`, `NodeHandle`, `ComponentHandle`

---

## AssetPipeline Integration

SnAPI.GameFramework ships with an inline AssetPipeline plugin plus helper functions
to register payload serializers and runtime factories. Serialization uses cereal
binary archives (same pattern as the AssetPipeline Texture plugin).

### Register payloads and factories
```cpp
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;
using namespace SnAPI::AssetPipeline;

AssetManager Manager;
RegisterAssetPipelinePayloads(Manager.GetRegistry());
Manager.GetRegistry().Freeze();
RegisterAssetPipelineFactories(Manager);
```

### Load runtime assets
```cpp
auto MountResult = Manager.MountPack("GameContent.snpak");
if (!MountResult.has_value())
{
    return;
}

auto WorldResult = Manager.Load<World>("main.world");
auto LevelResult = Manager.Load<Level>("main.level");
auto GraphResult = Manager.Load<NodeGraph>("main.graph");
```

### Write packs (cooked payloads)
```cpp
std::vector<uint8_t> WorldBytes;
auto WorldPayload = WorldSerializer::Serialize(WorldInstance);
SerializeWorldPayload(WorldPayload.value(), WorldBytes);

AssetPackWriter Writer;
AssetPackEntry Entry;
Entry.Id = AssetPipelineAssetIdFromName("main.world");
Entry.AssetKind = AssetKindWorld();
Entry.Name = "main.world";
Entry.Cooked = TypedPayload(PayloadWorld(), WorldSerializer::kSchemaVersion, WorldBytes);
Writer.AddAsset(std::move(Entry));
Writer.Write("GameContent.snpak");
```

### Inline plugin
If you use the AssetPipeline plugin system, the GameFramework plugin registers
the payload serializers for NodeGraph, Level, and World. Link this library so
the plugin symbol is available.

### Custom nodes/components
No extra AssetPipeline work is required for custom nodes/components. As long as
the types are registered with reflection and serialization, they are handled by
the existing payload serializers and factories.

---

## Scripting Interface

The framework exposes a small C ABI for reflection-based interaction. This is
intended for SWIG or hand-written bindings so that scripting runtimes can access
the same API that C++ does.

Key pieces:
- `include/ScriptABI.h` exposes reflection and Variant access (fields/methods).
- `IScriptEngine` + `ScriptRuntime` provide the runtime entry points.
- `ScriptComponent` stores script module/type and an instance id.

At a high level:
1. Register types with reflection (C++).
2. Use the ABI to query type info and invoke fields/methods.
3. Scripts hold handles, not raw pointers, just like C++.

The API surface is intentionally narrow; reflection does the heavy lifting.

---

## Relevance System

Relevance is a component-driven culling system. A node is relevant if:
- It is active, and
- It has no RelevanceComponent, or its policy returns true.

Attach a `RelevanceComponent` and provide a policy:
```cpp
struct TDistancePolicy
{
    float m_maxDistance = 50.0f;

    bool Evaluate(const RelevanceContext& Context) const
    {
        (void)Context;
        return true;
    }
};

auto Relevance = Node->Add<RelevanceComponent>();
Relevance->Policy(TDistancePolicy{});
```

Control evaluation cost:
```cpp
Graph.RelevanceBudget(1000); // 0 = unlimited
```

Relevance is evaluated at the start of `NodeGraph::Tick`. Nodes that are not
relevant do not tick or tick their components/children.

---

## Tests

Enable and run tests:
```bash
cmake -S . -B build -DSNAPI_GF_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

Tests cover:
- Reflection registration
- Node graph creation and traversal
- Handle validity and serialization
- World/level graph serialization
- Relevance policy behavior

---

## Example Application

The example app lives in `examples/FeatureShowcase` and demonstrates:
- Node and component creation
- Custom node/component reflection
- Graph nesting and world/level setup
- Serialization to payloads and `.snpak` packs
- Loading assets via AssetManager

`examples/MultiplayerExample` demonstrates:

- networking + replication with renderer-backed world
- camera + mesh component usage in a live scene
- profiler defaults for raw replay capture mode with optional live stream override

Build and run:
```bash
cmake -S . -B build -DSNAPI_GF_BUILD_EXAMPLES=ON
cmake --build build
./build/examples/FeatureShowcase/FeatureShowcase
./build/examples/MultiplayerExample/MultiplayerExample --local
```

## Renderer Refactor Status (2026-02-15)

This documentation now reflects the integrated renderer refactor:

- mesh runtime state moved from mesh assets to render objects (`IRenderObject` / `MeshRenderObject`)
- `StaticMeshComponent` and `SkeletalMeshComponent` now use per-instance render objects
- world renderer lifecycle is documented and routed through `RendererSystem`
