#pragma once

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)

#include <Input.h>

#include "BaseComponent.h"
#include "Math.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bridges world input snapshots into sibling `InputIntentComponent` state.
 *
 * `InputComponent` is the engine-supplied adapter from `InputSystem` snapshot data to the
 * gameplay-facing transient intent bus represented by `InputIntentComponent`. It samples keyboard,
 * mouse, and gamepad state each frame, converts that state into world-space movement and view
 * deltas, then publishes the results for movement and camera systems to consume.
 *
 * Local-player routing semantics:
 * - If the owning node is possessed by a local `LocalPlayer`, this component respects that player's
 *   `AcceptInput`, player index, and assigned-device policy.
 * - Non-primary local players do not receive keyboard input by default.
 * - Assigned-device mode forces input to the owning player's selected gamepad and disables keyboard input.
 * - If any local players exist and this node is not possessed by one of them, the component suppresses
 *   all input for the node instead of consuming shared global input.
 *
 * Core semantics:
 * - Movement intent is published in world space using the owning node's flattened right/forward basis.
 * - Forward input is interpreted as local `-Z`, then remapped into world space.
 * - Mouse look sensitivity is interpreted as degrees per pixel.
 * - Gamepad look sensitivity is interpreted as degrees per second and multiplied by frame delta time.
 * - Jump uses an edge-triggered latch via `InputIntentComponent::QueueJump()`.
 * - Look input is replaced each tick, not accumulated across frames by this component.
 *
 * Ownership and lifetime:
 * - The component owns only its input-binding settings.
 * - It auto-creates a sibling `InputIntentComponent` on `OnCreate()` when one does not already exist.
 * - It borrows the world's `InputSystem` snapshot for the duration of the tick only.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Performance notes:
 * - Work is constant time per frame.
 * - No heap allocation is performed during normal ticking.
 *
 * @warning This component requires `InputSystem` readiness. When input is unavailable, focus is lost,
 * or local-player routing suppresses this node, movement and look are cleared according to settings and
 * jump edges are not generated.
 *
 * @see InputIntentComponent
 * @see LocalPlayer
 * @see CharacterMovementController
 * @see SprintArmComponent
 */
