# SnAPI::GameFramework::FollowTargetComponent

Component that follows another node's world transform.

`FollowTargetComponent` is a lightweight spatial constraint. It samples another node's world transform, blends toward the desired position and/or rotation, and then writes the owning node's world transform back through `TransformComponent` helpers.

Typical use:
- attach it to a camera or helper node
- point `Settings::Target` at a player or anchor node
- use smoothing to keep follow behavior out of custom gameplay loops

Core semantics:
- follow operates in world space
- position and rotation synchronization can be enabled independently
- if the owner lacks a `TransformComponent`, the world-transform write path may create one
- UUID fallback can be enabled for replication/serialization restore paths where runtime slot keys are not yet populated

Threading model:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::FollowTargetComponent::Settings

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::FollowTargetComponent::kTypeName`

Stable type name for reflection.
</div>
<div class="snapi-api-card" markdown="1">
### `int SnAPI::GameFramework::FollowTargetComponent::kTickPriority`

Tick ordering hint: follow runs before camera/render consumers.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `Settings SnAPI::GameFramework::FollowTargetComponent::m_settings`

Follow behavior configuration.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `const Settings & SnAPI::GameFramework::FollowTargetComponent::GetSettings() const`

Access settings (const).
</div>
<div class="snapi-api-card" markdown="1">
### `Settings & SnAPI::GameFramework::FollowTargetComponent::EditSettings()`

Access settings for mutation.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::FollowTargetComponent::Tick(float DeltaSeconds)`

Variable-step follow update.

**Parameters**

- `DeltaSeconds`: Frame delta in seconds used for smoothing.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::FollowTargetComponent::ApplyFollow(float DeltaSeconds)`

Execute one follow update using current settings.

**Parameters**

- `DeltaSeconds`: Variable-step delta used for smoothing filters.

**Returns:** True when owner transform was updated.
</div>
