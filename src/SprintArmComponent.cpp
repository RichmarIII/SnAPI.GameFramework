#include "SprintArmComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <SnAPI/Math/LinearAlgebra.h>

#include <algorithm>
#include <cmath>

#include "BaseNode.h"
#include "BaseNode.inl"
#include "CameraComponent.h"
#include "InputIntentComponent.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
constexpr float kSmallNumber = 1.0e-6f;

[[nodiscard]] bool IsFiniteScalar(const float Value)
{
    return std::isfinite(Value);
}

[[nodiscard]] bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
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

[[nodiscard]] float ExtractYawDegrees(const Quat& Rotation)
{
    Vec3 Forward = NormalizeQuatOrIdentity(Rotation) * Vec3(0.0f, 0.0f, -1.0f);
    Forward.y() = 0.0f;
    const auto LengthSquared = Forward.squaredNorm();
    if (LengthSquared <= static_cast<Vec3::Scalar>(kSmallNumber))
    {
        return 0.0f;
    }

    Forward /= std::sqrt(LengthSquared);
    const float YawRadians = std::atan2(static_cast<float>(-Forward.x()), static_cast<float>(-Forward.z()));
    return WrapDegrees(static_cast<float>(
        SnAPI::Math::SLinearAlgebra::RadiansToDegrees(static_cast<SnAPI::Math::Scalar>(YawRadians))));
}

[[nodiscard]] Quat MakeYawRotation(const float YawDegrees)
{
    const SnAPI::Math::Scalar YawRadians =
        SnAPI::Math::SLinearAlgebra::DegreesToRadians(static_cast<SnAPI::Math::Scalar>(YawDegrees));
    return NormalizeQuatOrIdentity(Quat(SnAPI::Math::AngleAxis3D(YawRadians, SnAPI::Math::Vector3::UnitY())));
}

[[nodiscard]] Quat MakePitchRotation(const float PitchDegrees)
{
    const SnAPI::Math::Scalar PitchRadians =
        SnAPI::Math::SLinearAlgebra::DegreesToRadians(static_cast<SnAPI::Math::Scalar>(PitchDegrees));
    return NormalizeQuatOrIdentity(Quat(SnAPI::Math::AngleAxis3D(PitchRadians, SnAPI::Math::Vector3::UnitX())));
}

[[nodiscard]] bool AreNearlySameRotation(const Quat& A, const Quat& B)
{
    const Quat NormalizedA = NormalizeQuatOrIdentity(A);
    const Quat NormalizedB = NormalizeQuatOrIdentity(B);
    const double Dot = std::abs(static_cast<double>(NormalizedA.dot(NormalizedB)));
    return Dot >= 0.9999995;
}
} // namespace

void SprintArmComponent::OnCreate()
{
    RuntimeOnCreate();
}

void SprintArmComponent::Tick(const float DeltaSeconds)
{
    RuntimeTick(DeltaSeconds);
}

void SprintArmComponent::LateTick(const float DeltaSeconds)
{
    RuntimeLateTick(DeltaSeconds);
}

void SprintArmComponent::RuntimeOnCreate()
{
    InitializeFromOwner();
    ApplyArmToOwnerAndCamera();
}

void SprintArmComponent::RuntimeTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}

void SprintArmComponent::RuntimeLateTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;

    PullLookInputIntent();

    if (!m_settings.Enabled)
    {
        m_pendingYawDeltaDegrees = 0.0f;
        m_pendingPitchDeltaDegrees = 0.0f;
        return;
    }

    if (!m_initialized)
    {
        InitializeFromOwner();
    }
    if (!m_initialized)
    {
        return;
    }

    ApplyPendingLookInput();
    ApplyArmToOwnerAndCamera();
}

void SprintArmComponent::AddLookInput(const float YawDeltaDegrees, const float PitchDeltaDegrees)
{
    if (!IsFiniteScalar(YawDeltaDegrees) || !IsFiniteScalar(PitchDeltaDegrees))
    {
        return;
    }

    m_pendingYawDeltaDegrees += YawDeltaDegrees;
    m_pendingPitchDeltaDegrees += PitchDeltaDegrees;
}

void SprintArmComponent::SetViewAngles(const float YawDegreesValue, const float PitchDegreesValue)
{
    if (IsFiniteScalar(YawDegreesValue))
    {
        m_yawDegrees = WrapDegrees(YawDegreesValue);
    }
    if (IsFiniteScalar(PitchDegreesValue))
    {
        const float MinPitch = std::min(m_settings.MinPitchDegrees, m_settings.MaxPitchDegrees);
        const float MaxPitch = std::max(m_settings.MinPitchDegrees, m_settings.MaxPitchDegrees);
        m_pitchDegrees = std::clamp(PitchDegreesValue, MinPitch, MaxPitch);
    }

    m_settings.YawDegrees = m_yawDegrees;
    m_settings.PitchDegrees = m_pitchDegrees;
    m_initialized = true;
}

