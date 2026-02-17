#include "InputComponent.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)

#include <algorithm>
#include <cmath>

#include "BaseNode.h"
#include "CharacterMovementController.h"
#include "IWorld.h"
#include "InputSystem.h"
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{
namespace
{
using Scalar = SnAPI::Math::Scalar;

Vec3 NormalizeOrZero(const Vec3& Value)
{
    const Scalar LengthSquared = Value.squaredNorm();
    if (LengthSquared <= Scalar(1.0e-6))
    {
        return Vec3::Zero();
    }

    const Scalar InvLength = Scalar(1) / std::sqrt(LengthSquared);
    return Value * InvLength;
}
} // namespace

void InputComponent::Tick(float DeltaSeconds)
{
    (void)DeltaSeconds;

    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    auto MovementResult = Owner->Component<CharacterMovementController>();
    if (!MovementResult)
    {
        return;
    }
    auto& Movement = *MovementResult;

    auto* WorldPtr = Owner->World();
    if (!WorldPtr)
    {
        if (m_settings.ClearMoveWhenUnavailable)
        {
            Movement.SetMoveInput(Vec3::Zero());
        }
        return;
    }

    auto& InputSystemRef = WorldPtr->Input();
    if (!InputSystemRef.IsInitialized())
    {
        if (m_settings.ClearMoveWhenUnavailable)
        {
            Movement.SetMoveInput(Vec3::Zero());
        }
        return;
    }

    const auto* Snapshot = InputSystemRef.Snapshot();
    if (!Snapshot)
    {
        if (m_settings.ClearMoveWhenUnavailable)
        {
            Movement.SetMoveInput(Vec3::Zero());
        }
        return;
    }

    if (m_settings.RequireInputFocus && !Snapshot->IsWindowFocused())
    {
        if (m_settings.ClearMoveWhenUnavailable)
        {
            Movement.SetMoveInput(Vec3::Zero());
        }
        return;
    }

    float MoveX = 0.0f;
    float MoveZ = 0.0f;

    if (m_settings.KeyboardEnabled)
    {
        if (Snapshot->KeyDown(m_settings.MoveLeftKey))
        {
            MoveX -= 1.0f;
        }
        if (Snapshot->KeyDown(m_settings.MoveRightKey))
        {
            MoveX += 1.0f;
        }
        if (Snapshot->KeyDown(m_settings.MoveForwardKey))
        {
            MoveZ -= 1.0f;
        }
        if (Snapshot->KeyDown(m_settings.MoveBackwardKey))
        {
            MoveZ += 1.0f;
        }
    }

    const SnAPI::Input::DeviceId Gamepad = m_settings.GamepadEnabled ? ResolveGamepadDevice(*Snapshot)
                                                                      : SnAPI::Input::DeviceId{};

    if (m_settings.GamepadEnabled && Gamepad.IsValid())
    {
        const float AxisX = ApplyDeadzone(Snapshot->GamepadAxis(Gamepad, m_settings.MoveGamepadXAxis));
        float AxisY = ApplyDeadzone(Snapshot->GamepadAxis(Gamepad, m_settings.MoveGamepadYAxis));
        if (m_settings.InvertGamepadY)
        {
            AxisY = -AxisY;
        }

        MoveX += AxisX;
        MoveZ += AxisY;
    }

    auto MoveInput = Vec3(MoveX, 0.0f, MoveZ);
    if (m_settings.NormalizeMove)
    {
        MoveInput = NormalizeOrZero(MoveInput);
    }
    MoveInput *= static_cast<Vec3::Scalar>(m_settings.MoveScale);

    if (m_settings.MovementEnabled)
    {
        Movement.SetMoveInput(MoveInput);
    }
    else if (m_settings.ClearMoveWhenUnavailable)
    {
        Movement.SetMoveInput(Vec3::Zero());
    }

    if (!m_settings.JumpEnabled)
    {
        return;
    }

    bool JumpTriggered = false;
    if (m_settings.KeyboardEnabled)
    {
        JumpTriggered = Snapshot->KeyPressed(m_settings.JumpKey);
    }
    if (!JumpTriggered && m_settings.GamepadEnabled && Gamepad.IsValid())
    {
        JumpTriggered = Snapshot->GamepadButtonPressed(Gamepad, m_settings.JumpGamepadButton);
    }

    if (JumpTriggered)
    {
        Movement.Jump();
    }
}

SnAPI::Input::DeviceId InputComponent::ResolveGamepadDevice(const SnAPI::Input::InputSnapshot& Snapshot) const
{
    const auto& Gamepads = Snapshot.Gamepads();
    if (Gamepads.empty())
    {
        return {};
    }

    if (m_settings.PreferredGamepad.IsValid())
    {
        auto It = Gamepads.find(m_settings.PreferredGamepad);
        if (It != Gamepads.end() && It->second.Connected)
        {
            return It->first;
        }
        if (!m_settings.UseAnyGamepadWhenPreferredMissing)
        {
            return {};
        }
    }

    for (const auto& [Device, State] : Gamepads)
    {
        if (State.Connected)
        {
            return Device;
        }
    }

    return {};
}

float InputComponent::ApplyDeadzone(const float Value) const
{
    const float Deadzone = std::clamp(m_settings.GamepadDeadzone, 0.0f, 0.99f);
    const float Magnitude = std::abs(Value);
    if (Magnitude <= Deadzone)
    {
        return 0.0f;
    }

    const float Normalized = std::clamp((Magnitude - Deadzone) / (1.0f - Deadzone), 0.0f, 1.0f);
    return std::copysign(Normalized, Value);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_PHYSICS
