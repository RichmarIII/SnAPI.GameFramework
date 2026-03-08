# SnAPI::GameFramework::WorldExecutionProfile

Per-frame execution policy used by `World`.

`WorldExecutionProfile` allows one concrete `World` implementation to serve different operating modes by selectively enabling or disabling frame phases and subsystem work. Runtime gameplay, tool-time editor worlds, and PIE all reuse the same data structures but choose different policy defaults.

Semantics:
- Each flag gates one well-defined piece of world behavior for the current frame.
- Profiles are intended to be cheap value objects that can be swapped as mode changes occur.
- Disabling a phase does not necessarily mean the corresponding subsystem is uninitialized; it only changes execution.

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::RunGameplay`

Run gameplay host and ECS runtime tick phases.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::TickInput`

Pump world input in variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::TickUI`

Tick world UI contexts in variable tick.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::PumpNetworking`

Pump networking queues/sessions each frame.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::TickPhysicsSimulation`

Advance physics simulation in variable/fixed phases.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::AllowPhysicsQueries`

Allow query-only physics access even when simulation is disabled.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::TickAudio`

Update world audio subsystem.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::RunNodeEndFrame`

Run node/component end-frame flush.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::BuildUiRenderPackets`

Build UI packets and queue to renderer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldExecutionProfile::RenderFrame`

Submit renderer end-frame.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `WorldExecutionProfile SnAPI::GameFramework::WorldExecutionProfile::Runtime()`

Runtime/game defaults.
</div>
<div class="snapi-api-card" markdown="1">
### `WorldExecutionProfile SnAPI::GameFramework::WorldExecutionProfile::Editor()`

Editor defaults.
</div>
<div class="snapi-api-card" markdown="1">
### `WorldExecutionProfile SnAPI::GameFramework::WorldExecutionProfile::PIE()`

PIE defaults.
</div>
