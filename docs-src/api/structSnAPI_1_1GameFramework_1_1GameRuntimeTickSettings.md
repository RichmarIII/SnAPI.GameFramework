# SnAPI::GameFramework::GameRuntimeTickSettings

Tick/lifecycle policy for `GameRuntime::Update`.

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
