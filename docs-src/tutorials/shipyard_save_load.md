# Shipyard Save/Load

This tutorial is about persistence with modern serializer APIs.

The shipyard theme is just an excuse to practice the three real persistence scopes:

- one node
- one level
- one world

## What You Will Learn

- how to serialize the right scope for the job
- how to use payload byte helpers
- when to regenerate ids on repeated instantiation

## 1. Build A Small Shipyard

Create:

- a `Level` called `Shipyard`
- a child level called `DryDockA`
- a cargo crane node
- a ship hull node
- a lighting or render-settings node if your build includes renderer support

The exact art is irrelevant. The hierarchy shape is the lesson.

## 2. Save A Single Node As A Prefab-Like Payload

```cpp
auto Payload = NodeSerializer::Serialize(*ShipHullNode);
if (!Payload)
{
    return;
}

std::vector<uint8_t> Bytes;
if (!SerializeNodePayload(*Payload, Bytes))
{
    return;
}
```

This is the right scope for a reusable authored object.

## 3. Save A Whole Level

```cpp
auto LevelPayload = LevelSerializer::Serialize(*DryDockLevel);
if (!LevelPayload)
{
    return;
}

std::vector<uint8_t> LevelBytes;
(void)SerializeLevelPayload(*LevelPayload, LevelBytes);
```

This is the right scope for a content chunk.

## 4. Save The Whole World

```cpp
auto WorldPayload = WorldSerializer::Serialize(WorldInstance);
if (!WorldPayload)
{
    return;
}

std::vector<uint8_t> WorldBytes;
(void)SerializeWorldPayload(*WorldPayload, WorldBytes);
```

This is the right scope for a full snapshot or pack.

## 5. Load Back Into A New World

```cpp
World Loaded("LoadedShipyard");
auto Payload = DeserializeWorldPayload(WorldBytes.data(), WorldBytes.size());
if (!Payload)
{
    return;
}

if (auto LoadResult = WorldSerializer::Deserialize(*Payload, Loaded); !LoadResult)
{
    return;
}
```

Now inspect the loaded hierarchy and verify the objects you care about are present.

## 6. Repeated Instantiation Requires New Ids

If you deserialize the same node or level payload twice into the same world, use `TDeserializeOptions` with `RegenerateObjectIds = true`.

```cpp
TDeserializeOptions CopyOptions{};
CopyOptions.RegenerateObjectIds = true;
```

That is how you avoid id collisions and stale handle reuse.

## 7. What To Verify After Load

Check these first:

- hierarchy shape
- reflected fields
- handle-backed links
- component presence
- expected `OnCreate` side effects after load

The repository already has tests around these flows. Use them.

## 8. Extensions

1. Save `DryDockA` as a reusable level asset.
2. Instantiate it twice with regenerated ids.
3. Mount it through AssetPipeline instead of raw byte helpers.

Continue with [AssetPipeline Integration](assetpipeline.md).
