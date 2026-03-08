# SnAPI::GameFramework::AnimationPayload

Cooked payload for an animation asset.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::AnimationPayload::Name`

Animation clip name.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::AnimationPayload::DurationSeconds`

Clip duration in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::AnimationPayload::TicksPerSecond`

Tick-to-seconds conversion rate used by the source animation.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<AnimationTrackPayload> SnAPI::GameFramework::AnimationPayload::Tracks`

Per-bone animation tracks.
</div>
