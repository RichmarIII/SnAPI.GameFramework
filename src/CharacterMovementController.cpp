#include "CharacterMovementController.h"

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <cmath>
#include <cstdint>

#include "BaseNode.h"
#include "ColliderComponent.h"
#include "IWorld.h"
#include "NodeGraph.h"
#include "PhysicsSystem.h"
#include "RigidBodyComponent.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
using Scalar = SnAPI::Math::Scalar;

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

SnAPI::Physics::Vec3 ToPhysicsVec3(const Vec3& Value)
{
    return Value;
}

} // namespace

void CharacterMovementController::FixedTick(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f)
    {
        return;
    }

    auto* Owner = OwnerNode();
    if (!Owner)
    {
        m_jumpRequested = false;
        m_hasLastPosition = false;
        return;
    }

    auto RigidResult = Owner->Component<RigidBodyComponent>();
    if (!RigidResult)
    {
        m_jumpRequested = false;
        m_hasLastPosition = false;
        return;
    }

    auto& RigidBody = *RigidResult;
    if (!RigidBody.HasBody() && !RigidBody.CreateBody())
    {
        m_jumpRequested = false;
        m_hasLastPosition = false;
        return;
    }

    float VerticalVelocity = 0.0f;
    if (auto TransformResult = Owner->Component<TransformComponent>())
    {
        if (m_hasLastPosition)
        {
            VerticalVelocity = (TransformResult->Position.y() - m_lastPosition.y()) / DeltaSeconds;
        }
        m_lastPosition = TransformResult->Position;
        m_hasLastPosition = true;
    }

    m_grounded = RefreshGroundedState();

    Vec3 Move = m_moveInput;
    Move.y() = 0.0f;
    const Vec3 Direction = NormalizeOrZero(Move);
    if (LengthSquared(Direction) > Scalar(0))
    {
        constexpr float kMoveSpeedScale = 0.1f;
        Vec3 TargetVelocity = Direction * (m_settings.MoveForce * kMoveSpeedScale);
        TargetVelocity.y() = VerticalVelocity;
        (void)RigidBody.SetVelocity(TargetVelocity);
    }

    if (m_jumpRequested && m_grounded)
    {
        (void)RigidBody.ApplyForce(Vec3{0.0f, m_settings.JumpImpulse, 0.0f}, true);
        m_grounded = false;
    }

    m_jumpRequested = false;
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

    auto TransformResult = Owner->Component<TransformComponent>();
    if (!TransformResult)
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
    if (ColliderResult)
    {
        const std::uint32_t Layer = CollisionLayerToIndex(ColliderResult->GetSettings().Layer);
        Mask &= ~(1u << Layer);
    }

    SnAPI::Physics::RaycastRequest Request{};
    Request.Origin = ToPhysicsVec3(TransformResult->Position + Vec3{0.0f, m_settings.GroundProbeStartOffset, 0.0f});
    Request.Direction = SnAPI::Physics::Vec3{0.0f, -1.0f, 0.0f};
    Request.Distance = m_settings.GroundProbeDistance;
    Request.Mask = Mask;
    Request.Mode = SnAPI::Physics::EQueryMode::ClosestHit;

    SnAPI::Physics::RaycastHit Hit{};
    const std::uint32_t HitCount = Scene->Query().Raycast(Request, std::span<SnAPI::Physics::RaycastHit>(&Hit, 1));
    return HitCount > 0;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
