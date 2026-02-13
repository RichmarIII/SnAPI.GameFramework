#include "RigidBodyComponent.h"

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <SnAPI/Math/LinearAlgebra.h>
#include <cmath>

#include "BaseNode.h"
#include "ColliderComponent.h"
#include "IWorld.h"
#include "NodeGraph.h"
#include "PhysicsSystem.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
SnAPI::Physics::Vec3 ToPhysicsVec3(const Vec3& Value)
{
    return Value;
}

Vec3 FromPhysicsVec3(const SnAPI::Physics::Vec3& Value)
{
    return Value;
}

SnAPI::Physics::Quat EulerToQuat(const Vec3& Euler)
{
    // TransformComponent rotation is rendered as ZYX (Rz * Ry * Rx).
    const SnAPI::Math::Quaternion Rotation = SnAPI::Math::AngleAxis3D(Euler.z(), SnAPI::Math::Vector3::UnitZ())
                                           * SnAPI::Math::AngleAxis3D(Euler.y(), SnAPI::Math::Vector3::UnitY())
                                           * SnAPI::Math::AngleAxis3D(Euler.x(), SnAPI::Math::Vector3::UnitX());
    return SnAPI::Physics::MakeQuatXYZW(Rotation.x(), Rotation.y(), Rotation.z(), Rotation.w());
}

Vec3 QuatToEuler(const SnAPI::Physics::Quat& Q)
{
    // Decompose to ZYX to mirror render/application order exactly.
    const SnAPI::Math::Vector3 Zyx = Q.toRotationMatrix().eulerAngles(2, 1, 0);
    const auto Wrap = [](const float Value) {
        return std::remainder(Value, 6.28318530717958647692f);
    };
    return Vec3{Wrap(Zyx.z()), Wrap(Zyx.y()), Wrap(Zyx.x())};
}

Physics::Transform ReadOwnerTransform(BaseNode* Owner)
{
    Physics::Transform Out = Physics::IdentityTransform();
    if (!Owner)
    {
        return Out;
    }

    auto TransformResult = Owner->Component<TransformComponent>();
    if (!TransformResult)
    {
        return Out;
    }

    Physics::SetTransformPosition(Out, ToPhysicsVec3(TransformResult->Position));
    Physics::SetTransformRotation(Out, EulerToQuat(TransformResult->Rotation));
    return Out;
}

void WriteOwnerTransform(BaseNode* Owner, const Physics::Transform& TransformValue)
{
    if (!Owner)
    {
        return;
    }

    auto TransformResult = Owner->Component<TransformComponent>();
    if (!TransformResult)
    {
        const auto AddedResult = Owner->Add<TransformComponent>();
        if (!AddedResult)
        {
            return;
        }
        TransformResult = AddedResult;
    }

    TransformResult->Position = FromPhysicsVec3(Physics::TransformPosition(TransformValue));
    TransformResult->Rotation = QuatToEuler(Physics::TransformRotation(TransformValue));
}

Physics::ColliderDesc BuildColliderDesc(BaseNode* Owner)
{
    Physics::ColliderDesc Collider{};
    Collider.Shape.Type = Physics::EShapeType::Box;
    Collider.Shape.Box.HalfExtent = Physics::Vec3{0.5f, 0.5f, 0.5f};

    if (!Owner)
    {
        return Collider;
    }

    auto ColliderResult = Owner->Component<ColliderComponent>();
    if (!ColliderResult)
    {
        return Collider;
    }

    const auto& Settings = ColliderResult->GetSettings();
    switch (Settings.Shape)
    {
    case SnAPI::Physics::EShapeType::Sphere:
        Collider.Shape.Type = SnAPI::Physics::EShapeType::Sphere;
        Collider.Shape.Sphere.Radius = Settings.Radius;
        break;
    case SnAPI::Physics::EShapeType::Capsule:
        Collider.Shape.Type = SnAPI::Physics::EShapeType::Capsule;
        Collider.Shape.Capsule.Radius = Settings.Radius;
        Collider.Shape.Capsule.HalfHeight = Settings.HalfHeight;
        break;
    case SnAPI::Physics::EShapeType::Box:
    default:
        Collider.Shape.Type = SnAPI::Physics::EShapeType::Box;
        Collider.Shape.Box.HalfExtent = ToPhysicsVec3(Settings.HalfExtent);
        break;
    }

    Physics::SetTransformPosition(Collider.LocalTransform, ToPhysicsVec3(Settings.LocalPosition));
    Physics::SetTransformRotation(Collider.LocalTransform, EulerToQuat(Settings.LocalRotation));

    Collider.Density = Settings.Density;
    Collider.Friction = Settings.Friction;
    Collider.Restitution = Settings.Restitution;
    Collider.Layer = static_cast<SnAPI::Physics::CollisionLayer>(CollisionLayerToIndex(Settings.Layer));
    Collider.Mask = static_cast<SnAPI::Physics::CollisionMask>(Settings.Mask.Value());
    Collider.IsTrigger = Settings.IsTrigger;

    return Collider;
}

} // namespace

