# AssetPipeline Integration

GameFramework integrates with SnAPI.AssetPipeline at the payload and runtime-factory level.

The important update since older docs is the payload surface:

- node payloads
- level payloads
- world payloads

not old graph payloads.

## 1. Build Runtime Data

```cpp
RegisterBuiltinTypes();

World WorldInstance("GameWorld");
auto LevelHandle = WorldInstance.CreateLevel("MainLevel");
if (!LevelHandle)
{
    return;
}

auto* LevelRef = NodeCast<Level>(LevelHandle->Borrowed());
if (!LevelRef)
{
    return;
}

(void)LevelRef->CreateNode<BaseNode>("Player");
(void)LevelRef->CreateNode<BaseNode>("Camera");
```

## 2. Serialize To Payload Bytes

```cpp
auto WorldPayload = WorldSerializer::Serialize(WorldInstance);
if (!WorldPayload)
{
    return;
}

std::vector<uint8_t> WorldBytes;
if (!SerializeWorldPayload(*WorldPayload, WorldBytes))
{
    return;
}
```

You can do the same with:

- `NodeSerializer` + `SerializeNodePayload`
- `LevelSerializer` + `SerializeLevelPayload`

## 3. Register Payload Types And Factories

```cpp
::SnAPI::AssetPipeline::AssetManager Manager;
RegisterAssetPipelinePayloads(Manager.GetRegistry());
Manager.GetRegistry().Freeze();
RegisterAssetPipelineFactories(Manager);
```

Why this matters:

- payload registration teaches the asset system how to decode the bytes
- factory registration teaches it how to materialize runtime objects from those payloads

## 4. Write A `.snpak`

```cpp
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
(void)Writer.Write("DemoContent.snpak");
```

## 5. Mount And Load

```cpp
if (!Manager.MountPack("DemoContent.snpak"))
{
    return;
}

auto LoadedWorld = Manager.Load<World>("demo.world");
if (!LoadedWorld)
{
    return;
}
```

The loaded world is a detached runtime object you can inspect or integrate like any other world object, depending on your asset usage pattern.

## 6. `TAssetRef<T>` Is The High-Level Link Type

Use `TAssetRef<T>` when you want a reflected reference to a node, level, world, mesh-backed type, or other asset-managed object.

Key facts:

- it can resolve by asset id or asset name
- it can use a default asset manager
- it supports load-style flows without forcing you to hand-roll registry lookups every time

## 7. Practical Guidance

### Use node payloads for prefab-like subtree content

Good for:

- a single spawnable prop cluster
- a pawn archetype subtree
- a compact authored gameplay object

### Use level payloads for authored chunks

Good for:

- a playable arena
- a streamed dungeon room
- a mission slice

### Use world payloads for complete runtime snapshots or packs

Good for:

- full sample worlds
- benchmark scenes
- end-to-end integration tests

## What To Read Next

- [Networking, Replication, and Reflected RPC](networking.md)
- [Shipyard Save/Load](shipyard_save_load.md)
