#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "BaseComponent.h"
#include "Export.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Camera boom-style component for third-person pawn view control.
 * @remarks
 * Owns yaw/pitch view state and drives sibling `CameraComponent` offsets so camera
 * placement and rotation stay coherent with the pawn body orientation.
 *
 * Look input can be fed directly via `AddLookInput` or through sibling
 * `InputIntentComponent` when present.
 */
class SNAPI_GAMEFRAMEWORK_API SprintArmComponent : public BaseComponent, public ComponentCRTP<SprintArmComponent>
{
public:
    /** @brief Stable type name for reflection and serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SprintArmComponent";
    /** @brief Tick ordering hint: apply arm state before camera component updates. */
    static constexpr int kTickPriority = -5;

    /**
     * @brief Runtime configuration for sprint arm pose and behavior.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::SprintArmComponent::Settings";

        bool Enabled = true; /**< @brief Global runtime toggle for sprint arm behavior. */
        bool DriveOwnerYaw = true; /**< @brief When true, writes yaw-only rotation back to owning node transform. */
        float ArmLength = 2.8f; /**< @brief Distance from socket pivot to camera along local +Z (behind -Z facing pawn). */
        Vec3 SocketOffset = Vec3(0.0f, 1.35f, 0.0f); /**< @brief Local socket pivot offset from owner origin. */
        float YawDegrees = 0.0f; /**< @brief Current view yaw in degrees. */
        float PitchDegrees = -12.0f; /**< @brief Current view pitch in degrees. */
        float MinPitchDegrees = -80.0f; /**< @brief Minimum allowed pitch in degrees. */
        float MaxPitchDegrees = 80.0f; /**< @brief Maximum allowed pitch in degrees. */
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

    void OnCreate();
    /** @brief Variable-step input staging pass (currently unused). */
    void Tick(float DeltaSeconds);
    /** @brief Variable-step view/camera application pass. */
    void LateTick(float DeltaSeconds);

    /** @brief Non-virtual create entry used by ECS runtime bridge. */
    void RuntimeOnCreate();
    /** @brief Non-virtual variable-step entry used by ECS runtime bridge. */
    void RuntimeTick(float DeltaSeconds);
    /** @brief Non-virtual late-step entry used by ECS runtime bridge. */
    void RuntimeLateTick(float DeltaSeconds);

    void OnCreateImpl(IWorld&) { RuntimeOnCreate(); }
    void TickImpl(IWorld&, float DeltaSeconds) { RuntimeTick(DeltaSeconds); }
    void LateTickImpl(IWorld&, float DeltaSeconds) { RuntimeLateTick(DeltaSeconds); }

    /**
     * @brief Queue additive view input in degrees.
     * @param YawDeltaDegrees Positive values turn view right.
     * @param PitchDeltaDegrees Positive values look up.
     */
    void AddLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);

    /**
     * @brief Overwrite current view angles.
     * @param YawDegrees Absolute yaw in degrees.
     * @param PitchDegrees Absolute pitch in degrees.
     */
    void SetViewAngles(float YawDegrees, float PitchDegrees);

    /** @brief Current resolved yaw in degrees. */
    float YawDegrees() const
    {
        return m_yawDegrees;
    }

    /** @brief Current resolved pitch in degrees. */
    float PitchDegrees() const
    {
        return m_pitchDegrees;
    }

private:
    void InitializeFromOwner();
    void ApplyArmToOwnerAndCamera();
    void PullLookInputIntent();
    void ApplyPendingLookInput();

    Settings m_settings{};
    float m_yawDegrees = 0.0f;
    float m_pitchDegrees = 0.0f;
    float m_pendingYawDeltaDegrees = 0.0f;
    float m_pendingPitchDeltaDegrees = 0.0f;
    bool m_initialized = false;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
