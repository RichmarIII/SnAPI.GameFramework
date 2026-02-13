# AssetPipeline Integration

This page shows the full flow to package a `World`/`Level`/`NodeGraph` into `.snpak` and load it back through `AssetManager`.

The same pattern is used by `examples/FeatureShowcase` and `examples/WorldPerfBenchmark`.

## 1. Build Runtime Data

```cpp
RegisterBuiltinTypes();

World WorldInstance("GameWorld");
auto LevelHandle = WorldInstance.CreateLevel("MainLevel");
auto LevelRef = WorldInstance.LevelRef(LevelHandle.value());

auto GraphHandle = LevelRef->CreateGraph("Gameplay");
auto GraphRef = LevelRef->Graph(GraphHandle.value());

(void)GraphRef->CreateNode("Player");
```

## 2. Serialize to Payload Bytes

```cpp
auto WorldPayloadResult = WorldSerializer::Serialize(WorldInstance);
if (!WorldPayloadResult)
{
    return;
}

std::vector<uint8_t> WorldBytes;
if (!SerializeWorldPayload(WorldPayloadResult.value(), WorldBytes))
{
    return;
}
```

## 3. Write a `.snpak`

```cpp
#include "AssetPackWriter.h"

::SnAPI::AssetPipeline::AssetPackWriter Writer;

::SnAPI::AssetPipeline::AssetPackEntry Entry;
Entry.Id = AssetPipelineAssetIdFromName("demo.world");
Entry.AssetKind = AssetKindWorld();
Entry.Name = "demo.world";
Entry.VariantKey = "";
Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
    PayloadWorld(),
    WorldSerializer::kSchemaVersion,
    WorldBytes);

Writer.AddAsset(std::move(Entry));

auto WriteResult = Writer.Write("DemoContent.snpak");
if (!WriteResult)
{
    return;
}
```

## 4. Register GameFramework Payload + Runtime Factories

```cpp
#include "AssetManager.h"

::SnAPI::AssetPipeline::AssetManager Manager;

RegisterAssetPipelinePayloads(Manager.GetRegistry());
Manager.GetRegistry().Freeze();
RegisterAssetPipelineFactories(Manager);
```

Why this order matters:

1. payload serializers must be in the payload registry
2. optional `Freeze()` locks registry for fast read-only lookups
3. runtime factories enable `Load<T>`/`Get<T>` materialization

## 5. Mount and Load

```cpp
auto MountResult = Manager.MountPack("DemoContent.snpak");
if (!MountResult)
{
    return;
}

auto LoadedWorld = Manager.Load<World>("demo.world");
if (!LoadedWorld)
{
    return;
}
```

The returned world can be traversed normally (`Levels()`, `CreateNode`, `NodePool().ForEach`, etc.).

## 6. Common Failure Cases

- `Type not registered` on load:
    - `RegisterBuiltinTypes()` was not called early enough.
    - The type’s reflection macro was never linked/referenced.
- `Failed to load world from AssetManager`:
    - payload serializers/factories were not registered.
    - payload type/schema mismatch.
- Objects deserialize but links are missing:
    - verify handles/UUID fields were reflected and serialized.

## 7. Compression Notes

Compression behavior is primarily configured in SnAPI.AssetPipeline (writer settings and optional per-entry overrides).
GameFramework payload integration is compatible with those settings because it only provides payload bytes and factories.

Next: [Networking Replication and RPC](networking.md)
