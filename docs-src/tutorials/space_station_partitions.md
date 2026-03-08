# Space Station Partitions

This tutorial is about content organization, not combat or graphics tricks.

You will build a miniature space station using nested `Level` nodes so the station is easy to reason about, stream, serialize, and test.

## Why This Tutorial Exists

Older docs taught people to think in terms of `NodeGraph` partitions.

The modern replacement is simpler:

- create a root level
- create child `Level` nodes for partitions
- attach gameplay nodes under those partitions

## 1. Create The Station Root

```cpp
World WorldInstance("StationWorld");
auto StationHandle = WorldInstance.CreateLevel("OrpheusStation");
if (!StationHandle)
{
    return;
}

auto* Station = NodeCast<Level>(StationHandle->Borrowed());
if (!Station)
{
    return;
}
```

## 2. Create Partitions As Child Levels

```cpp
auto DockHandle = Station->CreateNode<Level>("DockRing");
auto CommandHandle = Station->CreateNode<Level>("CommandDeck");
auto ReactorHandle = Station->CreateNode<Level>("ReactorCore");

auto* DockRing = DockHandle ? NodeCast<Level>(DockHandle->Borrowed()) : nullptr;
auto* CommandDeck = CommandHandle ? NodeCast<Level>(CommandHandle->Borrowed()) : nullptr;
auto* ReactorCore = ReactorHandle ? NodeCast<Level>(ReactorHandle->Borrowed()) : nullptr;
```

Now you have three explicit content regions, but the world is still the owner.

## 3. Put Different Node Types In Different Regions

Examples:

- `DockRing`: player starts, cargo props, vehicle spawners
- `CommandDeck`: interactables, mission logic nodes, UI triggers
- `ReactorCore`: hazards, audio emitters, scripted events

```cpp
if (DockRing)
{
    (void)DockRing->CreateNode<PlayerStart>("DockSpawn");
    (void)DockRing->CreateNode<BaseNode>("CargoStack");
}

if (CommandDeck)
{
    (void)CommandDeck->CreateNode<BaseNode>("CaptainTerminal");
}

if (ReactorCore)
{
    (void)ReactorCore->CreateNode<BaseNode>("CoolantAlarm");
}
```

## 4. Why This Structure Helps

### Serialization

You can serialize:

- the whole world
- the whole station level
- one partition level
- one node subtree

### Gameplay host interactions

If your `PlayerStart` sits under the dock partition, spawned pawns inherit a meaningful parent context.

### Editor workflows

Partitions make hierarchy views and scene tools more readable.

### Testing

You can isolate one level partition in a test rather than rebuilding the entire world.

## 5. Add Visual Or Semantic Conventions

A good convention is to make child levels semantic, not purely decorative.

Good names:

- `EnemySpawns`
- `MissionLogic`
- `Props`
- `AudioZones`
- `Lighting`

Bad names:

- `Stuff1`
- `Graph2`
- `SubLayer`

The framework does not enforce this. Your team should.

## 6. Save One Partition Separately

Because `LevelSerializer` operates on a level, you can persist a partition in isolation.

```cpp
if (CommandDeck)
{
    auto Payload = LevelSerializer::Serialize(*CommandDeck);
    if (Payload)
    {
        std::vector<uint8_t> Bytes;
        (void)SerializeLevelPayload(*Payload, Bytes);
    }
}
```

That is the current way to get chunk-like authored content without falling back to the old graph abstraction.

## 7. Fun Extension Ideas

1. Put a `WorldRenderSettings` node under `CommandDeck` for a warmer bridge tone.
2. Put looping alarm audio under `ReactorCore`.
3. Add multiple `PlayerStart` nodes under `DockRing`.
4. Serialize `ReactorCore` separately and instantiate it twice with regenerated ids.

Continue with [Shipyard Save/Load](shipyard_save_load.md).