void SprintArmComponent::InitializeFromOwner()
{
    m_pendingYawDeltaDegrees = 0.0f;
    m_pendingPitchDeltaDegrees = 0.0f;

    m_yawDegrees = WrapDegrees(m_settings.YawDegrees);
    const float MinPitch = std::min(m_settings.MinPitchDegrees, m_settings.MaxPitchDegrees);
    const float MaxPitch = std::max(m_settings.MinPitchDegrees, m_settings.MaxPitchDegrees);
    m_pitchDegrees = std::clamp(m_settings.PitchDegrees, MinPitch, MaxPitch);

    BaseNode* Owner = OwnerNode();
    if (Owner)
    {
        NodeTransform OwnerWorld{};
        if (TransformComponent::TryGetNodeWorldTransform(*Owner, OwnerWorld))
        {
            m_yawDegrees = ExtractYawDegrees(OwnerWorld.Rotation);
        }
    }

    m_settings.YawDegrees = m_yawDegrees;
    m_settings.PitchDegrees = m_pitchDegrees;
    m_initialized = true;
}

void SprintArmComponent::ApplyArmToOwnerAndCamera()
{
    BaseNode* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    const float MinPitch = std::min(m_settings.MinPitchDegrees, m_settings.MaxPitchDegrees);
    const float MaxPitch = std::max(m_settings.MinPitchDegrees, m_settings.MaxPitchDegrees);
    m_yawDegrees = WrapDegrees(m_yawDegrees);
    m_pitchDegrees = std::clamp(m_pitchDegrees, MinPitch, MaxPitch);
    m_settings.YawDegrees = m_yawDegrees;
    m_settings.PitchDegrees = m_pitchDegrees;

    const Quat YawRotation = MakeYawRotation(m_yawDegrees);
    if (m_settings.DriveOwnerYaw)
    {
        NodeTransform OwnerWorld{};
        if (TransformComponent::TryGetNodeWorldTransform(*Owner, OwnerWorld))
        {
            if (!AreNearlySameRotation(OwnerWorld.Rotation, YawRotation))
            {
                (void)TransformComponent::TrySetNodeWorldPose(*Owner, OwnerWorld.Position, YawRotation, true);
            }
        }
        else if (auto TransformResult = Owner->Component<TransformComponent>())
        {
            TransformResult->Rotation = YawRotation;
        }
        else if (auto AddedTransform = Owner->Add<TransformComponent>())
        {
            AddedTransform->Rotation = YawRotation;
        }
    }

    auto CameraResult = Owner->Component<CameraComponent>();
    if (!CameraResult)
    {
        return;
    }

    auto& Camera = *CameraResult;
    auto& CameraSettings = Camera.EditSettings();
    CameraSettings.SyncFromTransform = true;

    const float ArmLength = std::max(0.0f, m_settings.ArmLength);
    const Quat PitchRotation = MakePitchRotation(m_pitchDegrees);
    const Vec3 ArmOffset = PitchRotation * Vec3(0.0f, 0.0f, ArmLength);
    if (IsFiniteVec3(m_settings.SocketOffset))
    {
        CameraSettings.LocalPositionOffset = m_settings.SocketOffset + ArmOffset;
    }
    else
    {
        CameraSettings.LocalPositionOffset = ArmOffset;
    }

    CameraSettings.LocalRotationOffsetEuler =
        Vec3(SnAPI::Math::SLinearAlgebra::DegreesToRadians(static_cast<SnAPI::Math::Scalar>(m_pitchDegrees)),
             0.0f,
             0.0f);
}

void SprintArmComponent::PullLookInputIntent()
{
    BaseNode* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    auto InputIntent = Owner->Component<InputIntentComponent>();
    if (!InputIntent)
    {
        return;
    }

    float YawDeltaDegrees = 0.0f;
    float PitchDeltaDegrees = 0.0f;
    InputIntent->ConsumeLookInput(YawDeltaDegrees, PitchDeltaDegrees);
    if (std::abs(YawDeltaDegrees) <= kSmallNumber && std::abs(PitchDeltaDegrees) <= kSmallNumber)
    {
        return;
    }

    AddLookInput(YawDeltaDegrees, PitchDeltaDegrees);
}

void SprintArmComponent::ApplyPendingLookInput()
{
    if (!IsFiniteScalar(m_pendingYawDeltaDegrees))
    {
        m_pendingYawDeltaDegrees = 0.0f;
    }
    if (!IsFiniteScalar(m_pendingPitchDeltaDegrees))
    {
        m_pendingPitchDeltaDegrees = 0.0f;
    }

    m_yawDegrees = WrapDegrees(m_yawDegrees + m_pendingYawDeltaDegrees);
    m_pitchDegrees += m_pendingPitchDeltaDegrees;
    m_pendingYawDeltaDegrees = 0.0f;
    m_pendingPitchDeltaDegrees = 0.0f;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
