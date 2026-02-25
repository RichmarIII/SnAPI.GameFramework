#include "CharacterMovementController.h"

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "BaseNode.h"
#include "ColliderComponent.h"
#include "IWorld.h"
#include "Level.h"
#include "PhysicsSystem.h"
#include "RigidBodyComponent.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
using Scalar = SnAPI::Math::Scalar;
constexpr float kJumpBufferSeconds = 0.12f;
constexpr float kGroundCoyoteSeconds = 0.08f;

Scalar LengthSquared(const Vec3& Value)
{
    return Value.squaredNorm();
}

Vec3 NormalizeOrZero(const Vec3& Value)
{
    const Scalar Len2 = LengthSquared(Value);
    if (Len2 <= Scalar(1.0e-6))
    {
        return Vec3{};
    }

    const Scalar InvLen = Scalar(1) / std::sqrt(Len2);
    return Value * InvLen;
}

float GroundProbeHalfHeight(BaseNode* Owner)
{
    if (!Owner)
    {
        return 0.0f;
    }

    auto ColliderResult = Owner->Component<ColliderComponent>();
    if (!ColliderResult)
    {
        return 0.5f;
    }

    const auto& Settings = ColliderResult->GetSettings();
    const float LocalYOffset = static_cast<float>(std::abs(Settings.LocalPosition.y()));
    switch (Settings.Shape)
    {
    case SnAPI::Physics::EShapeType::Sphere:
        return std::max(0.0f, Settings.Radius) + LocalYOffset;
    case SnAPI::Physics::EShapeType::Capsule:
        return std::max(0.0f, Settings.HalfHeight + Settings.Radius) + LocalYOffset;
    case SnAPI::Physics::EShapeType::Box:
    default:
        return std::max(0.0f, static_cast<float>(Settings.HalfExtent.y())) + LocalYOffset;
    }
}

} // namespace

void CharacterMovementController::FixedTick(float DeltaSeconds)
{
    RuntimeFixedTick(DeltaSeconds);
}

void CharacterMovementController::RuntimeFixedTick(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f)
    {
        return;
    }

    BaseNode* Owner = nullptr;
    {
        Owner = OwnerNode();
    }
    if (!Owner)
    {
        m_jumpRequested = false;
        m_jumpBufferSecondsRemaining = 0.0f;
        m_groundCoyoteSecondsRemaining = 0.0f;
        m_hasLastPosition = false;
        return;
    }

    auto RigidResult = [&]() {
        return Owner->Component<RigidBodyComponent>();
    }();
    if (!RigidResult)
    {
        m_jumpRequested = false;
        m_jumpBufferSecondsRemaining = 0.0f;
        m_groundCoyoteSecondsRemaining = 0.0f;
        m_hasLastPosition = false;
        return;
    }

    auto& RigidBody = *RigidResult;
    if (!RigidBody.HasBody() && !RigidBody.CreateBody())
    {
        m_jumpRequested = false;
        m_jumpBufferSecondsRemaining = 0.0f;
        m_groundCoyoteSecondsRemaining = 0.0f;
        m_hasLastPosition = false;
        return;
    }

    if (m_jumpRequested)
    {
        m_jumpBufferSecondsRemaining = kJumpBufferSeconds;
        m_jumpRequested = false;
    }

    float VerticalVelocity = 0.0f;
    bool HasPositionSample = false;
    Vec3 PositionSample = Vec3::Zero();
    {
        auto* WorldPtr = Owner->World();
        auto* Physics = WorldPtr ? &WorldPtr->Physics() : nullptr;
        auto* Scene = Physics ? Physics->Scene() : nullptr;
        if (Scene && RigidBody.HasBody())
        {
            auto BodyTransformResult = Scene->Rigid().BodyTransform(RigidBody.PhysicsBodyHandle());
            if (BodyTransformResult)
            {
                const SnAPI::Physics::Vec3 BodyPhysicsPosition = SnAPI::Physics::TransformPosition(*BodyTransformResult);
                PositionSample = Physics ? Physics->PhysicsToWorldPosition(BodyPhysicsPosition) : BodyPhysicsPosition;
                HasPositionSample = true;
            }
        }

        if (!HasPositionSample)
        {
            NodeTransform WorldTransform{};
            if (TransformComponent::TryGetNodeWorldTransform(*Owner, WorldTransform))
            {
                PositionSample = WorldTransform.Position;
                HasPositionSample = true;
            }
        }

        if (HasPositionSample)
        {
            if (m_hasLastPosition)
            {
                VerticalVelocity = (PositionSample.y() - m_lastPosition.y()) / DeltaSeconds;
            }
            m_lastPosition = PositionSample;
            m_hasLastPosition = true;
        }
        else
        {
            m_hasLastPosition = false;
        }
    }

    {
        m_grounded = RefreshGroundedState();
        if (m_grounded)
        {
            m_groundCoyoteSecondsRemaining = kGroundCoyoteSeconds;
        }
        else if (m_groundCoyoteSecondsRemaining > 0.0f)
        {
            m_groundCoyoteSecondsRemaining = std::max(0.0f, m_groundCoyoteSecondsRemaining - DeltaSeconds);
        }
    }

    {
        Vec3 Move = m_moveInput;
        Move.y() = 0.0f;
        const Vec3 Direction = NormalizeOrZero(Move);
        constexpr float kMoveSpeedScale = 0.1f;
        Vec3 TargetVelocity = Direction * (m_settings.MoveForce * kMoveSpeedScale);
        TargetVelocity.y() = VerticalVelocity;
        (void)RigidBody.SetVelocity(TargetVelocity);
    }

    if (m_jumpBufferSecondsRemaining > 0.0f && (m_grounded || m_groundCoyoteSecondsRemaining > 0.0f))
    {
        (void)RigidBody.ApplyForce(Vec3{0.0f, m_settings.JumpImpulse, 0.0f}, SnAPI::Physics::EForceMode::VelocityChange);
        m_jumpBufferSecondsRemaining = 0.0f;
        m_groundCoyoteSecondsRemaining = 0.0f;
        m_grounded = false;
    }

    if (m_jumpBufferSecondsRemaining > 0.0f)
    {
        m_jumpBufferSecondsRemaining = std::max(0.0f, m_jumpBufferSecondsRemaining - DeltaSeconds);
    }
    if (m_settings.ConsumeInputEachTick)
    {
        m_moveInput = Vec3::Zero();
    }
}

