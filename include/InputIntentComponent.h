#pragma once

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared transient intent bus for pawn-style movement, jump, and look input.
 *
 * `InputIntentComponent` is the decoupling point between input producers and gameplay consumers.
 * Components such as `InputComponent` publish world-space movement vectors, jump requests, and
 * look deltas here, while movement and camera components consume those values on their own tick.
 *
 * Core semantics:
 * - Movement input is stored as a plain world-space vector and persists until overwritten or cleared.
 * - Jump is a latched boolean edge that remains set until explicitly cleared or consumed.
 * - Look input is an accumulated pair of yaw/pitch deltas in degrees and is cleared by `ConsumeLookInput()`.
 * - Non-finite movement and look values are sanitized away instead of being stored.
 *
 * Ownership and lifetime:
 * - The component owns only its transient runtime state.
 * - No external buffers or handles are borrowed or retained.
 * - The stored values are valid only for the lifetime of this component and are not replicated.
 *
 * Threading model:
 * - Main-thread only.
 * - Not thread-safe; external synchronization would be required for cross-thread producers.
 *
 * Performance notes:
 * - All operations are constant time and allocation-free.
 *
 * @note This component does not impose normalization, clamping, or deadzone rules beyond rejecting
 * non-finite values. Producers define the scale and semantic meaning of the stored intents.
 *
 * @see InputComponent
 * @see CharacterMovementController
 * @see SprintArmComponent
 */
class InputIntentComponent : public BaseComponent, public ComponentCRTP<InputIntentComponent>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::InputIntentComponent";

    /**
     * @brief Replace the current world-space movement intent.
     * @param Input Desired movement vector in world space.
     * @post `MoveWorldInput()` returns either @p Input or zero when @p Input contains non-finite values.
     */
    void SetMoveWorldInput(const Vec3& Input);
    /**
     * @brief Add world-space movement intent to the current accumulated value.
     * @param Input Movement delta in world space.
     * @remarks Non-finite input is ignored.
     */
    void AddMoveWorldInput(const Vec3& Input);
    /**
     * @brief Read the current world-space movement intent.
     * @return Borrowed reference to the stored movement vector.
     */
    const Vec3& MoveWorldInput() const;
    /** @brief Clear movement intent to the zero vector. */
    void ClearMoveWorldInput();

    /**
     * @brief Latch a jump request until a consumer clears or consumes it.
     * @post `JumpRequested()` returns `true`.
     */
    void QueueJump();
    /**
     * @brief Overwrite the stored jump-request state.
     * @param Requested New latched jump state.
     */
    void SetJumpRequested(bool Requested);
    /**
     * @brief Read jump-request state without clearing it.
     * @return `true` when a jump is currently pending.
     */
    bool JumpRequested() const;
    /**
     * @brief Read and clear jump-request state.
     * @return The previously latched jump state.
     * @post `JumpRequested()` returns `false`.
     */
    bool ConsumeJumpRequested();

    /**
     * @brief Replace the current look delta intent.
     * @param YawDeltaDegrees Yaw delta in degrees.
     * @param PitchDeltaDegrees Pitch delta in degrees.
     * @post Non-finite inputs are converted to zero before storage.
     */
    void SetLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);
    /**
     * @brief Accumulate additional look delta intent.
     * @param YawDeltaDegrees Additional yaw delta in degrees.
     * @param PitchDeltaDegrees Additional pitch delta in degrees.
     * @remarks Non-finite inputs are ignored.
     */
    void AddLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);
    /**
     * @brief Read and clear the accumulated look delta intent.
     * @param OutYawDeltaDegrees Receives the stored yaw delta in degrees.
     * @param OutPitchDeltaDegrees Receives the stored pitch delta in degrees.
     * @post Subsequent look reads return zero until new deltas are added.
     */
    void ConsumeLookInput(float& OutYawDeltaDegrees, float& OutPitchDeltaDegrees);
    /** @brief Clear stored look intent to zero. */
    void ClearLookInput();

    /**
     * @brief Clear movement, jump, and look state in one call.
     * @post Movement is zero, jump is not requested, and look deltas are zero.
     */
    void ResetIntents();

private:
    Vec3 m_moveWorldInput = Vec3::Zero();
    bool m_jumpRequested = false;
    float m_lookYawDeltaDegrees = 0.0f;
    float m_lookPitchDeltaDegrees = 0.0f;
};

} // namespace SnAPI::GameFramework
