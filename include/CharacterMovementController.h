#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include "CollisionFilters.h"
#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Physics-driven character movement helper component.
 * @remarks
 * This controller applies movement forces to a sibling `RigidBodyComponent` and
 * performs a downward probe to determine grounded state.
 *
 * Input can be provided directly through `SetMoveInput`/`Jump` or indirectly
 * through sibling `InputIntentComponent` when present.
 */
class CharacterMovementController : public BaseComponent, public ComponentCRTP<CharacterMovementController>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CharacterMovementController";

    /**
     * @brief Runtime movement tuning settings.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::CharacterMovementController::Settings";

        float MoveForce = 35.0f; /**< @brief Horizontal acceleration-like value (m/s^2) applied via velocity-change each fixed tick. */
        float JumpImpulse = 4.5f; /**< @brief Upward velocity delta applied when grounded and jump requested. */
        float GroundProbeStartOffset = 0.1f; /**< @brief Extra upward offset above collider top used for grounded ray origin. */
        float GroundProbeDistance = 1.2f; /**< @brief Extra downward reach below collider bottom used for grounded checks. */
        CollisionMaskFlags GroundMask = kCollisionMaskAll; /**< @brief Collision mask for ground probe query. */
        bool ConsumeInputEachTick = false; /**< @brief Clear movement input after each fixed tick when true. */
        bool KeepUpright = true; /**< @brief Lock character roll/pitch by writing yaw-only body rotation each fixed tick. */
    };

    /** @brief Access settings (const). */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /** @brief Access settings for mutation. */
    Settings& EditSettings()
    {
        return m_settings;
    }

    void FixedTick(float DeltaSeconds);
    /** @brief Non-virtual fixed-step entry used by ECS runtime bridge. */
    void RuntimeFixedTick(float DeltaSeconds);
    void FixedTickImpl(IWorld&, float DeltaSeconds) { RuntimeFixedTick(DeltaSeconds); }

    /** @brief Replace current movement input vector. */
    void SetMoveInput(const Vec3& Input);
    /** @brief Add to current movement input vector. */
    void AddMoveInput(const Vec3& Input);
    /** @brief Get current movement input vector. */
    const Vec3& MoveInput() const
    {
        return m_moveInput;
    }

    /** @brief Queue a jump request for next fixed tick. */
    void Jump();

    /** @brief Check grounded result from latest fixed tick probe. */
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
