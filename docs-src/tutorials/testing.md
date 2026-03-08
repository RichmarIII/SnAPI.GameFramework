# Testing and Validation

The tests in this repository are a good map of what the framework currently guarantees.

## 1. Build And Run Tests

```bash
cmake -S . -B build/debug -DSNAPI_GF_BUILD_TESTS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

## 2. What The Current Test Suite Covers

Representative coverage in `tests/` includes:

- `HandleTests.cpp`
  - end-of-frame deletion semantics
  - UUID-only and runtime-backed handle resolution
- `EcsOnlyWorldTests.cpp`
  - ECS-only world ticks
  - hierarchy mirroring
  - recursive destroy
  - level wrapper behavior
- `GameRuntimeTests.cpp`
  - `GameRuntime::Update()` world driving
  - fixed-step backlog behavior
  - networking subsystem initialization through runtime settings
- `GameplayHostTests.cpp`
  - join/leave flow
  - policy hooks
  - possession selection
  - connection lifecycle callbacks
- `InputSystemTests.cpp`
  - input bootstrap and world tick pumping
- `SerializationTests.cpp`
  - node subtree round-trip
  - `OnCreate` after fields are populated
  - legacy vector/quaternion payload compatibility
  - UUID regeneration/remapping
  - value codec registry behavior
- `LevelWorldSerializationTests.cpp`
  - nested level round-trips
  - world round-trips
  - repeated-instantiation UUID regeneration
- `NetReplicationTests.cpp`
  - spawn/update ordering
  - pending parent/component resolution
  - session-backed snapshot replication
- `WorldNetworkingTests.cpp`
  - world role visibility to nodes/components
  - `CallRPC(...)` routing by role
- `PhysicsIntegrationTests.cpp`
  - runtime physics bootstrap
  - fixed-tick stepping
  - character movement
  - floating-origin behavior
  - sleep/wake activity changes
- `WorldEcsRuntimeTests.cpp`
  - storage priority ordering
  - runtime component attach/remove
  - runtime hierarchy correctness
  - execution-profile gating

## 3. Use Tests As Documentation Of Contracts

If you are unsure whether something is guaranteed, the fastest check is often:

```bash
rg -n "TEST_CASE" tests
```

This repository's tests are useful because many of them target behavioral contracts rather than just implementation helpers.

## 4. Fast Targeted Commands

```bash
ctest --test-dir build/debug --output-on-failure -R "Physics|CharacterMovement"
ctest --test-dir build/debug --output-on-failure -R "Serialization|LevelWorld"
ctest --test-dir build/debug --output-on-failure -R "Networking|Replication|GameplayHost"
```

## 5. Example Runs Still Matter

Run the examples after substantial changes:

```bash
./build/debug/examples/FeatureShowcase/FeatureShowcase
./build/debug/examples/WorldPerfBenchmark/WorldPerfBenchmark
./build/debug/examples/MultiplayerExample/MultiplayerExample --local
./build/debug/examples/MultiplayerExample/MultiplayerExample --server
./build/debug/examples/MultiplayerExample/MultiplayerExample --client
```

Why:

- tests validate contracts in isolation
- examples validate whole-system integration

## 6. Good Change-Specific Checklists

### Reflection or serialization changes

Run:

- `ReflectionTests`
- `SerializationTests`
- `LevelWorldSerializationTests`
- `FeatureShowcase`

### Networking or gameplay host changes

Run:

- `NetReplicationTests`
- `WorldNetworkingTests`
- `GameplayHostTests`
- multiplayer example in at least server/client mode

### Physics changes

Run:

- `PhysicsIntegrationTests`
- any example that relies on fixed-step pawn motion or rigid bodies

### Runtime/world execution changes

Run:

- `GameRuntimeTests`
- `EcsOnlyWorldTests`
- `WorldEcsRuntimeTests`
- `HandleTests`

## 7. A Minimal New Test Pattern

```cpp
#include <catch2/catch_test_macros.hpp>
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

TEST_CASE("World creates and destroys a node")
{
    RegisterBuiltinTypes();

    World WorldInstance("TestWorld");
    auto HandleResult = WorldInstance.CreateNode<BaseNode>("Actor");
    REQUIRE(HandleResult);

    REQUIRE(WorldInstance.DestroyNode(*HandleResult));
    WorldInstance.EndFrame();
}
```

That pattern is intentionally simple:

- set up the minimal world state
- exercise one contract
- flush `EndFrame()` when destruction behavior matters

## What To Read Next

- [Architecture](../architecture.md)
- [First Play Session](first_play_session.md)
