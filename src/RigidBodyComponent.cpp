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

SnAPI::Physics::Vec3 WorldToPhysicsPosition(PhysicsSystem* PhysicsSystemPtr,
                                            const Vec3& WorldPosition,
                                            const bool AllowInitializeOrigin)
{
    
    if (!PhysicsSystemPtr)
    {
        return ToPhysicsVec3(WorldPosition);
    }

    return PhysicsSystemPtr->WorldToPhysicsPosition(ToPhysicsVec3(WorldPosition), AllowInitializeOrigin);
}

Vec3 PhysicsToWorldPosition(const PhysicsSystem* PhysicsSystemPtr, const SnAPI::Physics::Vec3& PhysicsPosition)
{
    
    if (!PhysicsSystemPtr)
    {
        return FromPhysicsVec3(PhysicsPosition);
    }

    return FromPhysicsVec3(PhysicsSystemPtr->PhysicsToWorldPosition(PhysicsPosition));
}

SnAPI::Physics::Quat EulerToQuat(const Vec3& Euler)
{
    
    const SnAPI::Math::Quaternion Rotation = SnAPI::Math::AngleAxis3D(Euler.z(), SnAPI::Math::Vector3::UnitZ())
                                           * SnAPI::Math::AngleAxis3D(Euler.y(), SnAPI::Math::Vector3::UnitY())
                                           * SnAPI::Math::AngleAxis3D(Euler.x(), SnAPI::Math::Vector3::UnitX());
    return SnAPI::Physics::MakeQuatXYZW(Rotation.x(), Rotation.y(), Rotation.z(), Rotation.w());
}

SnAPI::Physics::Quat ToPhysicsQuat(const Quat& Rotation)
{
    
    return SnAPI::Physics::MakeQuatXYZW(Rotation.x(), Rotation.y(), Rotation.z(), Rotation.w());
}

Quat FromPhysicsQuat(const SnAPI::Physics::Quat& Rotation)
{
    
    Quat Out = Quat::Identity();
    Out.x() = static_cast<Quat::Scalar>(Rotation.x());
    Out.y() = static_cast<Quat::Scalar>(Rotation.y());
    Out.z() = static_cast<Quat::Scalar>(Rotation.z());
    Out.w() = static_cast<Quat::Scalar>(Rotation.w());
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

Physics::Transform ReadOwnerTransform(BaseNode* Owner,
                                     PhysicsSystem* PhysicsSystemPtr,
                                     const bool AllowInitializeOrigin)
{
    
    Physics::Transform Out = Physics::IdentityTransform();
    if (!Owner)
    {
        return Out;
    }

    auto TransformResult = [&]() {
        
        return Owner->Component<TransformComponent>();
    }();
    if (!TransformResult)
    {
        return Out;
    }

    {
        
        Physics::SetTransformPosition(Out, WorldToPhysicsPosition(PhysicsSystemPtr, TransformResult->Position, AllowInitializeOrigin));
    }
    {
        
        Physics::SetTransformRotation(Out, ToPhysicsQuat(TransformResult->Rotation));
    }
    return Out;
}

void WriteOwnerTransform(BaseNode* Owner,
                         const Physics::Vec3& PhysicsPosition,
                         const Physics::Quat& PhysicsRotation,
                         const PhysicsSystem* PhysicsSystemPtr)
{
    
    if (!Owner)
    {
        return;
    }

    auto TransformResult = [&]() {
        
        return Owner->Component<TransformComponent>();
    }();
    if (!TransformResult)
    {
        const auto AddedResult = [&]() {
            
            return Owner->Add<TransformComponent>();
        }();
        if (!AddedResult)
        {
            return;
        }
        TransformResult = AddedResult;
    }

    {
        
        const Vec3 WorldPosition = PhysicsToWorldPosition(PhysicsSystemPtr, PhysicsPosition);
        TransformResult->Position = WorldPosition;
    }
    {
        
        TransformResult->Rotation = FromPhysicsQuat(PhysicsRotation);
    }
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
    Desc.WorldTransform = ReadOwnerTransform(Owner, Physics, true);
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
    m_lastSyncedPhysicsPosition = SnAPI::Physics::TransformPosition(Desc.WorldTransform);
    m_lastSyncedPhysicsRotation = SnAPI::Physics::TransformRotation(Desc.WorldTransform);
    m_hasLastSyncedTransform = true;
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
        Scene->Rigid().DestroyBody(m_bodyHandle);
    }

    m_bodyHandle = {};
    m_isSleeping = false;
    m_hasLastSyncedTransform = false;
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

bool RigidBodyComponent::Teleport(const Vec3& Position, const Quat& Rotation, const bool ResetVelocity)
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
    SnAPI::Physics::SetTransformPosition(TransformValue, WorldToPhysicsPosition(Physics, Position, true));
    SnAPI::Physics::SetTransformRotation(TransformValue, ToPhysicsQuat(Rotation));
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

    m_lastSyncedPhysicsPosition = SnAPI::Physics::TransformPosition(TransformValue);
    m_lastSyncedPhysicsRotation = SnAPI::Physics::TransformRotation(TransformValue);
    m_hasLastSyncedTransform = true;

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

    if (auto* Physics = ResolvePhysicsSystem())
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
    if (!m_settings.SyncFromPhysics)
    {
        return false;
    }
    if (!m_bodyHandle.IsValid())
    {
        
        return false;
    }

    auto* Physics = [&]() {
        
        return ResolvePhysicsSystem();
    }();
    auto* Scene = [&]() {
        
        return Physics ? Physics->Scene() : nullptr;
    }();
    if (!Scene)
    {
        
        return false;
    }

    auto TransformResult = [&]() {
        
        return Scene->Rigid().BodyTransform(m_bodyHandle);
    }();
    if (!TransformResult)
    {
        
        return false;
    }

    SnAPI::Physics::Vec3 PhysicsPosition{};
    SnAPI::Physics::Quat PhysicsRotation{};
    {
        
        PhysicsPosition = SnAPI::Physics::TransformPosition(TransformResult.value());
        PhysicsRotation = SnAPI::Physics::TransformRotation(TransformResult.value());
    }

    constexpr double PositionEpsilon = 1e-5;
    constexpr double PositionEpsilonSq = PositionEpsilon * PositionEpsilon;
    constexpr double RotationDotThreshold = 0.999999995; // ~0.0002 rad (~0.011 deg)
    if (m_hasLastSyncedTransform)
    {
        
        const double PositionDeltaSq = (PhysicsPosition - m_lastSyncedPhysicsPosition).squaredNorm();
        const double RotationDot = std::abs(static_cast<double>(PhysicsRotation.dot(m_lastSyncedPhysicsRotation)));
        if (PositionDeltaSq <= PositionEpsilonSq && RotationDot >= RotationDotThreshold)
        {
            
            return true;
        }
    }

    m_lastSyncedPhysicsPosition = PhysicsPosition;
    m_lastSyncedPhysicsRotation = PhysicsRotation;
    m_hasLastSyncedTransform = true;

    {
        
        WriteOwnerTransform(OwnerNode(), PhysicsPosition, PhysicsRotation, Physics);
    }
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

    const SnAPI::Physics::Transform TransformValue = ReadOwnerTransform(OwnerNode(), Physics, true);
    const auto Result = Scene->Rigid().SetBodyTransform(m_bodyHandle, TransformValue);
    return Result.has_value();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
