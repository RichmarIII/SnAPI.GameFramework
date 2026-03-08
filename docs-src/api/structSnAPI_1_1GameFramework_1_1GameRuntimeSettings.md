# SnAPI::GameFramework::GameRuntimeSettings

Bootstrap and runtime-policy settings consumed by `GameRuntime::Init`.

`GameRuntimeSettings` describes everything needed to create one runtime session: world construction, subsystem bootstrap parameters, gameplay host configuration, and frame-loop policy. The structure is intentionally value-based so apps, tests, and tools can assemble settings in-place without needing a builder object.

Ownership:
- `WorldFactory`, when provided, transfers ownership of the returned `World` to `GameRuntime`.
- Optional subsystem settings enable initialization; the subsystem instances themselves are still owned by the world.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::GameRuntimeSettings::WorldName`

Name assigned to the created world instance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<std::unique_ptr<class World>(std::string)> SnAPI::GameFramework::GameRuntimeSettings::WorldFactory`

Optional world factory override (defaults to `World`).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::GameRuntimeSettings::RegisterBuiltins`

Register built-in reflection/serialization types once during init.
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntimeTickSettings SnAPI::GameFramework::GameRuntimeSettings::Tick`

Tick/lifecycle policy for `Update`.
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<GameRuntimeGameplaySettings> SnAPI::GameFramework::GameRuntimeSettings::Gameplay`

Optional high-level gameplay orchestration settings.
</div>
