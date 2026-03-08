# SnAPI::GameFramework::InputIntentComponent

Shared transient intent bus for pawn-style movement, jump, and look input.

`InputIntentComponent` is the decoupling point between input producers and gameplay consumers. Components such as `InputComponent` publish world-space movement vectors, jump requests, and look deltas here, while movement and camera components consume those values on their own tick.

Core semantics:
- Movement input is stored as a plain world-space vector and persists until overwritten or cleared.
- Jump is a latched boolean edge that remains set until explicitly cleared or consumed.
- Look input is an accumulated pair of yaw/pitch deltas in degrees and is cleared by `ConsumeLookInput()`.
- Non-finite movement and look values are sanitized away instead of being stored.

Ownership and lifetime:
- The component owns only its transient runtime state.
- No external buffers or handles are borrowed or retained.
- The stored values are valid only for the lifetime of this component and are not replicated.

Threading model:
- Main-thread only.
- Not thread-safe; external synchronization would be required for cross-thread producers.

Performance notes:
- All operations are constant time and allocation-free.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::InputIntentComponent::kTypeName`

Stable reflected type name used for serialization registration.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::InputIntentComponent::m_moveWorldInput`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::InputIntentComponent::m_jumpRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::InputIntentComponent::m_lookYawDeltaDegrees`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::InputIntentComponent::m_lookPitchDeltaDegrees`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::SetMoveWorldInput(const Vec3 &Input)`

Replace the current world-space movement intent.

**Parameters**

- `Input`: Desired movement vector in world space.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::AddMoveWorldInput(const Vec3 &Input)`

Add world-space movement intent to the current accumulated value.

**Parameters**

- `Input`: Movement delta in world space.
</div>
<div class="snapi-api-card" markdown="1">
### `const Vec3 & SnAPI::GameFramework::InputIntentComponent::MoveWorldInput() const`

Read the current world-space movement intent.

**Returns:** Borrowed reference to the stored movement vector.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::ClearMoveWorldInput()`

Clear movement intent to the zero vector.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::QueueJump()`

Latch a jump request until a consumer clears or consumes it.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::SetJumpRequested(bool Requested)`

Overwrite the stored jump-request state.

**Parameters**

- `Requested`: New latched jump state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::InputIntentComponent::JumpRequested() const`

Read jump-request state without clearing it.

**Returns:** `true` when a jump is currently pending.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::InputIntentComponent::ConsumeJumpRequested()`

Read and clear jump-request state.

**Returns:** The previously latched jump state.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::SetLookInput(float YawDeltaDegrees, float PitchDeltaDegrees)`

Replace the current look delta intent.

**Parameters**

- `YawDeltaDegrees`: Yaw delta in degrees.
- `PitchDeltaDegrees`: Pitch delta in degrees.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::AddLookInput(float YawDeltaDegrees, float PitchDeltaDegrees)`

Accumulate additional look delta intent.

**Parameters**

- `YawDeltaDegrees`: Additional yaw delta in degrees.
- `PitchDeltaDegrees`: Additional pitch delta in degrees.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::ConsumeLookInput(float &OutYawDeltaDegrees, float &OutPitchDeltaDegrees)`

Read and clear the accumulated look delta intent.

**Parameters**

- `OutYawDeltaDegrees`: Receives the stored yaw delta in degrees.
- `OutPitchDeltaDegrees`: Receives the stored pitch delta in degrees.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::ClearLookInput()`

Clear stored look intent to zero.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::InputIntentComponent::ResetIntents()`

Clear movement, jump, and look state in one call.
</div>
