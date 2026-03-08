#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include "CollisionFilters.h"
#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Fixed-step character locomotion helper built on top of a sibling `RigidBodyComponent`.
 *
 * `CharacterMovementController` is a lightweight gameplay controller for upright character motion.
 * It reads movement and jump intent, maintains a grounded cache, and writes the resulting motion
 * into a sibling rigid body during fixed tick.
 *
 * Core semantics:
 * - The controller only performs meaningful work in `FixedTick()`.
 * - Movement is velocity-driven, not force-integrated: the controller computes a target horizontal
 *   velocity each fixed step and writes it directly through `RigidBodyComponent::SetVelocity()`.
 * - Jumping uses a short internal jump buffer and coyote-time window to absorb input/tick jitter.
 * - Grounding is determined by a downward raycast that starts slightly above the collider top and
 *   extends below the collider bottom.
 * - When `KeepUpright` is enabled, pitch and roll are stripped by teleporting the rigid body to a
 *   yaw-only rotation before horizontal movement is applied.
 *
 * Input semantics:
 * - Direct callers can drive the controller through `SetMoveInput()`, `AddMoveInput()`, and `Jump()`.
 * - If a sibling `InputIntentComponent` exists, movement is copied from it each fixed tick and jump
 *   requests are consumed from it.
 * - `ConsumeInputEachTick` clears only this component's stored move vector; it does not clear a sibling
 *   `InputIntentComponent`.
 *
 * Ownership and lifetime:
 * - The controller owns only transient locomotion state such as cached input, grounded state, and
 *   jump-buffer timers.
 * - The actual physics body is owned by the sibling `RigidBodyComponent` / physics scene.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Error semantics:
 * - Missing owner, rigid body, or physics scene is treated as a soft failure.
 * - In those cases the controller clears pending jump/grace state and exits without throwing.
 *
 * @note The implementation currently uses fixed internal grace windows for jump buffering and coyote
 * time rather than exposing them in `Settings`.
 *
 * @see RigidBodyComponent
 * @see ColliderComponent
 * @see InputIntentComponent
 */
class CharacterMovementController : public BaseComponent, public ComponentCRTP<CharacterMovementController>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CharacterMovementController";

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Runtime movement tuning settings for `CharacterMovementController`.
     *
     * These settings are sampled directly during fixed tick. Mutating them affects future simulation
     * steps only; there is no separate rebuild phase.
     */
    struct Settings
    {
        /** @brief Stable reflected type name used for serialization registration. */
        static constexpr const char* kTypeName = "SnAPI::GameFramework::CharacterMovementController::Settings";

        float MoveForce = 35.0f; /**< @brief Horizontal movement tuning scalar. The current implementation multiplies it by a fixed `0.1` scale to derive target planar speed. */
        float JumpImpulse = 4.5f; /**< @brief Upward velocity change applied through `VelocityChange` force mode when a buffered jump is accepted. */
        float GroundProbeStartOffset = 0.1f; /**< @brief Upward offset in world units above the collider top used as the grounded-ray origin. */
        float GroundProbeDistance = 1.2f; /**< @brief Additional downward reach in world units below the collider bottom used for grounded checks. */
        CollisionMaskFlags GroundMask = kCollisionMaskAll; /**< @brief Collision mask used for the grounded probe after the owner's own collision layer is removed. */
        bool ConsumeInputEachTick = false; /**< @brief Clear the controller's stored move vector after each fixed tick. Does not clear sibling `InputIntentComponent` state. */
        bool KeepUpright = true; /**< @brief Force yaw-only orientation by teleporting away pitch and roll each fixed tick. */
    };

    /**
     * @brief Read the current movement settings.
     * @return Borrowed reference to the stored settings object.
     */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /**
     * @brief Mutate the current movement settings.
     * @return Borrowed reference to the stored settings object.
     */
    Settings& EditSettings()
    {
        return m_settings;
    }

    /**
     * @brief Advance character movement during fixed simulation.
     * @param DeltaSeconds Fixed-step delta in seconds.
     *
     * Semantics:
     * - Pulls input from a sibling `InputIntentComponent` when present.
     * - Refreshes grounded state by raycast.
     * - Optionally forces upright rotation.
     * - Writes planar velocity directly to the sibling rigid body.
     * - Applies a buffered jump when the controller is grounded or still inside coyote time.
     */
    void FixedTick(float DeltaSeconds);

    /**
     * @brief Replace the current stored movement input vector.
     * @param Input Desired world-space movement input. The Y component is ignored during locomotion.
     */
    void SetMoveInput(const Vec3& Input);
    /**
     * @brief Add to the current stored movement input vector.
     * @param Input Additional world-space movement input. The Y component is ignored during locomotion.
     */
    void AddMoveInput(const Vec3& Input);
    /**
     * @brief Read the controller's current stored movement input vector.
     * @return Borrowed reference to the stored world-space input vector.
     */
    const Vec3& MoveInput() const
    {
        return m_moveInput;
    }

    /**
     * @brief Queue a jump request for the next eligible fixed tick.
     * @remarks The request is converted into an internal buffered-jump timer during `FixedTick()`.
     */
    void Jump();

    /**
     * @brief Read the grounded result from the latest fixed-step probe.
     * @return `true` when the most recent probe found a valid ground hit.
     */
    bool IsGrounded() const
    {
        return m_grounded;
    }

private:
    bool RefreshGroundedState();

    Settings m_settings{}; /**< @brief Movement settings. */
    Vec3 m_moveInput{}; /**< @brief Current input vector (X/Z expected). */
    Vec3 m_lastPosition{}; /**< @brief Previous world position sample for vertical velocity estimation. */
    bool m_hasLastPosition = false; /**< @brief True when `m_lastPosition` contains a valid sample. */
    bool m_jumpRequested = false; /**< @brief Deferred jump trigger processed on fixed tick. */
    float m_jumpBufferSecondsRemaining = 0.0f; /**< @brief Jump request hold window to absorb tick-order/input timing jitter. */
    float m_groundCoyoteSecondsRemaining = 0.0f; /**< @brief Short post-ground grace window that keeps jump responsive on transient probe misses. */
    bool m_grounded = false; /**< @brief Cached grounded state from latest probe. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
