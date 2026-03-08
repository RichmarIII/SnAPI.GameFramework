# Start Here

This guide is for someone new to the framework who wants the current, accurate mental model before building anything serious.

The docs now assume the modern runtime shape:

- `GameRuntime`
- `World`
- `Level`
- `BaseNode`
- `BaseComponent`
- optional subsystems owned by the world
- optional `GameplayHost` for higher-level session logic

## Build Once Before Reading

```bash
cmake -S . -B build/debug \
  -DSNAPI_GF_BUILD_TESTS=ON \
  -DSNAPI_GF_BUILD_EXAMPLES=ON \
  -DSNAPI_GF_BUILD_DOCS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Useful example binaries:

```bash
./build/debug/examples/FeatureShowcase/FeatureShowcase
./build/debug/examples/WorldPerfBenchmark/WorldPerfBenchmark
./build/debug/examples/MultiplayerExample/MultiplayerExample --local
./build/debug/examples/MultiplayerExample/MultiplayerExample --server
./build/debug/examples/MultiplayerExample/MultiplayerExample --client
```

## Suggested Learning Order

### Core framework path

1. [Worlds, Levels, and Hierarchies](tutorials/worlds_graphs.md)
2. [Nodes and Components](tutorials/nodes_components.md)
3. [Input System](tutorials/input.md)
4. [UI System](tutorials/ui.md)
5. [Renderer Integration](tutorials/renderer.md)
6. [Physics System and Components](tutorials/physics.md)
7. [Physics Queries and Events](tutorials/physics_queries_events.md)
8. [Reflection and Serialization](tutorials/reflection_serialization.md)
9. [AssetPipeline Integration](tutorials/assetpipeline.md)
10. [Networking, Replication, and Reflected RPC](tutorials/networking.md)
11. [Audio Components](tutorials/audio.md)
12. [Testing and Validation](tutorials/testing.md)

### Project-style newcomer labs

1. [First Play Session](tutorials/first_play_session.md)
2. [Space Station Partitions](tutorials/space_station_partitions.md)
3. [Couch Co-op Laser Tag](tutorials/couch_coop_laser_tag.md)
4. [Haunted Radio](tutorials/haunted_radio.md)
5. [Bouncy Basement](tutorials/bouncy_basement.md)
6. [Shipyard Save/Load](tutorials/shipyard_save_load.md)
7. [Postcard Renderer](tutorials/postcard_renderer.md)
8. [Net Arena](tutorials/net_arena.md)
9. [Tool World vs Game World](tutorials/tool_world_vs_game_world.md)
10. [Scripted Gadget Lab](tutorials/scripted_gadget_lab.md)

## The Five Rules To Remember

1. Handles are identity. Borrowed pointers are not ownership.
2. `World` owns the content hierarchy and the optional subsystems.
3. `Level` is a node. Use child `Level` nodes instead of looking for an old `NodeGraph` layer.
4. `GameRuntime` is the easiest way to get a correct frame loop.
5. Serialization, replication, and reflected RPC all depend on reflection metadata being correct.

## Which Tutorials Matter Most For Different Goals

### I want to make a single-player prototype

Read:

1. [Worlds, Levels, and Hierarchies](tutorials/worlds_graphs.md)
2. [Nodes and Components](tutorials/nodes_components.md)
3. [First Play Session](tutorials/first_play_session.md)
4. [Physics System and Components](tutorials/physics.md)
5. [Renderer Integration](tutorials/renderer.md)

### I want to understand multiplayer/session flow

Read:

1. [Networking, Replication, and Reflected RPC](tutorials/networking.md)
2. [Net Arena](tutorials/net_arena.md)
3. [Couch Co-op Laser Tag](tutorials/couch_coop_laser_tag.md)
4. [Audio Components](tutorials/audio.md)

### I want to work on tools/editor code

Read:

1. [Architecture](architecture.md)
2. [UI System](tutorials/ui.md)
3. [Renderer Integration](tutorials/renderer.md)
4. [Tool World vs Game World](tutorials/tool_world_vs_game_world.md)

### I want to understand persistence and assets

Read:

1. [Reflection and Serialization](tutorials/reflection_serialization.md)
2. [AssetPipeline Integration](tutorials/assetpipeline.md)
3. [Shipyard Save/Load](tutorials/shipyard_save_load.md)

## A Note About Older Terminology

If you still see `NodeGraph` in old notes, blog posts, or stale generated content, translate it like this:

- scene partitioning -> child `Level` nodes
- standalone graph serialization -> node, level, or world serialization depending on scope
- graph ownership -> world ownership

Start with [Worlds, Levels, and Hierarchies](tutorials/worlds_graphs.md).
