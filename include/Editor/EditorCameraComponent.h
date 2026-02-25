#pragma once

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Editor-oriented free-fly FPS camera controller.
 * @remarks
 * Controls owner `TransformComponent` using normalized world input:
 * - Hold right mouse button to enable navigation (configurable).
 * - Mouse move adjusts yaw/pitch.
 * - `W/A/S/D` move on local forward/right.
 * - `Q/E` move down/up on world Y.
 * - Shift applies fast-move multiplier (default 2x).
 *
 * Expected pairing:
 * - Attach this component to the same node as `CameraComponent`.
 * - Keep `CameraComponent::Settings::SyncFromTransform = true`.
 */
class EditorCameraComponent final : public BaseComponent, public ComponentCRTP<EditorCameraComponent>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::EditorCameraComponent";
    /** Run before `CameraComponent` so same-frame transform edits are consumed immediately. */
    static constexpr int kTickPriority = -10;

    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::EditorCameraComponent::Settings";

        bool Enabled = true; /**< @brief Master enable gate. */
        bool RequireInputFocus = true; /**< @brief Ignore input when host window is unfocused. */
        bool RequireRightMouseButton = true; /**< @brief Require RMB held for look + movement. */
        bool RequirePointerInsideViewport = true; /**< @brief Only accept navigation input while pointer is inside this camera's render viewport. */
        float MoveSpeed = 12.0f; /**< @brief Base move speed in world units/second. */
        float FastMoveMultiplier = 2.0f; /**< @brief Shift speed multiplier. */
        float LookSensitivity = 0.10f; /**< @brief Degrees per mouse pixel. */
        bool InvertY = false; /**< @brief Invert vertical look axis. */
    };

    [[nodiscard]] const Settings& GetSettings() const
    {
        return m_settings;
    }

    [[nodiscard]] Settings& EditSettings()
    {
        return m_settings;
    }

    void Tick(float DeltaSeconds);
    void TickImpl(IWorld&, float DeltaSeconds) { Tick(DeltaSeconds); }

private:
    void SynchronizeOrientationFromRotation(const Quat& Rotation);
    [[nodiscard]] Quat ComposeRotation() const;

    Settings m_settings{};
    float m_yawDegrees = 0.0f;
    float m_pitchDegrees = 0.0f;
    bool m_orientationInitialized = false;
    bool m_navigationActive = false;
    bool m_hasLastMousePosition = false;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_RENDERER