SnType()
class InputComponent : public BaseComponent, public ComponentCRTP<InputComponent>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::InputComponent";

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Runtime binding, filtering, and shaping configuration for `InputComponent`.
     *
     * Keyboard and gamepad movement contributions are summed in local input space before optional
     * normalization and scaling. Mouse and gamepad look are converted into yaw/pitch deltas in degrees.
     * The settings are passive data: mutating them affects subsequent ticks only.
     */
    SnType()
    struct Settings
    {
        /** @brief Stable reflected type name used for serialization registration. */
        static constexpr const char* kTypeName = "SnAPI::GameFramework::InputComponent::Settings";

        SnField(SnKey("MovementEnabled"))
        bool MovementEnabled = true; /**< @brief Publish movement intent when `true`; otherwise movement is suppressed. */
        SnField(SnKey("JumpEnabled"))
        bool JumpEnabled = true; /**< @brief Publish jump edges when `true`. */
        SnField(SnKey("KeyboardEnabled"))
        bool KeyboardEnabled = true; /**< @brief Allow keyboard movement/jump contribution before local-player routing filters are applied. */
        SnField(SnKey("GamepadEnabled"))
        bool GamepadEnabled = true; /**< @brief Allow gamepad movement/jump/look contribution before local-player routing filters are applied. */
        SnField(SnKey("RequireInputFocus"))
        bool RequireInputFocus = true; /**< @brief Suppress movement and look when the input snapshot reports the window as unfocused. */
        SnField(SnKey("NormalizeMove"))
        bool NormalizeMove = true; /**< @brief Normalize combined local X/Z movement before applying `MoveScale`. */
        SnField(SnKey("ClearMoveWhenUnavailable"))
        bool ClearMoveWhenUnavailable = true; /**< @brief Write zero movement intent when input cannot currently be resolved. */
        SnField(SnKey("LookEnabled"))
        bool LookEnabled = true; /**< @brief Publish look deltas when `true`. */
        SnField(SnKey("MouseLookEnabled"))
        bool MouseLookEnabled = true; /**< @brief Allow mouse delta contribution to look input. */
        SnField(SnKey("GamepadLookEnabled"))
        bool GamepadLookEnabled = true; /**< @brief Allow right-stick contribution to look input. */
        SnField(SnKey("RequireRightMouseButtonForLook"))
        bool RequireRightMouseButtonForLook = false; /**< @brief Require right mouse button to be held before mouse look is sampled. */

        SnField(SnKey("MoveScale"))
        float MoveScale = 1.0f; /**< @brief Scalar multiplier applied after optional movement normalization. */
        SnField(SnKey("GamepadDeadzone"))
        float GamepadDeadzone = 0.2f; /**< @brief Per-axis deadzone in the range `[0, 0.99]`, clamped on use. */
        SnField(SnKey("InvertGamepadY"))
        bool InvertGamepadY = false; /**< @brief Invert the configured movement Y axis before mapping it into local forward/back motion. */
        SnField(SnKey("MouseLookSensitivity"))
        float MouseLookSensitivity = 0.12f; /**< @brief Mouse look sensitivity in degrees per pixel. Negative values are treated as zero. */
        SnField(SnKey("InvertMouseY"))
        bool InvertMouseY = false; /**< @brief Invert vertical mouse look contribution. */
        SnField(SnKey("GamepadLookSensitivity"))
        float GamepadLookSensitivity = 180.0f; /**< @brief Gamepad look sensitivity in degrees per second at full stick deflection. Negative values are treated as zero. */
        SnField(SnKey("InvertGamepadLookY"))
        bool InvertGamepadLookY = false; /**< @brief Invert vertical gamepad look contribution. */

        SnField(SnKey("MoveForwardKey"))
        SnAPI::Input::EKey MoveForwardKey = SnAPI::Input::EKey::W; /**< @brief Keyboard key mapped to forward movement, which corresponds to local `-Z`. */
        SnField(SnKey("MoveBackwardKey"))
        SnAPI::Input::EKey MoveBackwardKey = SnAPI::Input::EKey::S; /**< @brief Keyboard key mapped to backward movement, which corresponds to local `+Z`. */
        SnField(SnKey("MoveLeftKey"))
        SnAPI::Input::EKey MoveLeftKey = SnAPI::Input::EKey::A; /**< @brief Keyboard key mapped to local `-X` motion. */
        SnField(SnKey("MoveRightKey"))
        SnAPI::Input::EKey MoveRightKey = SnAPI::Input::EKey::D; /**< @brief Keyboard key mapped to local `+X` motion. */
        SnField(SnKey("JumpKey"))
        SnAPI::Input::EKey JumpKey = SnAPI::Input::EKey::Space; /**< @brief Keyboard key that generates a jump edge. */

        SnField(SnKey("MoveGamepadXAxis"))
        SnAPI::Input::EGamepadAxis MoveGamepadXAxis = SnAPI::Input::EGamepadAxis::LeftX; /**< @brief Gamepad axis sampled for local X movement. */
        SnField(SnKey("MoveGamepadYAxis"))
        SnAPI::Input::EGamepadAxis MoveGamepadYAxis = SnAPI::Input::EGamepadAxis::LeftY; /**< @brief Gamepad axis sampled for local forward/back movement. */
        SnField(SnKey("LookGamepadXAxis"))
        SnAPI::Input::EGamepadAxis LookGamepadXAxis = SnAPI::Input::EGamepadAxis::RightX; /**< @brief Gamepad axis sampled for yaw look delta. */
        SnField(SnKey("LookGamepadYAxis"))
        SnAPI::Input::EGamepadAxis LookGamepadYAxis = SnAPI::Input::EGamepadAxis::RightY; /**< @brief Gamepad axis sampled for pitch look delta. */
        SnField(SnKey("JumpGamepadButton"))
        SnAPI::Input::EGamepadButton JumpGamepadButton = SnAPI::Input::EGamepadButton::South; /**< @brief Gamepad button that generates a jump edge. */

        SnField(SnKey("PreferredGamepad"))
        SnAPI::Input::DeviceId PreferredGamepad{}; /**< @brief Preferred gamepad device id. An invalid id means "auto-select". */
        SnField(SnKey("UseAnyGamepadWhenPreferredMissing"))
        bool UseAnyGamepadWhenPreferredMissing = true; /**< @brief Fall back to the first connected gamepad when the preferred one is absent. */
    };

    /**
     * @brief Read the current input-binding settings.
     * @return Borrowed reference to the stored settings object.
     */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /**
     * @brief Mutate the current input-binding settings.
     * @return Borrowed reference to the stored settings object.
     * @remarks Changes affect future ticks only; no immediate resampling or rebinding occurs.
     */
    SnField(SnKey("Settings"), SnConstGetter(GetSettings))
    Settings& EditSettings()
    {
        return m_settings;
    }

    /**
     * @brief Ensure the sibling `InputIntentComponent` exists.
     * @post The owning node has an `InputIntentComponent` unless node/component creation failed upstream.
     */
    void OnCreate();
    /**
     * @brief Per-frame input sampling and intent publishing.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     *
     * The component reads the current input snapshot, resolves local-player routing, converts input
     * to world-space movement, and publishes movement/jump/look state into the sibling intent component.
     * `DeltaSeconds` is used only for gamepad look scaling.
     */
    void Tick(float DeltaSeconds);

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
