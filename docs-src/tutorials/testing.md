# Testing and Validation

This page gives a practical test workflow for GameFramework features.

## 1. Build and Run Unit Tests

```bash
cmake -S . -B build/debug -DSNAPI_GF_BUILD_TESTS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

The main test binary is `GameFrameworkTests` (Catch2).

## 2. What Is Already Covered

Current tests include:

- `tests/HandleTests.cpp`
    - handle validity and end-of-frame deletion behavior
- `tests/NodeGraphTests.cpp`
    - node + component ticking through graph hierarchy
- `tests/ReflectionTests.cpp`
    - type registration, inheritance, field/method flags, audio RPC endpoint metadata, and audio settings replication flags
- `tests/SerializationTests.cpp`
    - graph serialization round-trip, cross-graph handle resolution, custom `TValueCodec`
- `tests/LevelWorldSerializationTests.cpp`
    - level/world serialization round-trips
- `tests/RelevanceTests.cpp`
    - relevance policy gating tick
- `tests/NetReplicationTests.cpp`
    - replication spawn/update ordering and session integration
- `tests/WorldNetworkingTests.cpp`
    - world-owned networking system wiring, replication/rpc bridge integration
    - `INode::CallRPC` / `IComponent::CallRPC` role-based routing behavior
    - component `TypeKey` assignment for reflection RPC dispatch

## 3. Run Integration Examples

Unit tests are necessary but not sufficient for gameplay systems.
Also run these binaries:

```bash
./build/debug/examples/FeatureShowcase/FeatureShowcase
./build/debug/examples/WorldPerfBenchmark/WorldPerfBenchmark
./build/debug/examples/MultiplayerExample/MultiplayerExample --server
./build/debug/examples/MultiplayerExample/MultiplayerExample --client
```

This validates end-to-end behavior (asset packs, runtime factories, replication bridges, rendering loop).

## 4. Suggested Test Matrix For Changes

If you change reflection code:

- run `ReflectionTests` and `SerializationTests`
- verify lazy auto-registration still resolves types on first use

If you change serialization code:

- run `SerializationTests`, `LevelWorldSerializationTests`
- run `FeatureShowcase` to confirm real payload load path

If you change networking/replication code:

- run `NetReplicationTests`
- run `WorldNetworkingTests`
- validate `CallRPC(...)` routes correctly for server/client/listen-server roles
- run multiplayer example across two processes/devices
- watch connection dumps:
    - `pending_rel` should not grow forever
    - non-zero `pkt_lost` alone is not a failure if reliable backlog drains and gameplay state remains correct

If you change component lifecycle code:

- run `NodeGraphTests` and `HandleTests`
- verify deferred destruction semantics still hold

## 5. Add a New Test (Template)

```cpp
#include <catch2/catch_test_macros.hpp>
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

TEST_CASE("My feature round-trips")
{
    RegisterBuiltinTypes();

    NodeGraph Graph;
    auto NodeResult = Graph.CreateNode("Actor");
    REQUIRE(NodeResult);

    auto* Node = NodeResult->Borrowed();
    REQUIRE(Node != nullptr);

    // setup

    auto Payload = NodeGraphSerializer::Serialize(Graph);
    REQUIRE(Payload);

    NodeGraph Loaded;
    REQUIRE(NodeGraphSerializer::Deserialize(Payload.value(), Loaded));

    // assert
}
```

## 6. CI-Friendly Command Set

```bash
cmake -S . -B build/ci -DSNAPI_GF_BUILD_TESTS=ON -DSNAPI_GF_BUILD_EXAMPLES=ON
cmake --build build/ci --config Release
ctest --test-dir build/ci --output-on-failure
```

Use this baseline before merging engine-level changes.
