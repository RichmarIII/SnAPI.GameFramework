#include "PawnBase.h"

#include "BaseNode.inl"
#include "TransformComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "CameraComponent.h"
#include "SprintArmComponent.h"
#include "StaticMeshComponent.h"
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "CharacterMovementController.h"
#include "ColliderComponent.h"
#include "InputIntentComponent.h"
#include "RigidBodyComponent.h"
#endif

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)
#include "InputComponent.h"
#endif

namespace SnAPI::GameFramework
{

PawnBase::PawnBase()
{
    TypeKey(StaticTypeId<PawnBase>());
    Replicated(true);
}

PawnBase::PawnBase(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<PawnBase>());
    Replicated(true);
}

void PawnBase::OnCreateImpl(IWorld& WorldRef)
{
    (void)WorldRef;
    EnsureDefaultComponents();
}

void PawnBase::OnPossess(const NodeHandle& PlayerHandle)
{
    (void)PlayerHandle;
#if defined(SNAPI_GF_ENABLE_RENDERER)
        if (auto Camera = Component<CameraComponent>())
        {
            if (Camera->GetSettings().AutoActivateForPlayer)
                Camera->SetActive(true);
        }
#endif
}

void PawnBase::OnUnpossess(const NodeHandle& PlayerHandle)
{
    (void)PlayerHandle;
#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (auto Camera = Component<CameraComponent>())
    {
        Camera->SetActive(false);
    }
#endif
}

void PawnBase::EnsureDefaultComponents()
{
    if (!Has<TransformComponent>())
    {
        (void)Add<TransformComponent>();
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (!Has<SprintArmComponent>())
    {
        auto SprintArm = Add<SprintArmComponent>();
        if (SprintArm)
        {
            auto& Settings = SprintArm->EditSettings();
            Settings.Enabled = true;
            Settings.DriveOwnerYaw = true;
            Settings.ArmLength = 2.8f;
            Settings.SocketOffset = Vec3(0.0f, 1.35f, 0.0f);
            Settings.YawDegrees = 0.0f;
            Settings.PitchDegrees = -12.0f;
            Settings.MinPitchDegrees = -80.0f;
            Settings.MaxPitchDegrees = 80.0f;
        }
    }

    if (!Has<CameraComponent>())
    {
        auto Camera = Add<CameraComponent>();
        if (Camera)
        {
            auto& Settings = Camera->EditSettings();
            Settings.NearClip = 0.05f;
            Settings.FarClip = 5000.0f;
            Settings.FovDegrees = 70.0f;
            Settings.Aspect = 16.0f / 9.0f;
            Settings.Active = false;
            Settings.SyncFromTransform = true;
            Settings.LocalPositionOffset = Vec3(0.0f, 1.35f, 2.8f);
            Settings.LocalRotationOffsetEuler = Vec3(-0.21f, 0.0f, 0.0f);
            Camera->SetActive(false);
        }
    }
    else if (auto Camera = Component<CameraComponent>())
    {
        Camera->SetActive(false);
    }

    if (!Has<StaticMeshComponent>())
    {
        auto Mesh = Add<StaticMeshComponent>();
        if (Mesh)
        {
            auto& Settings = Mesh->EditSettings();
            Settings.MeshPath = "primitive://capsule";
            Settings.Visible = true;
            Settings.CastShadows = true;
            Settings.SyncFromTransform = true;
            Settings.RegisterWithRenderer = true;
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (!Has<InputIntentComponent>())
    {
        (void)Add<InputIntentComponent>();
    }

    if (!Has<ColliderComponent>())
    {
        auto Collider = Add<ColliderComponent>();
        if (Collider)
        {
            auto& Settings = Collider->EditSettings();
            Settings.Shape = SnAPI::Physics::EShapeType::Capsule;
            Settings.Radius = 0.35f;
            Settings.HalfHeight = 0.6f;
            Settings.Friction = 0.85f;
            Settings.Restitution = 0.0f;
            Settings.Density = 1.0f;
            Settings.IsTrigger = false;
        }
    }

    if (!Has<RigidBodyComponent>())
    {
        RigidBodyComponent::Settings Settings{};
        Settings.BodyType = SnAPI::Physics::EBodyType::Dynamic;
        Settings.Mass = 80.0f;
        Settings.LinearDamping = 0.2f;
        Settings.AngularDamping = 8.0f;
        Settings.EnableCcd = true;
        Settings.StartActive = true;
        Settings.SyncFromPhysics = true;
        Settings.SyncToPhysics = false;
        Settings.EnableRenderInterpolation = true;
        Settings.AutoDeactivateWhenSleeping = false;
        auto Body = Add<RigidBodyComponent>(Settings);
        if (Body)
        {

        }
    }

    if (!Has<CharacterMovementController>())
    {
        auto Movement = Add<CharacterMovementController>();
        if (Movement)
        {
            auto& Settings = Movement->EditSettings();
            Settings.MoveForce = 45.0f;
            Settings.JumpImpulse = 5.5f;
            Settings.GroundProbeStartOffset = 0.05f;
            Settings.GroundProbeDistance = 0.25f;
            Settings.ConsumeInputEachTick = false;
            Settings.KeepUpright = true;
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_PHYSICS)
    if (!Has<InputComponent>())
    {
        (void)Add<InputComponent>();
    }
#endif
}

} // namespace SnAPI::GameFramework