void CharacterMovementController::SetMoveInput(const Vec3& Input)
{
    m_moveInput = Input;
}

void CharacterMovementController::AddMoveInput(const Vec3& Input)
{
    m_moveInput += Input;
}

void CharacterMovementController::Jump()
{
    m_jumpRequested = true;
}

bool CharacterMovementController::RefreshGroundedState()
{
    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return false;
    }

    NodeTransform OwnerWorldTransform{};
    if (!TransformComponent::TryGetNodeWorldTransform(*Owner, OwnerWorldTransform))
    {
        return false;
    }

    auto* WorldPtr = Owner->World();
    auto* Physics = WorldPtr ? &WorldPtr->Physics() : nullptr;
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Scene)
    {
        return false;
    }

    SnAPI::Physics::CollisionMask Mask = static_cast<SnAPI::Physics::CollisionMask>(m_settings.GroundMask.Value());
    auto ColliderResult = Owner->Component<ColliderComponent>();
    const float HalfHeight = GroundProbeHalfHeight(Owner);
    if (ColliderResult)
    {
        const std::uint32_t Layer = CollisionLayerToIndex(ColliderResult->GetSettings().Layer);
        Mask &= ~(1u << Layer);
    }

    SnAPI::Physics::RaycastRequest Request{};
    const float StartOffset = std::max(0.0f, m_settings.GroundProbeStartOffset);
    const float ProbeDepth = std::max(0.0f, m_settings.GroundProbeDistance);
    const SnAPI::Physics::Vec3 WorldOrigin = OwnerWorldTransform.Position + Vec3{0.0f, HalfHeight + StartOffset, 0.0f};
    Request.Origin = Physics ? Physics->WorldToPhysicsPosition(WorldOrigin, false) : WorldOrigin;
    Request.Direction = SnAPI::Physics::Vec3{0.0f, -1.0f, 0.0f};
    Request.Distance = (HalfHeight * 2.0f) + StartOffset + ProbeDepth;
    Request.Mask = Mask;
    Request.Mode = SnAPI::Physics::EQueryMode::AllHits;

    SnAPI::Physics::BodyHandle SelfBody{};
    if (auto RigidBodyResult = Owner->Component<RigidBodyComponent>(); RigidBodyResult && RigidBodyResult->HasBody())
    {
        SelfBody = RigidBodyResult->PhysicsBodyHandle();
    }

    std::array<SnAPI::Physics::RaycastHit, 8> Hits{};
    const std::uint32_t HitCount = Scene->Query().Raycast(Request, std::span<SnAPI::Physics::RaycastHit>(Hits));
    for (std::uint32_t Index = 0; Index < HitCount && Index < Hits.size(); ++Index)
    {
        if (SelfBody.IsValid() && Hits[Index].Body == SelfBody)
        {
            continue;
        }
        return true;
    }
    return false;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
