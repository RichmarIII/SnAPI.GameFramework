# SnAPI::GameFramework::GameRuntimeTickSettings

Frame-phase policy used by `GameRuntime::Update`.

`GameRuntimeTickSettings` controls which world phases run every call to `Update` and how fixed-step simulation time is accumulated. This is the primary place where an application chooses between pure variable-step behavior and a mixed fixed/variable loop.

Units:
- `FixedDeltaSeconds` is measured in seconds.
- `MaxFpsWhenVSyncOff` is measured in frames per second.

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeTickSettings::EnableFixedTick`

Execute fixed-step ticks from accumulator time.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::GameRuntimeTickSettings::FixedDeltaSeconds`

Fixed-step interval used when `EnableFixedTick` is true.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::GameRuntimeTickSettings::MaxFixedStepsPerUpdate`

Safety cap to avoid spiral-of-death under long frames.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeTickSettings::EnableLateTick`

Execute `World::LateTick` each update.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeTickSettings::EnableEndFrame`

Execute `World::EndFrame` each update.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::GameRuntimeTickSettings::MaxFpsWhenVSyncOff`

Optional frame cap applied only while renderer VSync mode is `Off`; `<= 0` disables cap.
</div>
