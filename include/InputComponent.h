#pragma once

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)

#include <Input.h>

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Bridges normalized world input snapshots into `InputIntentComponent`.
 * @remarks
 * This component samples `World::Input().Snapshot()` each frame and writes
 * movement/jump/look intent to sibling `InputIntentComponent`, allowing movement
 * and camera systems to consume input through a shared, decoupled contract.
 *
 * Intended usage:
 * - Add this component to a controllable node (typically one that has
 *   `CharacterMovementController` and/or `SprintArmComponent`).
 * - Tune bindings and analog shaping through `Settings`.
 * - Keep gameplay code backend-agnostic by consuming normalized input only.
 * - When `LocalPlayer` possession exists, input is automatically routed through the
 *   possessing local player and its assigned input device.
 */
class InputComponent : public BaseComponent, public ComponentCRTP<InputComponent>
{
public:
    /** @brief Stable type name used for reflection and serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::InputComponent";

    /**
     * @brief Runtime binding and shaping configuration for `InputComponent`.
     * @remarks
     * Keyboard and gamepad input sources can be enabled independently and are
     * merged before optional normalization/scaling.
     */
    struct Settings
    {
        /** @brief Stable type name used for reflection and serialization registration. */
        static constexpr const char* kTypeName = "SnAPI::GameFramework::InputComponent::Settings";

        bool MovementEnabled = true; /**< @brief Enables movement intent publishing into sibling `InputIntentComponent`. */
        bool JumpEnabled = true; /**< @brief Enables jump intent publishing into sibling `InputIntentComponent`. */
        bool KeyboardEnabled = true; /**< @brief Enables keyboard source contribution. */
        bool GamepadEnabled = true; /**< @brief Enables gamepad source contribution. */
        bool RequireInputFocus = true; /**< @brief Suppresses movement/jump when input context reports focus lost. */
        bool NormalizeMove = true; /**< @brief Normalizes merged X/Z movement to unit length before scaling. */
        bool ClearMoveWhenUnavailable = true; /**< @brief Writes zero movement intent when input system/snapshot/focus is unavailable. */
        bool LookEnabled = true; /**< @brief Enables look intent publishing into sibling `InputIntentComponent`. */
        bool MouseLookEnabled = true; /**< @brief Enables mouse delta contribution to look input. */
        bool GamepadLookEnabled = true; /**< @brief Enables right-stick look contribution. */
        bool RequireRightMouseButtonForLook = false; /**< @brief Require RMB held for mouse look when true. */

        float MoveScale = 1.0f; /**< @brief Scalar multiplier applied after optional movement normalization. */
        float GamepadDeadzone = 0.2f; /**< @brief Per-axis deadzone in [0, 0.99] for gamepad analog movement. */
        bool InvertGamepadY = false; /**< @brief Inverts configured gamepad Y axis before mapping to world Z movement. */
        float MouseLookSensitivity = 0.12f; /**< @brief Degrees applied per mouse pixel of movement. */
        bool InvertMouseY = false; /**< @brief Invert vertical mouse look axis. */
        float GamepadLookSensitivity = 180.0f; /**< @brief Degrees-per-second scale for gamepad look input. */
        bool InvertGamepadLookY = false; /**< @brief Invert vertical gamepad look axis. */

        SnAPI::Input::EKey MoveForwardKey = SnAPI::Input::EKey::W; /**< @brief Keyboard key mapped to forward movement (negative Z by default in this component). */
        SnAPI::Input::EKey MoveBackwardKey = SnAPI::Input::EKey::S; /**< @brief Keyboard key mapped to backward movement (positive Z by default in this component). */
        SnAPI::Input::EKey MoveLeftKey = SnAPI::Input::EKey::A; /**< @brief Keyboard key mapped to left movement (negative X). */
        SnAPI::Input::EKey MoveRightKey = SnAPI::Input::EKey::D; /**< @brief Keyboard key mapped to right movement (positive X). */
        SnAPI::Input::EKey JumpKey = SnAPI::Input::EKey::Space; /**< @brief Keyboard key mapped to jump trigger. */

        SnAPI::Input::EGamepadAxis MoveGamepadXAxis = SnAPI::Input::EGamepadAxis::LeftX; /**< @brief Gamepad axis used for X movement contribution. */
        SnAPI::Input::EGamepadAxis MoveGamepadYAxis = SnAPI::Input::EGamepadAxis::LeftY; /**< @brief Gamepad axis used for Z movement contribution. */
        SnAPI::Input::EGamepadAxis LookGamepadXAxis = SnAPI::Input::EGamepadAxis::RightX; /**< @brief Gamepad axis used for yaw look contribution. */
        SnAPI::Input::EGamepadAxis LookGamepadYAxis = SnAPI::Input::EGamepadAxis::RightY; /**< @brief Gamepad axis used for pitch look contribution. */
        SnAPI::Input::EGamepadButton JumpGamepadButton = SnAPI::Input::EGamepadButton::South; /**< @brief Gamepad button mapped to jump trigger. */

        SnAPI::Input::DeviceId PreferredGamepad{}; /**< @brief Optional preferred gamepad device id; zero means auto-select first connected pad. */
        bool UseAnyGamepadWhenPreferredMissing = true; /**< @brief Allows fallback to first connected gamepad when preferred id is not currently connected. */
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

    /** @brief Ensure required sibling intent component exists. */
    void OnCreate();
    /** @brief Non-virtual create entry used by ECS runtime bridge. */
    void RuntimeOnCreate();
    /**
     * @brief Per-frame input sampling and intent publishing.
     * @param DeltaSeconds Time since last variable tick.
     * @remarks `DeltaSeconds` is currently unused; this method is edge/state driven.
     */
    void Tick(float DeltaSeconds);
    /** @brief Non-virtual per-frame entry used by ECS runtime bridge. */
    void RuntimeTick(float DeltaSeconds);
    void OnCreateImpl(IWorld&) { RuntimeOnCreate(); }
    void TickImpl(IWorld&, float DeltaSeconds) { RuntimeTick(DeltaSeconds); }

private:
    /**
     * @brief Resolve the gamepad id used for this frame.
     * @param Snapshot Current normalized input snapshot.
     * @return Selected connected gamepad id, or invalid id when none should be used.
     */
    [[nodiscard]] SnAPI::Input::DeviceId ResolveGamepadDevice(const SnAPI::Input::InputSnapshot& Snapshot) const;

    /**
     * @brief Apply configured deadzone shaping to an analog axis.
     * @param Value Raw normalized axis value in [-1, 1].
     * @return Deadzone-shaped normalized axis value in [-1, 1].
     */
    [[nodiscard]] float ApplyDeadzone(float Value) const;

    Settings m_settings{}; /**< @brief Runtime binding/shaping settings for this input bridge component. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_PHYSICS
