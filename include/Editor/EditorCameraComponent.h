#pragma once

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Editor-oriented free-fly camera controller.
 *
 * `EditorCameraComponent` turns a node into the standard editor navigation camera.
 * It reads the input snapshot each tick, updates cached yaw and pitch state, and writes the
 * resulting transform back to the owning node's `TransformComponent`.
 *
 * Core semantics:
 * - Navigation is gated by `Settings` and runtime input availability.
 * - Mouse deltas drive yaw and pitch in degrees and pitch is clamped to `[-89, 89]`.
 * - `W/A/S/D` move along the local forward and right basis derived from the current yaw and pitch.
 * - `Q/E` move down and up along world Y.
 * - The first frame after navigation activates re-primes orientation and mouse position to
 *   avoid a large synthetic delta.
 *
 * Expected pairing:
 * - Attach this component to the same node as `CameraComponent`.
 * - Keep `CameraComponent::Settings::SyncFromTransform = true` so camera matrices follow the transform.
 *
 * Ownership and lifetime:
 * - Owned by the usual node/component system.
 * - Requires a live owner node, world, input snapshot, and `TransformComponent` to do useful work.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see CameraComponent
 * @see TransformComponent
 */
class EditorCameraComponent final : public BaseComponent, public ComponentCRTP<EditorCameraComponent>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::EditorCameraComponent";
    /** @brief Run before `CameraComponent` so same-frame transform edits are consumed immediately. */
    static constexpr int kTickPriority = -10;

    /**
     * @brief Runtime tuning parameters for editor camera navigation.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::EditorCameraComponent::Settings";

        bool Enabled = true; /**< @brief Master enable gate. When `false`, the component leaves the transform untouched. */
        bool RequireInputFocus = true; /**< @brief Ignore navigation input while the host window is unfocused. */
        bool RequireRightMouseButton = true; /**< @brief Require the right mouse button to be held before look and move input is consumed. */
        bool RequirePointerInsideViewport = true; /**< @brief Only accept navigation input while the pointer is inside a viewport currently bound to this camera. */
        float MoveSpeed = 12.0f; /**< @brief Base translation speed in world units per second. */
        float FastMoveMultiplier = 2.0f; /**< @brief Additional multiplier applied while Shift is held. */
        float LookSensitivity = 0.10f; /**< @brief Angular look sensitivity measured in degrees per mouse pixel. */
        bool InvertY = false; /**< @brief Invert the sign of vertical look input. */
    };

    /**
     * @brief Access navigation settings.
     * @return Borrowed settings reference.
     */
    [[nodiscard]] const Settings& GetSettings() const
    {
        return m_settings;
    }

    /**
     * @brief Mutably edit navigation settings.
     * @return Borrowed settings reference.
     * @remarks Setting changes take effect on the next tick.
     */
    [[nodiscard]] Settings& EditSettings()
    {
        return m_settings;
    }

    /**
     * @brief Variable-step navigation update.
     * @param DeltaSeconds Frame delta in seconds.
     * @remarks
     * The component early-outs when prerequisites are missing and sanitizes non-finite
     * transform and orientation state before applying input.
     */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief Editor-only tick alias that forwards to `Tick`.
     * @param DeltaSeconds Frame delta in seconds.
     */
    void EditorTick(float DeltaSeconds);
#endif

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
