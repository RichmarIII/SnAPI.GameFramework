#include "InputComponent.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "BaseNode.h"
#include "BaseNode.inl"
#include "IWorld.h"
#include "InputIntentComponent.h"
#include "InputSystem.h"
#include "LocalPlayer.h"
#include "NodeCast.h"
#include "TransformComponent.h"
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif

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

std::optional<std::uint64_t> ResolveLocalOwnerConnectionId(const IWorld& WorldRef)
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    const auto& Network = WorldRef.Networking();
    if (Network.IsClient() && !Network.IsListenServer())
    {
        const auto Primary = Network.PrimaryConnection();
        if (Primary.has_value())
        {
            return static_cast<std::uint64_t>(*Primary);
        }
        return std::nullopt;
    }
#endif

    return std::uint64_t{0};
}

LocalPlayerInputRouting ResolveLocalPlayerRouting(const BaseNode& ControlledNode)
{
    LocalPlayerInputRouting Routing{};
    auto* WorldRef = ControlledNode.World();
    if (!WorldRef)
    {
        return Routing;
    }

    const auto LocalOwnerConnectionId = ResolveLocalOwnerConnectionId(*WorldRef);
    if (!LocalOwnerConnectionId.has_value())
    {
        return Routing;
    }

    const NodeHandle ControlledHandle = ControlledNode.Handle();
    struct VisitContext
    {
        LocalPlayerInputRouting* Routing = nullptr;
        NodeHandle Controlled{};
        std::uint64_t OwnerConnectionId = 0;
    };
    VisitContext Context{};
    Context.Routing = &Routing;
    Context.Controlled = ControlledHandle;
    Context.OwnerConnectionId = *LocalOwnerConnectionId;
    WorldRef->ForEachNode(
        [](void* UserData, const NodeHandle&, BaseNode& Node) {
            auto* Context = static_cast<VisitContext*>(UserData);
            auto* Player = NodeCast<LocalPlayer>(&Node);
            if (!Player)
            {
                return;
            }

            if (Player->GetOwnerConnectionId() != Context->OwnerConnectionId)
            {
                return;
            }

            Context->Routing->HasAnyLocalPlayers = true;
            if (Player->GetPossessedNode() != Context->Controlled)
            {
                return;
            }
            if (!Context->Routing->Controller)
            {
                Context->Routing->Controller = Player;
            }
        },
        &Context);

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

float ReadGamepadAxis(const SnAPI::Input::InputSnapshot& Snapshot,
                      const SnAPI::Input::DeviceId Device,
                      const SnAPI::Input::EGamepadAxis Axis)
{
    return Snapshot.GamepadAxis(Device, Axis);
}

bool ReadGamepadButtonPressed(const SnAPI::Input::InputSnapshot& Snapshot,
                              const SnAPI::Input::DeviceId Device,
                              const SnAPI::Input::EGamepadButton Button)
{
    return Snapshot.GamepadButtonPressed(Device, Button);
}

Vec3 FlattenAndNormalizeOrFallback(const Vec3& Direction, const Vec3& Fallback)
{
    Vec3 Flattened = Direction;
    Flattened.y() = 0.0f;

    const Vec3 Normalized = NormalizeOrZero(Flattened);
    if (Normalized.squaredNorm() <= static_cast<Vec3::Scalar>(1.0e-6))
    {
        return Fallback;
    }

    return Normalized;
}

void ResolveMoveBasis(BaseNode& Owner, Vec3& OutRight, Vec3& OutForward)
{
    OutRight = Vec3(1.0f, 0.0f, 0.0f);
    OutForward = Vec3(0.0f, 0.0f, -1.0f);

    NodeTransform OwnerWorld{};
    if (!TransformComponent::TryGetNodeWorldTransform(Owner, OwnerWorld))
    {
        return;
    }

    Quat Rotation = OwnerWorld.Rotation;
    if (Rotation.squaredNorm() > static_cast<Quat::Scalar>(0))
    {
        Rotation.normalize();
    }
    else
    {
        Rotation = Quat::Identity();
    }

    OutRight = FlattenAndNormalizeOrFallback(
        Rotation * Vec3(1.0f, 0.0f, 0.0f),
        OutRight);
    OutForward = FlattenAndNormalizeOrFallback(
        Rotation * Vec3(0.0f, 0.0f, -1.0f),
        OutForward);
}
} // namespace

void InputComponent::OnCreate()
{
    RuntimeOnCreate();
}

void InputComponent::RuntimeOnCreate()
{
    auto* Owner = OwnerNode();
    if (!Owner || Owner->Has<InputIntentComponent>())
    {
        return;
    }

    (void)Owner->Add<InputIntentComponent>();
}

void InputComponent::Tick(float DeltaSeconds)
{
    RuntimeTick(DeltaSeconds);
}

