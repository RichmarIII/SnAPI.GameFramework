#pragma once

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)

#include <Input.h>

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Bridges normalized world input snapshot data into character movement commands.
 * @remarks
 * This component reads `World::Input().Snapshot()` each frame and translates configured
 * keyboard/gamepad bindings into:
 * - planar movement (`CharacterMovementController::SetMoveInput`)
 * - jump trigger edges (`CharacterMovementController::Jump`)
 *
 * Intended usage:
 * - Add this component to a node that already owns `CharacterMovementController`.
 * - Tune bindings and analog shaping through `Settings`.
 * - Keep gameplay code backend-agnostic by consuming normalized input only.
 * - When `LocalPlayer` possession exists, input is automatically routed through the
 *   possessing local player and its assigned input device.
 */
class InputComponent : public IComponent
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

        bool MovementEnabled = true; /**< @brief Enables movement vector dispatch into `CharacterMovementController`. */
        bool JumpEnabled = true; /**< @brief Enables jump trigger dispatch into `CharacterMovementController`. */
        bool KeyboardEnabled = true; /**< @brief Enables keyboard source contribution. */
        bool GamepadEnabled = true; /**< @brief Enables gamepad source contribution. */
        bool RequireInputFocus = true; /**< @brief Suppresses movement/jump when input context reports focus lost. */
        bool NormalizeMove = true; /**< @brief Normalizes merged X/Z movement to unit length before scaling. */
        bool ClearMoveWhenUnavailable = true; /**< @brief Writes zero movement when input system/snapshot/controller is unavailable. */

        float MoveScale = 1.0f; /**< @brief Scalar multiplier applied after optional movement normalization. */
        float GamepadDeadzone = 0.2f; /**< @brief Per-axis deadzone in [0, 0.99] for gamepad analog movement. */
        bool InvertGamepadY = false; /**< @brief Inverts configured gamepad Y axis before mapping to world Z movement. */

        SnAPI::Input::EKey MoveForwardKey = SnAPI::Input::EKey::W; /**< @brief Keyboard key mapped to forward movement (negative Z by default in this component). */
        SnAPI::Input::EKey MoveBackwardKey = SnAPI::Input::EKey::S; /**< @brief Keyboard key mapped to backward movement (positive Z by default in this component). */
        SnAPI::Input::EKey MoveLeftKey = SnAPI::Input::EKey::A; /**< @brief Keyboard key mapped to left movement (negative X). */
        SnAPI::Input::EKey MoveRightKey = SnAPI::Input::EKey::D; /**< @brief Keyboard key mapped to right movement (positive X). */
        SnAPI::Input::EKey JumpKey = SnAPI::Input::EKey::Space; /**< @brief Keyboard key mapped to jump trigger. */

        SnAPI::Input::EGamepadAxis MoveGamepadXAxis = SnAPI::Input::EGamepadAxis::LeftX; /**< @brief Gamepad axis used for X movement contribution. */
        SnAPI::Input::EGamepadAxis MoveGamepadYAxis = SnAPI::Input::EGamepadAxis::LeftY; /**< @brief Gamepad axis used for Z movement contribution. */
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

    /**
     * @brief Per-frame input sampling and movement/jump dispatch.
     * @param DeltaSeconds Time since last variable tick.
     * @remarks `DeltaSeconds` is currently unused; this method is edge/state driven.
     */
    void Tick(float DeltaSeconds) override;

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
