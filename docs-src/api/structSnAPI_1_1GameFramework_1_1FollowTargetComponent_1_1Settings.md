# SnAPI::GameFramework::FollowTargetComponent::Settings

Follow behavior settings.

Units:
- `PositionOffset` is in world units
- smoothing values are exponential frequencies in hertz

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::FollowTargetComponent::Settings::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::FollowTargetComponent::Settings::Target`

Target node to follow.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::FollowTargetComponent::Settings::PositionOffset`

World-space offset added to target position when syncing position.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::FollowTargetComponent::Settings::SyncPosition`

Enable position follow.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::FollowTargetComponent::Settings::SyncRotation`

Enable rotation follow from target rotation.
</div>
<div class="snapi-api-card" markdown="1">
### `Quat SnAPI::GameFramework::FollowTargetComponent::Settings::RotationOffset`

Extra rotation applied after followed target rotation when SyncRotation is true.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::FollowTargetComponent::Settings::PositionSmoothingHz`

Exponential smoothing frequency for position (0 = instant snap).
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::FollowTargetComponent::Settings::RotationSmoothingHz`

Exponential smoothing frequency for rotation (0 = instant snap).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::FollowTargetComponent::Settings::ResolveTargetByUuidFallback`

Resolve target through UUID fallback when runtime key path is unavailable.
</div>
