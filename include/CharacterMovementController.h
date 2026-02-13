#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include "CollisionFilters.h"
#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Physics-driven character movement helper component.
 * @remarks
 * This controller applies movement forces to a sibling `RigidBodyComponent` and
 * performs a downward probe to determine grounded state.
 */
class CharacterMovementController : public IComponent
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
        float JumpImpulse = 4.5f; /**< @brief Upward impulse applied when grounded and jump requested. */
        float GroundProbeStartOffset = 0.1f; /**< @brief Upward offset from transform position for ground probe origin. */
        float GroundProbeDistance = 1.2f; /**< @brief Downward probe distance used for grounded checks. */
        CollisionMaskFlags GroundMask = kCollisionMaskAll; /**< @brief Collision mask for ground probe query. */
        bool ConsumeInputEachTick = false; /**< @brief Clear movement input after each fixed tick when true. */
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

    void FixedTick(float DeltaSeconds) override;

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
    bool m_grounded = false; /**< @brief Cached grounded state from latest probe. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
