#include "FollowTargetComponent.h"

#include <algorithm>
#include <cmath>

#include "BaseNode.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
float ExponentialAlpha(const float SmoothingHz, const float DeltaSeconds)
{
    if (SmoothingHz <= 0.0f || DeltaSeconds <= 0.0f)
    {
        return 1.0f;
    }

    const float Raw = 1.0f - std::exp(-SmoothingHz * std::max(DeltaSeconds, 0.0f));
    return std::clamp(Raw, 0.0f, 1.0f);
}

Quat NormalizeQuatOrIdentity(const Quat& Rotation)
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

Quat NlerpShortestPath(const Quat& A, const Quat& B, const float Alpha)
{
    const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);
    Quat End = B;
    if (A.dot(B) < static_cast<Quat::Scalar>(0))
    {
        End.coeffs() = -End.coeffs();
    }

    Quat Out = Quat::Identity();
    Out.coeffs() = (A.coeffs() * static_cast<Quat::Scalar>(1.0f - ClampedAlpha))
                 + (End.coeffs() * static_cast<Quat::Scalar>(ClampedAlpha));
    return NormalizeQuatOrIdentity(Out);
}

} // namespace

void FollowTargetComponent::Tick(const float DeltaSeconds)
{
    (void)ApplyFollow(DeltaSeconds);
}

bool FollowTargetComponent::ApplyFollow(const float DeltaSeconds)
{
    if (m_settings.Target.IsNull())
    {
        return false;
    }

    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return false;
    }

    BaseNode* TargetNode = m_settings.Target.Borrowed();
    if (!TargetNode && m_settings.ResolveTargetByUuidFallback)
    {
        TargetNode = m_settings.Target.BorrowedSlowByUuid();
    }
    if (!TargetNode || TargetNode == Owner)
    {
        return false;
    }

    NodeTransform OwnerWorld{};
    if (!TransformComponent::TryGetNodeWorldTransform(*Owner, OwnerWorld))
    {
        return false;
    }

    NodeTransform TargetWorld{};
    if (!TransformComponent::TryGetNodeWorldTransform(*TargetNode, TargetWorld))
    {
        return false;
    }

    NodeTransform DesiredWorld = OwnerWorld;

    if (m_settings.SyncPosition)
    {
        const Vec3 DesiredPosition = TargetWorld.Position + m_settings.PositionOffset;
        const float PositionAlpha = ExponentialAlpha(m_settings.PositionSmoothingHz, DeltaSeconds);
        const auto PositionBlend = static_cast<Vec3::Scalar>(PositionAlpha);
        DesiredWorld.Position = OwnerWorld.Position + ((DesiredPosition - OwnerWorld.Position) * PositionBlend);
    }

    if (m_settings.SyncRotation)
    {
        const Quat DesiredRotation = NormalizeQuatOrIdentity(TargetWorld.Rotation * m_settings.RotationOffset);
        const float RotationAlpha = ExponentialAlpha(m_settings.RotationSmoothingHz, DeltaSeconds);
        DesiredWorld.Rotation = (RotationAlpha >= 1.0f)
                                    ? DesiredRotation
                                    : NlerpShortestPath(OwnerWorld.Rotation, DesiredRotation, RotationAlpha);
    }

    return TransformComponent::TrySetNodeWorldTransform(*Owner, DesiredWorld, true);
}

} // namespace SnAPI::GameFramework