void InputComponent::RuntimeTick(float DeltaSeconds)
{
    const float ClampedDeltaSeconds = std::max(0.0f, DeltaSeconds);

    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    auto IntentResult = Owner->Component<InputIntentComponent>();
    if (!IntentResult)
    {
        return;
    }
    auto& Intent = *IntentResult;

    const auto ClearMoveIntent = [&]() {
        if (m_settings.ClearMoveWhenUnavailable)
        {
            Intent.SetMoveWorldInput(Vec3::Zero());
        }
    };
    const auto ClearLookIntent = [&]() {
        Intent.SetLookInput(0.0f, 0.0f);
    };

    auto* WorldPtr = Owner->World();
    if (!WorldPtr)
    {
        ClearMoveIntent();
        ClearLookIntent();
        return;
    }

    auto& InputSystemRef = WorldPtr->Input();
    if (!InputSystemRef.IsInitialized())
    {
        ClearMoveIntent();
        ClearLookIntent();
        return;
    }

    const auto* Snapshot = InputSystemRef.Snapshot();
    if (!Snapshot)
    {
        ClearMoveIntent();
        ClearLookIntent();
        return;
    }

    if (m_settings.RequireInputFocus && !Snapshot->IsWindowFocused())
    {
        ClearMoveIntent();
        ClearLookIntent();
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
            ClearMoveIntent();
            ClearLookIntent();
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
        ClearMoveIntent();
        ClearLookIntent();
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
        const float AxisX =
            ApplyDeadzone(ReadGamepadAxis(*Snapshot, Gamepad, m_settings.MoveGamepadXAxis));
        float AxisY = ApplyDeadzone(ReadGamepadAxis(*Snapshot, Gamepad, m_settings.MoveGamepadYAxis));
        if (m_settings.InvertGamepadY)
        {
            AxisY = -AxisY;
        }

        MoveX += AxisX;
        MoveZ += AxisY;
    }

    Vec3 MoveInputLocal = Vec3(MoveX, 0.0f, MoveZ);
    if (m_settings.NormalizeMove)
    {
        MoveInputLocal = NormalizeOrZero(MoveInputLocal);
    }

    Vec3 MoveBasisRight = Vec3(1.0f, 0.0f, 0.0f);
    Vec3 MoveBasisForward = Vec3(0.0f, 0.0f, -1.0f);
    ResolveMoveBasis(*Owner, MoveBasisRight, MoveBasisForward);

    // Input local space uses -Z as forward; map into world space using the resolved view basis.
    Vec3 MoveInput = (MoveBasisRight * MoveInputLocal.x()) - (MoveBasisForward * MoveInputLocal.z());
    MoveInput *= static_cast<Vec3::Scalar>(m_settings.MoveScale);

    if (m_settings.MovementEnabled)
    {
        Intent.SetMoveWorldInput(MoveInput);
    }
    else if (m_settings.ClearMoveWhenUnavailable)
    {
        Intent.SetMoveWorldInput(Vec3::Zero());
    }

    if (m_settings.JumpEnabled)
    {
        bool JumpTriggered = false;
        if (KeyboardEnabled)
        {
            JumpTriggered = Snapshot->KeyPressed(m_settings.JumpKey);
        }
        if (!JumpTriggered && GamepadEnabled && Gamepad.IsValid())
        {
            JumpTriggered = ReadGamepadButtonPressed(*Snapshot, Gamepad, m_settings.JumpGamepadButton);
        }

        if (JumpTriggered)
        {
            Intent.QueueJump();
        }
    }

    float LookYawDelta = 0.0f;
    float LookPitchDelta = 0.0f;

    if (m_settings.LookEnabled)
    {
        if (m_settings.MouseLookEnabled)
        {
            const bool MouseLookAllowed = !m_settings.RequireRightMouseButtonForLook
                                       || Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Right);
            if (MouseLookAllowed)
            {
                const float MouseDeltaX = Snapshot->Mouse().DeltaX;
                const float MouseDeltaY = Snapshot->Mouse().DeltaY;
                if (std::isfinite(MouseDeltaX) && std::isfinite(MouseDeltaY))
                {
                    const float Sensitivity = std::max(0.0f, m_settings.MouseLookSensitivity);
                    // Positive screen-space X motion should yaw the camera to the right.
                    LookYawDelta -= MouseDeltaX * Sensitivity;
                    const float PitchSign = m_settings.InvertMouseY ? 1.0f : -1.0f;
                    LookPitchDelta += MouseDeltaY * Sensitivity * PitchSign;
                }
            }
        }

        if (m_settings.GamepadLookEnabled && GamepadEnabled && Gamepad.IsValid())
        {
            const float AxisX =
                ApplyDeadzone(ReadGamepadAxis(*Snapshot, Gamepad, m_settings.LookGamepadXAxis));
            float AxisY = ApplyDeadzone(ReadGamepadAxis(*Snapshot, Gamepad, m_settings.LookGamepadYAxis));
            if (m_settings.InvertGamepadLookY)
            {
                AxisY = -AxisY;
            }

            const float DegreesPerSecond = std::max(0.0f, m_settings.GamepadLookSensitivity);
            LookYawDelta += AxisX * DegreesPerSecond * ClampedDeltaSeconds;
            LookPitchDelta += AxisY * DegreesPerSecond * ClampedDeltaSeconds;
        }
    }

    Intent.SetLookInput(LookYawDelta, LookPitchDelta);
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
