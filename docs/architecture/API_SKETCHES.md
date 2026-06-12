# API Sketches

Read this when:

- planning new public APIs or checking whether an implementation matches the
  intended public shape

Related context:

- `../ARCHITECTURE.md`
- `RUNTIME_CORE.md`
- `ASSET_PIPELINE.md`
- `RENDERING.md`

## Runtime Session

Representative public shape:

```cpp
GameRuntime runtime{};
GameRuntimeSettings settings{};

if (!runtime.Init(settings))
{
    return;
}

World& world = runtime.World();
runtime.Update(deltaSeconds);
runtime.Shutdown();
```

## World Objects

```cpp
NodeHandle node = *world.CreateNode<PawnBase>("Player");
BaseNode* borrowed = node.Borrowed();
```

Handles are durable identity. Borrowed pointers are temporary views.

## Assets

Source assets and runtime payloads should remain distinct:

```cpp
AssetRef<StaticMeshAsset> authoredMesh;
StaticMeshAssetRuntime cookedMesh;
```

Exact names may differ, but public APIs should communicate whether they operate
on source documents, import settings, cooked payloads, or runtime objects.

## Rendering

GameFramework rendering APIs should be GameFramework-owned while consuming
Renderer.New contracts:

```cpp
GameRenderObject object{};
GameRenderMesh mesh{};
GameRenderWindow window{};
```

Do not expose Renderer.New internals where a stable GameFramework concept is the
actual API boundary.
