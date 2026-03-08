#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "BaseComponent.h"
#include "Export.h"
#include "Math.h"
#include <string_view>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Camera-boom component for third-person pawn view control.
 *
 * `SprintArmComponent` stores view yaw/pitch state and writes the resulting boom offset
 * into a sibling `CameraComponent`. It can also optionally rotate the owning node's yaw
 * so body orientation follows the view, making it the stock third-person camera rig helper
 * used by built-in pawn types.
 *
 * Core semantics:
 * - actual camera application happens in `LateTick(...)`
 * - queued look input is accumulated through `AddLookInput(...)` and/or consumed from a sibling `InputIntentComponent`
 * - when `DriveOwnerYaw` is enabled, only yaw is written back to the owning node transform
 * - the component forces the sibling camera to use transform synchronization and writes camera local offsets each frame
 *
 * Ownership and lifetime:
 * - The component does not own the camera; it mutates a sibling `CameraComponent` if one exists.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see CameraComponent
 * @see InputIntentComponent
 */
class SNAPI_GAMEFRAMEWORK_API SprintArmComponent : public BaseComponent, public ComponentCRTP<SprintArmComponent>
{
public:
    /** @brief Stable type name for reflection and serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SprintArmComponent";
    /** @brief Tick ordering hint: apply arm state before camera component updates. */
    static constexpr int kTickPriority = -5;

    /**
     * @brief Runtime configuration for spring-arm pose and behavior.
     *
     * Units:
     * - angles are in degrees
     * - `ArmLength` and `SocketOffset` are in world units
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

    /** @brief Initialize internal yaw/pitch state from settings and owner pose, then apply the arm immediately. */
    void OnCreate();
    /** @brief Variable-step input staging pass. @param DeltaSeconds Frame delta in seconds. @remarks Currently retained for tick-order symmetry. */
    void Tick(float DeltaSeconds);
    /** @brief Variable-step view/camera application pass. @param DeltaSeconds Frame delta in seconds. */
    void LateTick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only property change hook. @param Name Changed reflected field name. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

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
