#include "Editor/EditorCameraComponent.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)

#include "BaseNode.h"
#include "CameraComponent.h"
#include "IWorld.h"
#include "InputSystem.h"
#include "TransformComponent.h"

#include <IGraphicsAPI.hpp>
#include <Input.h>
#include <SnAPI/Math/LinearAlgebra.h>

#include <algorithm>
#include <cmath>

#include "CameraBase.hpp"
#include "RendererSystem.h"

namespace SnAPI::GameFramework
{
namespace
{
constexpr float kMinPitchDegrees = -89.0f;
constexpr float kMaxPitchDegrees = 89.0f;
constexpr float kSmallNumber = 1.0e-6f;

[[nodiscard]] bool IsFiniteScalar(const float Value)
{
    return std::isfinite(Value);
}

[[nodiscard]] bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}

[[nodiscard]] bool IsFiniteQuat(const Quat& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z()) && std::isfinite(Value.w());
}

[[nodiscard]] Quat NormalizeQuatOrIdentity(const Quat& Rotation)
{
    Quat Out = Rotation;
    if (Out.squaredNorm() > static_cast<Quat::Scalar>(0))
    {
        Out.normalize();
    }
    else
    {
        Out = Quat::Identity();
    }
    return Out;
}

[[nodiscard]] float WrapDegrees(float Degrees)
{
    if (!IsFiniteScalar(Degrees))
    {
        return 0.0f;
    }

    while (Degrees > 180.0f)
    {
        Degrees -= 360.0f;
    }
    while (Degrees < -180.0f)
    {
        Degrees += 360.0f;
    }
    return Degrees;
}

[[nodiscard]] Vec3 NormalizeOrZero(const Vec3& Value)
{
    const auto LengthSquared = Value.squaredNorm();
    if (LengthSquared <= static_cast<Vec3::Scalar>(kSmallNumber))
    {
        return Vec3::Zero();
    }
    return Value / std::sqrt(LengthSquared);
}

[[nodiscard]] bool IsPointInsideViewportRect(const float X, const float Y, const SnAPI::Graphics::ViewportFit& Rect)
{
    if (!std::isfinite(Rect.X) || !std::isfinite(Rect.Y) || !std::isfinite(Rect.Width) || !std::isfinite(Rect.Height))
    {
        return false;
    }

    if (Rect.Width <= 0.0f || Rect.Height <= 0.0f)
    {
        return false;
    }

    return X >= Rect.X && X < (Rect.X + Rect.Width) && Y >= Rect.Y && Y < (Rect.Y + Rect.Height);
}

[[nodiscard]] bool IsPointerInsideCameraViewport(const IWorld& WorldRef,
                                                 const SnAPI::Input::InputSnapshot& Snapshot,
                                                 const SnAPI::Graphics::ICamera* Camera)
{
    if (!Camera)
    {
        return true;
    }

    const float MouseX = Snapshot.Mouse().X;
    const float MouseY = Snapshot.Mouse().Y;
    if (!std::isfinite(MouseX) || !std::isfinite(MouseY))
    {
        return false;
    }

    const auto& Renderer = WorldRef.Renderer();
    if (!Renderer.IsInitialized())
    {
        return true;
    }

    const SnAPI::Graphics::IGraphicsAPI* Graphics = SnAPI::Graphics::IGraphicsAPI::Instance();
    if (!Graphics)
    {
        return true;
    }

    bool HasMatchedViewport = false;

    const auto ViewportIds = Graphics->RenderViewportIDs();
    for (const auto ViewportId : ViewportIds)
    {
        const auto Config = Graphics->GetRenderViewportConfig(ViewportId);
        if (!Config.has_value() || !Config->Enabled)
        {
            continue;
        }

        // Editor camera navigation must be scoped to explicitly camera-bound viewports only.
        // Root/fullscreen utility viewports often keep null camera and should not grant control.
        if (Config->pCamera != Camera)
        {
            continue;
        }

        HasMatchedViewport = true;
        if (IsPointInsideViewportRect(MouseX, MouseY, Config->OutputRect))
        {
            return true;
        }
    }

    // If this camera is not bound to any viewport, do not hard-block navigation.
    return !HasMatchedViewport;
}
} // namespace