PhysicsSystem* RigidBodyComponent::ResolvePhysicsSystem() const
{
    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return nullptr;
    }

    auto* WorldPtr = Owner->World();
    if (!WorldPtr)
    {
        return nullptr;
    }

    return &WorldPtr->Physics();
}

void RigidBodyComponent::OnCreate()
{
    CreateBody();
}

void RigidBodyComponent::OnDestroy()
{
    DestroyBody();
}

void RigidBodyComponent::FixedTick(float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (!m_bodyHandle.IsValid())
    {
        if (!CreateBody())
        {
            return;
        }
    }

    if (m_settings.BodyType == Physics::EBodyType::Dynamic)
    {
        SyncFromPhysics();
    }
    else
    {
        SyncToPhysics();
    }
}

bool RigidBodyComponent::CreateBody()
{
    if (m_bodyHandle.IsValid())
    {
        return true;
    }

    auto* Owner = OwnerNode();
    auto* Physics = ResolvePhysicsSystem();
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Owner || !Scene)
    {
        return false;
    }

    SnAPI::Physics::BodyDesc Desc{};
    Desc.BodyType = m_settings.BodyType;
    Desc.WorldTransform = ReadOwnerTransform(Owner);
    Desc.LinearVelocity = ToPhysicsVec3(m_settings.InitialLinearVelocity);
    Desc.AngularVelocity = ToPhysicsVec3(m_settings.InitialAngularVelocity);
    Desc.Mass = m_settings.Mass;
    Desc.LinearDamping = m_settings.LinearDamping;
    Desc.AngularDamping = m_settings.AngularDamping;
    Desc.EnableCcd = m_settings.EnableCcd;
    Desc.StartActive = m_settings.StartActive;
    Desc.Colliders.push_back(BuildColliderDesc(Owner));

    const auto CreateResult = Scene->Rigid().CreateBody(Desc);
    if (!CreateResult)
    {
        return false;
    }

    m_bodyHandle = CreateResult.value();
    BindPhysicsEvents();
    if (m_settings.BodyType == Physics::EBodyType::Dynamic)
    {
        if (const auto SleepingResult = Scene->Rigid().IsBodySleeping(m_bodyHandle))
        {
            UpdateSleepDrivenActivity(SleepingResult.value());
        }
        else
        {
            UpdateSleepDrivenActivity(false);
        }
    }
    else
    {
        m_isSleeping = false;
        Active(true);
    }
    return true;
}

void RigidBodyComponent::DestroyBody()
{
    UnbindPhysicsEvents();
    if (!m_bodyHandle.IsValid())
    {
        return;
    }

    auto* Physics = ResolvePhysicsSystem();
    if (auto* Scene = Physics ? Physics->Scene() : nullptr)
    {
        (void)Scene->Rigid().DestroyBody(m_bodyHandle);
    }

    m_bodyHandle = {};
    m_isSleeping = false;
    Active(true);
}

bool RigidBodyComponent::RecreateBody()
{
    DestroyBody();
    return CreateBody();
}

bool RigidBodyComponent::ApplyForce(const Vec3& Force, const bool AsImpulse)
{
    const auto Mode = AsImpulse ? Physics::EForceMode::Impulse : Physics::EForceMode::Force;
    return ApplyForce(Force, Mode);
}

bool RigidBodyComponent::ApplyForce(const Vec3& Force, SnAPI::Physics::EForceMode Mode)
{
    if (!m_bodyHandle.IsValid() && !CreateBody())
    {
        return false;
    }

    auto* Physics = ResolvePhysicsSystem();
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Scene)
    {
        return false;
    }

    auto Result = Scene->Rigid().ApplyForce(m_bodyHandle, ToPhysicsVec3(Force), Mode);
    if (!Result)
    {
        return false;
    }

    if (m_settings.BodyType == Physics::EBodyType::Dynamic)
    {
        UpdateSleepDrivenActivity(false);
    }
    return true;
}

bool RigidBodyComponent::SetVelocity(const Vec3& Linear, const Vec3& Angular)
{
    if (!m_bodyHandle.IsValid() && !CreateBody())
    {
        return false;
    }

    auto* Physics = ResolvePhysicsSystem();
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Scene)
    {
        return false;
    }

    auto Result = Scene->Rigid().SetBodyVelocity(m_bodyHandle, ToPhysicsVec3(Linear), ToPhysicsVec3(Angular));
    if (!Result)
    {
        return false;
    }

    if (m_settings.BodyType == Physics::EBodyType::Dynamic)
    {
        UpdateSleepDrivenActivity(false);
    }
    return true;
}

