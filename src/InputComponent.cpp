#include "InputComponent.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)

#include <algorithm>
#include <cmath>
#include <optional>

#include "BaseNode.h"
#include "CharacterMovementController.h"
#include "IWorld.h"
#include "InputSystem.h"
#include "LocalPlayer.h"
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

struct LocalPlayerInputRouting
{
    const LocalPlayer* Controller = nullptr;
    bool HasAnyLocalPlayers = false;
};

LocalPlayerInputRouting ResolveLocalPlayerRouting(const BaseNode& ControlledNode)
{
    LocalPlayerInputRouting Routing{};
    auto* Graph = ControlledNode.OwnerGraph();
    if (!Graph)
    {
        return Routing;
    }

    const NodeHandle ControlledHandle = ControlledNode.Handle();
    Graph->NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        auto* Player = dynamic_cast<LocalPlayer*>(&Node);
        if (!Player)
        {
            return;
        }

        Routing.HasAnyLocalPlayers = true;

        // Local input assignment is runtime-local state and is represented by owner connection id 0.
        if (Player->GetOwnerConnectionId() != 0)
        {
            return;
        }
        if (Player->GetPossessedNode() != ControlledHandle)
        {
            return;
        }
        if (!Routing.Controller)
        {
            Routing.Controller = Player;
        }
    });

    return Routing;
}

bool IsGamepadConnected(const SnAPI::Input::InputSnapshot& Snapshot, const SnAPI::Input::DeviceId Device)
{
    if (!Device.IsValid())
    {
        return false;
    }

    const auto& Gamepads = Snapshot.Gamepads();
    const auto It = Gamepads.find(Device);
    return It != Gamepads.end() && It->second.Connected;
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

    bool KeyboardEnabled = m_settings.KeyboardEnabled;
    bool GamepadEnabled = m_settings.GamepadEnabled;
    std::optional<SnAPI::Input::DeviceId> ForcedGamepad{};

    const LocalPlayerInputRouting LocalRouting = ResolveLocalPlayerRouting(*Owner);
    if (LocalRouting.Controller)
    {
        if (!LocalRouting.Controller->GetAcceptInput())
        {
            if (m_settings.ClearMoveWhenUnavailable)
            {
                Movement.SetMoveInput(Vec3::Zero());
            }
            return;
        }

        if (LocalRouting.Controller->GetUseAssignedInputDevice())
        {
            // Assigned-device mode isolates player control to the owning gamepad only.
            KeyboardEnabled = false;
            GamepadEnabled = true;
            ForcedGamepad = LocalRouting.Controller->GetAssignedInputDevice();
        }
        else if (LocalRouting.Controller->GetPlayerIndex() != 0)
        {
            // Keyboard remains reserved for primary local player by default.
            KeyboardEnabled = false;
        }
    }
    else if (LocalRouting.HasAnyLocalPlayers)
    {
        // When local-player possession is active globally, unpossessed actors must not consume global input.
        if (m_settings.ClearMoveWhenUnavailable)
        {
            Movement.SetMoveInput(Vec3::Zero());
        }
        return;
    }

    float MoveX = 0.0f;
    float MoveZ = 0.0f;

    if (KeyboardEnabled)
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

    SnAPI::Input::DeviceId Gamepad{};
    if (GamepadEnabled)
    {
        if (ForcedGamepad.has_value())
        {
            if (IsGamepadConnected(*Snapshot, *ForcedGamepad))
            {
                Gamepad = *ForcedGamepad;
            }
        }
        else
        {
            Gamepad = ResolveGamepadDevice(*Snapshot);
        }
    }

    if (GamepadEnabled && Gamepad.IsValid())
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
    if (KeyboardEnabled)
    {
        JumpTriggered = Snapshot->KeyPressed(m_settings.JumpKey);
    }
    if (!JumpTriggered && GamepadEnabled && Gamepad.IsValid())
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