void EditorCameraComponent::Tick(const float DeltaSeconds)
{
    if (!m_settings.Enabled)
    {
        return;
    }

    BaseNode* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    auto TransformResult = Owner->Component<TransformComponent>();
    if (!TransformResult)
    {
        return;
    }

    auto* WorldPtr = Owner->World();
    if (!WorldPtr)
    {
        return;
    }

    auto& InputRef = WorldPtr->Input();
    if (!InputRef.IsInitialized())
    {
        return;
    }

    const auto* Snapshot = InputRef.Snapshot();
    if (!Snapshot)
    {
        return;
    }

    if (m_settings.RequireInputFocus && !Snapshot->IsWindowFocused())
    {
        m_navigationActive = false;
        m_hasLastMousePosition = false;
        return;
    }

    auto& Transform = *TransformResult;
    if (!IsFiniteVec3(Transform.Position))
    {
        Transform.Position = Vec3::Zero();
    }
    if (!IsFiniteQuat(Transform.Rotation))
    {
        Transform.Rotation = Quat::Identity();
    }

    const auto OwnerCameraResult = Owner->Component<CameraComponent>();
    const SnAPI::Graphics::ICamera* OwnerCamera = OwnerCameraResult ? OwnerCameraResult->Camera() : nullptr;

    const bool PointerInsideViewport = !m_settings.RequirePointerInsideViewport
                                    || IsPointerInsideCameraViewport(*WorldPtr, *Snapshot, OwnerCamera);

    const bool NavigationEnabled = PointerInsideViewport &&
                                (!m_settings.RequireRightMouseButton
                                 || Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Right));

    if (!m_orientationInitialized)
    {
        SynchronizeOrientationFromRotation(Transform.Rotation);
        m_orientationInitialized = true;
    }

    if (!NavigationEnabled)
    {
        m_navigationActive = false;
        m_hasLastMousePosition = false;
    }
    else
    {
        if (!m_navigationActive)
        {
            // Re-sync once when navigation activates so external transform edits do not cause first-frame drift.
            SynchronizeOrientationFromRotation(Transform.Rotation);
            m_navigationActive = true;
            m_hasLastMousePosition = false;
        }

        const float Sensitivity = std::max(0.0f, m_settings.LookSensitivity);
        const float MouseX = Snapshot->Mouse().X;
        const float MouseY = Snapshot->Mouse().Y;
        if (IsFiniteScalar(MouseX) && IsFiniteScalar(MouseY))
        {
            if (!m_navigationActive || !m_hasLastMousePosition)
            {
                // Prime mouse baseline on RMB activation to avoid synthetic click-frame deltas.
                m_lastMouseX = MouseX;
                m_lastMouseY = MouseY;
                m_hasLastMousePosition = true;
            }
            else
            {
                const float DeltaX = MouseX - m_lastMouseX;
                const float DeltaY = MouseY - m_lastMouseY;
                m_lastMouseX = MouseX;
                m_lastMouseY = MouseY;

                if (IsFiniteScalar(DeltaX) && IsFiniteScalar(DeltaY))
                {
                    // Positive screen-space X drag should yaw camera to the right.
                    m_yawDegrees = WrapDegrees(m_yawDegrees - (DeltaX * Sensitivity));
                    const float PitchSign = m_settings.InvertY ? 1.0f : -1.0f;
                    m_pitchDegrees = std::clamp(
                        m_pitchDegrees + (DeltaY * Sensitivity * PitchSign), kMinPitchDegrees, kMaxPitchDegrees);
                    Transform.Rotation = ComposeRotation();
                }
            }
        }
    }

    if (!NavigationEnabled)
    {
        return;
    }

    const float ForwardInput = (Snapshot->KeyDown(SnAPI::Input::EKey::W) ? 1.0f : 0.0f)
                             - (Snapshot->KeyDown(SnAPI::Input::EKey::S) ? 1.0f : 0.0f);
    const float RightInput = (Snapshot->KeyDown(SnAPI::Input::EKey::D) ? 1.0f : 0.0f)
                           - (Snapshot->KeyDown(SnAPI::Input::EKey::A) ? 1.0f : 0.0f);
    const float UpInput = (Snapshot->KeyDown(SnAPI::Input::EKey::E) ? 1.0f : 0.0f)
                        - (Snapshot->KeyDown(SnAPI::Input::EKey::Q) ? 1.0f : 0.0f);

    if (!IsFiniteScalar(ForwardInput) || !IsFiniteScalar(RightInput) || !IsFiniteScalar(UpInput))
    {
        return;
    }

    Vec3 Forward = NormalizeOrZero(Transform.Rotation * Vec3(0.0f, 0.0f, -1.0f));
    Vec3 Right = NormalizeOrZero(Transform.Rotation * Vec3(1.0f, 0.0f, 0.0f));
    const Vec3 WorldUp = Vec3(0.0f, 1.0f, 0.0f);

    Vec3 MoveDirection = (Forward * static_cast<Vec3::Scalar>(ForwardInput))
                       + (Right * static_cast<Vec3::Scalar>(RightInput))
                       + (WorldUp * static_cast<Vec3::Scalar>(UpInput));
    MoveDirection = NormalizeOrZero(MoveDirection);
    if (MoveDirection.squaredNorm() <= static_cast<Vec3::Scalar>(kSmallNumber))
    {
        return;
    }

    float Speed = std::max(0.0f, m_settings.MoveSpeed);
    const bool FastMove = Snapshot->KeyDown(SnAPI::Input::EKey::LeftShift)
                       || Snapshot->KeyDown(SnAPI::Input::EKey::RightShift);
    if (FastMove)
    {
        Speed *= std::max(0.0f, m_settings.FastMoveMultiplier);
    }

    const float ClampedDeltaSeconds = std::max(0.0f, IsFiniteScalar(DeltaSeconds) ? DeltaSeconds : 0.0f);
    Transform.Position += MoveDirection * static_cast<Vec3::Scalar>(Speed * ClampedDeltaSeconds);
}