bool RigidBodyComponent::Teleport(const Vec3& Position, const Vec3& Rotation, const bool ResetVelocity)
{
    if (!m_bodyHandle.IsValid() && !CreateBody())
    {
        return false;
    }

    auto* Physics = ResolvePhysicsSystem();
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Scene)
    {
        return false;
    }

    if (auto* Owner = OwnerNode())
    {
        auto TransformResult = Owner->Component<TransformComponent>();
        if (!TransformResult)
        {
            const auto AddedResult = Owner->Add<TransformComponent>();
            if (!AddedResult)
            {
                return false;
            }
            TransformResult = AddedResult;
        }

        TransformResult->Position = Position;
        TransformResult->Rotation = Rotation;
    }

    SnAPI::Physics::Transform TransformValue = SnAPI::Physics::IdentityTransform();
    SnAPI::Physics::SetTransformPosition(TransformValue, ToPhysicsVec3(Position));
    SnAPI::Physics::SetTransformRotation(TransformValue, EulerToQuat(Rotation));
    if (auto Result = Scene->Rigid().SetBodyTransform(m_bodyHandle, TransformValue); !Result)
    {
        return false;
    }

    if (ResetVelocity)
    {
        if (auto Result = Scene->Rigid().SetBodyVelocity(m_bodyHandle,
                                                         SnAPI::Physics::Vec3::Zero(),
                                                         SnAPI::Physics::Vec3::Zero());
            !Result)
        {
            return false;
        }
    }

    if (m_settings.BodyType == Physics::EBodyType::Dynamic)
    {
        UpdateSleepDrivenActivity(false);
    }
    return true;
}

void RigidBodyComponent::BindPhysicsEvents()
{
    if (m_sleepListenerToken != 0 || !m_bodyHandle.IsValid())
    {
        return;
    }

    auto* Physics = ResolvePhysicsSystem();
    if (!Physics)
    {
        return;
    }

    m_sleepListenerToken = Physics->AddBodySleepListener(m_bodyHandle, [this](const SnAPI::Physics::PhysicsEvent& Event) {
        HandlePhysicsEvent(Event);
    });
}

void RigidBodyComponent::UnbindPhysicsEvents()
{
    if (m_sleepListenerToken == 0)
    {
        return;
    }

    auto* Physics = ResolvePhysicsSystem();
    if (Physics)
    {
        (void)Physics->RemoveBodySleepListener(m_sleepListenerToken);
    }
    m_sleepListenerToken = 0;
}

void RigidBodyComponent::HandlePhysicsEvent(const SnAPI::Physics::PhysicsEvent& Event)
{
    if (!m_bodyHandle.IsValid())
    {
        return;
    }
    if (m_settings.BodyType != Physics::EBodyType::Dynamic)
    {
        return;
    }

    if (Event.BodyA != m_bodyHandle && Event.BodyB != m_bodyHandle)
    {
        return;
    }

    if (Event.Type == SnAPI::Physics::EPhysicsEventType::BodySleep)
    {
        UpdateSleepDrivenActivity(true);
    }
    else if (Event.Type == SnAPI::Physics::EPhysicsEventType::BodyWake)
    {
        UpdateSleepDrivenActivity(false);
    }
}

void RigidBodyComponent::UpdateSleepDrivenActivity(const bool Sleeping)
{
    m_isSleeping = Sleeping;
    if (!m_settings.AutoDeactivateWhenSleeping || m_settings.BodyType != Physics::EBodyType::Dynamic)
    {
        Active(true);
        return;
    }

    Active(!Sleeping);
}

bool RigidBodyComponent::SyncFromPhysics() const
{
    if (!m_settings.SyncFromPhysics || !m_bodyHandle.IsValid())
    {
        return false;
    }

    auto* Physics = ResolvePhysicsSystem();
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Scene)
    {
        return false;
    }

    auto TransformResult = Scene->Rigid().BodyTransform(m_bodyHandle);
    if (!TransformResult)
    {
        return false;
    }

    WriteOwnerTransform(OwnerNode(), TransformResult.value());
    return true;
}

bool RigidBodyComponent::SyncToPhysics() const
{
    if (!m_settings.SyncToPhysics || !m_bodyHandle.IsValid())
    {
        return false;
    }

    auto* Physics = ResolvePhysicsSystem();
    auto* Scene = Physics ? Physics->Scene() : nullptr;
    if (!Scene)
    {
        return false;
    }

    const SnAPI::Physics::Transform TransformValue = ReadOwnerTransform(OwnerNode());
    const auto Result = Scene->Rigid().SetBodyTransform(m_bodyHandle, TransformValue);
    return Result.has_value();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
