---
title: SnAPI.GameFramework
---

<div class="snapi-hero">
  <div>
    <p class="snapi-kicker">Gameplay framework for C++23</p>
    <h1>SnAPI.GameFramework</h1>
    <p>
      A world-owned gameplay framework built around <code>GameRuntime</code>, <code>World</code>,
      <code>Level</code>, <code>BaseNode</code>, <code>BaseComponent</code>, reflection,
      serialization, replication, and subsystem adapters for input, UI, networking, physics,
      audio, rendering, and scripting.
    </p>
    <div class="snapi-actions">
      <a class="md-button md-button--primary" href="tutorials/">Start Here</a>
      <a class="md-button" href="architecture/">Architecture</a>
      <a class="md-button" href="api/">API Reference</a>
    </div>
    <div class="snapi-badges">
      <span class="snapi-badge">GameRuntime</span>
      <span class="snapi-badge">World + Levels</span>
      <span class="snapi-badge">Nodes + Components</span>
      <span class="snapi-badge">Reflection</span>
      <span class="snapi-badge">Serialization</span>
      <span class="snapi-badge">Replication + RPC</span>
      <span class="snapi-badge">GameplayHost</span>
      <span class="snapi-badge">Physics</span>
      <span class="snapi-badge">Renderer</span>
      <span class="snapi-badge">Editor + UI Viewports</span>
    </div>
    <div class="snapi-hero__features">
      <strong>What the current framework actually looks like</strong>
      <ul>
        <li><code>GameRuntime</code> owns one <code>World</code> and drives init, frame update, and shutdown.</li>
        <li><code>World</code> owns the node hierarchy, dense node/component storage, script runtime, and optional subsystems.</li>
        <li><code>Level</code> is a convenience node type for grouping content. Nested partitions are built with child <code>Level</code> nodes, not a separate public <code>NodeGraph</code> type.</li>
        <li><code>BaseNode</code> and <code>BaseComponent</code> are the main gameplay building blocks users derive from.</li>
        <li>Handles are durable identity. Borrowed pointers are short-lived views into world-owned storage.</li>
        <li>Reflection metadata powers serialization, replication, editor property panels, and reflected RPC dispatch.</li>
        <li><code>NodeSerializer</code>, <code>LevelSerializer</code>, and <code>WorldSerializer</code> replace the old graph serializer flow.</li>
        <li><code>NetworkSystem</code> wires <code>NetReplicationBridge</code> and <code>NetRpcBridge</code> into the active session.</li>
        <li><code>GameplayHost</code> manages high-level join/leave/load flows, game/game-mode objects, and local-player state.</li>
        <li>Editor bootstrap now defers node/component <code>OnCreate</code> work until the UI viewport and render path are ready.</li>
      </ul>
    </div>
  </div>
</div>

## Quick Start

```bash
cmake -S . -B build/debug \
  -DSNAPI_GF_BUILD_TESTS=ON \
  -DSNAPI_GF_BUILD_EXAMPLES=ON \
  -DSNAPI_GF_BUILD_DOCS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

## If You Read The Old Docs

The biggest conceptual change is this:

- Old docs talked about `World -> Level -> NodeGraph -> Node -> Component`.
- The current public model is `GameRuntime -> World -> Level -> BaseNode/BaseComponent`.
- If you want nested content partitions, create child `Level` nodes.
- Dense storage is now the default runtime model for both nodes and components; `WorldEcsRuntime` is the scheduling/storage layer behind the normal API, not a separate user-facing mirror.

## Recommended First Reads

<div class="snapi-grid">
  <a class="snapi-card" href="tutorials/worlds_graphs/">
    <h3>Worlds, Levels, and Hierarchies</h3>
    <p>Learn the real object model before touching subsystems.</p>
  </a>
  <a class="snapi-card" href="tutorials/nodes_components/">
    <h3>Nodes and Components</h3>
    <p>Build gameplay objects, attach components, and understand ownership.</p>
  </a>
  <a class="snapi-card" href="tutorials/first_play_session/">
    <h3>First Play Session</h3>
    <p>Build a tiny playable runtime using the current framework flow.</p>
  </a>
  <a class="snapi-card" href="tutorials/tool_world_vs_game_world/">
    <h3>Tool World vs Game World</h3>
    <p>Understand execution profiles, deferred OnCreate, and editor/runtime differences.</p>
  </a>
  <a class="snapi-card" href="tutorials/networking/">
    <h3>Networking</h3>
    <p>See how replication, reflected RPC, and GameplayHost fit together now.</p>
  </a>
  <a class="snapi-card" href="tutorials/scripted_gadget_lab/">
    <h3>Scripted Gadget Lab</h3>
    <p>Wire a script-backed gameplay object into the runtime lifecycle.</p>
  </a>
</div>

## Newcomer Project Labs

These are the fun, guided tutorials added for this docs refresh:

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

## Module Map

- [Start Here](tutorials.md): guided learning order and tutorial catalog
- [Architecture](architecture.md): frame order, init order, ownership, and subsystem model
- [API Reference](api/index.md): generated from the public headers; regenerate it after major API refactors if stale legacy types still appear
- `Docs/GameFramework/README.md` in the repository: module overview used as the contract-writing baseline for the API contract pass