void EditorCameraComponent::SynchronizeOrientationFromRotation(const Quat& Rotation)
{
    const Quat Normalized = NormalizeQuatOrIdentity(Rotation);
    Vec3 Forward = NormalizeOrZero(Normalized * Vec3(0.0f, 0.0f, -1.0f));
    if (!IsFiniteVec3(Forward))
    {
        m_yawDegrees = 0.0f;
        m_pitchDegrees = 0.0f;
        return;
    }

    const float ClampedY = std::clamp(static_cast<float>(Forward.y()), -1.0f, 1.0f);
    const float PitchRadians = std::asin(ClampedY);
    const float YawRadians = std::atan2(static_cast<float>(-Forward.x()), static_cast<float>(-Forward.z()));

    m_pitchDegrees = std::clamp(
        static_cast<float>(SnAPI::Math::SLinearAlgebra::RadiansToDegrees(static_cast<SnAPI::Math::Scalar>(PitchRadians))),
        kMinPitchDegrees,
        kMaxPitchDegrees);
    m_yawDegrees = WrapDegrees(
        static_cast<float>(SnAPI::Math::SLinearAlgebra::RadiansToDegrees(static_cast<SnAPI::Math::Scalar>(YawRadians))));
}

Quat EditorCameraComponent::ComposeRotation() const
{
    const auto YawRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(static_cast<SnAPI::Math::Scalar>(m_yawDegrees));
    const auto PitchRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(static_cast<SnAPI::Math::Scalar>(m_pitchDegrees));

    const Quat YawQuat(SnAPI::Math::AngleAxis3D(YawRadians, SnAPI::Math::Vector3::UnitY()));
    const Quat PitchQuat(SnAPI::Math::AngleAxis3D(PitchRadians, SnAPI::Math::Vector3::UnitX()));
    return NormalizeQuatOrIdentity(YawQuat * PitchQuat);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_RENDERER
