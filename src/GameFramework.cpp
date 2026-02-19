#include "TypeRegistration.h"

#include "BaseNode.h"
#include "BuiltinTypes.h"
#include "Level.h"
#include "NodeGraph.h"
#include "FollowTargetComponent.h"
#include "Relevance.h"
#include "Serialization.h"
#include "ScriptComponent.h"
#include "TransformComponent.h"
#include "TypeAutoRegistration.h"
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "CharacterMovementController.h"
#include "ColliderComponent.h"
#if defined(SNAPI_GF_ENABLE_INPUT)
#include "InputComponent.h"
#endif
#include "RigidBodyComponent.h"
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
#include "AudioListenerComponent.h"
#include "AudioSourceComponent.h"
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "CameraComponent.h"
#include "SkeletalMeshComponent.h"
#include "StaticMeshComponent.h"
#endif
#include "TypeRegistry.h"
#include "World.h"

#include <initializer_list>

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(BaseNode, (TTypeBuilder<BaseNode>(BaseNode::kTypeName)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(NodeGraph, (TTypeBuilder<NodeGraph>(NodeGraph::kTypeName)
    .Base<BaseNode>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Level, (TTypeBuilder<Level>(Level::kTypeName)
    .Base<NodeGraph>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(World, (TTypeBuilder<World>(World::kTypeName)
    .Base<NodeGraph>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TransformComponent, (TTypeBuilder<TransformComponent>(TransformComponent::kTypeName)
    .Field("Position", &TransformComponent::Position, EFieldFlagBits::Replication)
    .Field("Rotation", &TransformComponent::Rotation, EFieldFlagBits::Replication)
    .Field("Scale", &TransformComponent::Scale, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(FollowTargetComponent::Settings, (TTypeBuilder<FollowTargetComponent::Settings>(FollowTargetComponent::Settings::kTypeName)
    .Field("Target", &FollowTargetComponent::Settings::Target)
    .Field("PositionOffset", &FollowTargetComponent::Settings::PositionOffset)
    .Field("SyncPosition", &FollowTargetComponent::Settings::SyncPosition)
    .Field("SyncRotation", &FollowTargetComponent::Settings::SyncRotation)
    .Field("RotationOffset", &FollowTargetComponent::Settings::RotationOffset)
    .Field("PositionSmoothingHz", &FollowTargetComponent::Settings::PositionSmoothingHz)
    .Field("RotationSmoothingHz", &FollowTargetComponent::Settings::RotationSmoothingHz)
    .Field("ResolveTargetByUuidFallback", &FollowTargetComponent::Settings::ResolveTargetByUuidFallback)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(FollowTargetComponent, (TTypeBuilder<FollowTargetComponent>(FollowTargetComponent::kTypeName)
    .Field("Settings",
           &FollowTargetComponent::EditSettings,
           &FollowTargetComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(RelevanceComponent, (TTypeBuilder<RelevanceComponent>(RelevanceComponent::kTypeName)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(ScriptComponent, (TTypeBuilder<ScriptComponent>(ScriptComponent::kTypeName)
    .Field("ScriptModule", &ScriptComponent::ScriptModule)
    .Field("ScriptType", &ScriptComponent::ScriptType)
    .Field("Instance", &ScriptComponent::Instance)
    .Constructor<>()
    .Register()));

#if defined(SNAPI_GF_ENABLE_AUDIO)

SNAPI_REFLECT_TYPE(AudioSourceComponent::Settings, (TTypeBuilder<AudioSourceComponent::Settings>(AudioSourceComponent::Settings::kTypeName)
    .Field("SoundPath", &AudioSourceComponent::Settings::SoundPath, EFieldFlagBits::Replication)
    .Field("Streaming", &AudioSourceComponent::Settings::Streaming)
    .Field("AutoPlay", &AudioSourceComponent::Settings::AutoPlay)
    .Field("Looping", &AudioSourceComponent::Settings::Looping)
    .Field("Volume", &AudioSourceComponent::Settings::Volume)
    .Field("SpatialGain", &AudioSourceComponent::Settings::SpatialGain)
    .Field("MinDistance", &AudioSourceComponent::Settings::MinDistance)
    .Field("MaxDistance", &AudioSourceComponent::Settings::MaxDistance)
    .Field("Rolloff", &AudioSourceComponent::Settings::Rolloff)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AudioSourceComponent, (TTypeBuilder<AudioSourceComponent>(AudioSourceComponent::kTypeName)
    .Field("Settings",
           &AudioSourceComponent::EditSettings,
           &AudioSourceComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Method("PlayServer",
            &AudioSourceComponent::PlayServer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("PlayClient",
            &AudioSourceComponent::PlayClient,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetMulticast)
    .Method("StopServer",
            &AudioSourceComponent::StopServer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("StopClient",
            &AudioSourceComponent::StopClient,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetMulticast)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AudioListenerComponent, (TTypeBuilder<AudioListenerComponent>(AudioListenerComponent::kTypeName)
    .Field("Active", &AudioListenerComponent::EditActive, &AudioListenerComponent::GetActive)
    .Method("SetActiveServer",
            &AudioListenerComponent::SetActiveServer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("SetActiveClient",
            &AudioListenerComponent::SetActiveClient,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetMulticast)
    .Constructor<>()
    .Register()));

#endif // SNAPI_GF_ENABLE_AUDIO

#if defined(SNAPI_GF_ENABLE_PHYSICS)

SNAPI_REFLECT_TYPE(ColliderComponent::Settings, (TTypeBuilder<ColliderComponent::Settings>(ColliderComponent::Settings::kTypeName)
    .Field("Shape", &ColliderComponent::Settings::Shape)
    .Field("HalfExtent", &ColliderComponent::Settings::HalfExtent)
    .Field("Radius", &ColliderComponent::Settings::Radius)
    .Field("HalfHeight", &ColliderComponent::Settings::HalfHeight)
    .Field("LocalPosition", &ColliderComponent::Settings::LocalPosition)
    .Field("LocalRotation", &ColliderComponent::Settings::LocalRotation)
    .Field("Density", &ColliderComponent::Settings::Density)
    .Field("Friction", &ColliderComponent::Settings::Friction)
    .Field("Restitution", &ColliderComponent::Settings::Restitution)
    .Field("Layer", &ColliderComponent::Settings::Layer)
    .Field("Mask", &ColliderComponent::Settings::Mask)
    .Field("IsTrigger", &ColliderComponent::Settings::IsTrigger)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(ColliderComponent, (TTypeBuilder<ColliderComponent>(ColliderComponent::kTypeName)
    .Field("Settings",
           &ColliderComponent::EditSettings,
           &ColliderComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(RigidBodyComponent::Settings, (TTypeBuilder<RigidBodyComponent::Settings>(RigidBodyComponent::Settings::kTypeName)
    .Field("BodyType", &RigidBodyComponent::Settings::BodyType)
    .Field("Mass", &RigidBodyComponent::Settings::Mass)
    .Field("LinearDamping", &RigidBodyComponent::Settings::LinearDamping)
    .Field("AngularDamping", &RigidBodyComponent::Settings::AngularDamping)
    .Field("EnableCcd", &RigidBodyComponent::Settings::EnableCcd)
    .Field("StartActive", &RigidBodyComponent::Settings::StartActive)
    .Field("InitialLinearVelocity", &RigidBodyComponent::Settings::InitialLinearVelocity)
    .Field("InitialAngularVelocity", &RigidBodyComponent::Settings::InitialAngularVelocity)
    .Field("SyncFromPhysics", &RigidBodyComponent::Settings::SyncFromPhysics)
    .Field("SyncToPhysics", &RigidBodyComponent::Settings::SyncToPhysics)
    .Field("EnableRenderInterpolation", &RigidBodyComponent::Settings::EnableRenderInterpolation)
    .Field("AutoDeactivateWhenSleeping", &RigidBodyComponent::Settings::AutoDeactivateWhenSleeping)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(RigidBodyComponent, (TTypeBuilder<RigidBodyComponent>(RigidBodyComponent::kTypeName)
    .Field("Settings",
           &RigidBodyComponent::EditSettings,
           &RigidBodyComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(CharacterMovementController::Settings, (TTypeBuilder<CharacterMovementController::Settings>(CharacterMovementController::Settings::kTypeName)
    .Field("MoveForce", &CharacterMovementController::Settings::MoveForce)
    .Field("JumpImpulse", &CharacterMovementController::Settings::JumpImpulse)
    .Field("GroundProbeStartOffset", &CharacterMovementController::Settings::GroundProbeStartOffset)
    .Field("GroundProbeDistance", &CharacterMovementController::Settings::GroundProbeDistance)
    .Field("GroundMask", &CharacterMovementController::Settings::GroundMask)
    .Field("ConsumeInputEachTick", &CharacterMovementController::Settings::ConsumeInputEachTick)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(CharacterMovementController, (TTypeBuilder<CharacterMovementController>(CharacterMovementController::kTypeName)
    .Field("Settings",
           &CharacterMovementController::EditSettings,
           &CharacterMovementController::GetSettings)
    .Method("SetMoveInput", &CharacterMovementController::SetMoveInput)
    .Method("AddMoveInput", &CharacterMovementController::AddMoveInput)
    .Method("Jump", &CharacterMovementController::Jump)
    .Constructor<>()
    .Register()));

#if defined(SNAPI_GF_ENABLE_INPUT)

SNAPI_REFLECT_TYPE(InputComponent::Settings, (TTypeBuilder<InputComponent::Settings>(InputComponent::Settings::kTypeName)
    .Field("MovementEnabled", &InputComponent::Settings::MovementEnabled)
    .Field("JumpEnabled", &InputComponent::Settings::JumpEnabled)
    .Field("KeyboardEnabled", &InputComponent::Settings::KeyboardEnabled)
    .Field("GamepadEnabled", &InputComponent::Settings::GamepadEnabled)
    .Field("RequireInputFocus", &InputComponent::Settings::RequireInputFocus)
    .Field("NormalizeMove", &InputComponent::Settings::NormalizeMove)
    .Field("ClearMoveWhenUnavailable", &InputComponent::Settings::ClearMoveWhenUnavailable)
    .Field("MoveScale", &InputComponent::Settings::MoveScale)
    .Field("GamepadDeadzone", &InputComponent::Settings::GamepadDeadzone)
    .Field("InvertGamepadY", &InputComponent::Settings::InvertGamepadY)
    .Field("MoveForwardKey", &InputComponent::Settings::MoveForwardKey)
    .Field("MoveBackwardKey", &InputComponent::Settings::MoveBackwardKey)
    .Field("MoveLeftKey", &InputComponent::Settings::MoveLeftKey)
    .Field("MoveRightKey", &InputComponent::Settings::MoveRightKey)
    .Field("JumpKey", &InputComponent::Settings::JumpKey)
    .Field("MoveGamepadXAxis", &InputComponent::Settings::MoveGamepadXAxis)
    .Field("MoveGamepadYAxis", &InputComponent::Settings::MoveGamepadYAxis)
    .Field("JumpGamepadButton", &InputComponent::Settings::JumpGamepadButton)
    .Field("PreferredGamepad", &InputComponent::Settings::PreferredGamepad)
    .Field("UseAnyGamepadWhenPreferredMissing", &InputComponent::Settings::UseAnyGamepadWhenPreferredMissing)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(InputComponent, (TTypeBuilder<InputComponent>(InputComponent::kTypeName)
    .Field("Settings",
           &InputComponent::EditSettings,
           &InputComponent::GetSettings)
    .Constructor<>()
    .Register()));

#endif // SNAPI_GF_ENABLE_INPUT

#endif // SNAPI_GF_ENABLE_PHYSICS

#if defined(SNAPI_GF_ENABLE_RENDERER)

SNAPI_REFLECT_TYPE(CameraComponent::Settings, (TTypeBuilder<CameraComponent::Settings>(CameraComponent::Settings::kTypeName)
    .Field("NearClip", &CameraComponent::Settings::NearClip)
    .Field("FarClip", &CameraComponent::Settings::FarClip)
    .Field("FovDegrees", &CameraComponent::Settings::FovDegrees)
    .Field("Aspect", &CameraComponent::Settings::Aspect)
    .Field("Active", &CameraComponent::Settings::Active)
    .Field("SyncFromTransform", &CameraComponent::Settings::SyncFromTransform)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(CameraComponent, (TTypeBuilder<CameraComponent>(CameraComponent::kTypeName)
    .Field("Settings",
           &CameraComponent::EditSettings,
           &CameraComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Method("SetActive", &CameraComponent::SetActive)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(StaticMeshComponent::Settings, (TTypeBuilder<StaticMeshComponent::Settings>(StaticMeshComponent::Settings::kTypeName)
    .Field("MeshPath", &StaticMeshComponent::Settings::MeshPath, EFieldFlagBits::Replication)
    .Field("Visible", &StaticMeshComponent::Settings::Visible, EFieldFlagBits::Replication)
    .Field("CastShadows", &StaticMeshComponent::Settings::CastShadows, EFieldFlagBits::Replication)
    .Field("SyncFromTransform", &StaticMeshComponent::Settings::SyncFromTransform)
    .Field("RegisterWithRenderer", &StaticMeshComponent::Settings::RegisterWithRenderer)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(StaticMeshComponent, (TTypeBuilder<StaticMeshComponent>(StaticMeshComponent::kTypeName)
    .Field("Settings",
           &StaticMeshComponent::EditSettings,
           &StaticMeshComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Method("ReloadMesh", &StaticMeshComponent::ReloadMesh)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SkeletalMeshComponent::Settings, (TTypeBuilder<SkeletalMeshComponent::Settings>(SkeletalMeshComponent::Settings::kTypeName)
    .Field("MeshPath", &SkeletalMeshComponent::Settings::MeshPath, EFieldFlagBits::Replication)
    .Field("Visible", &SkeletalMeshComponent::Settings::Visible, EFieldFlagBits::Replication)
    .Field("CastShadows", &SkeletalMeshComponent::Settings::CastShadows, EFieldFlagBits::Replication)
    .Field("SyncFromTransform", &SkeletalMeshComponent::Settings::SyncFromTransform)
    .Field("RegisterWithRenderer", &SkeletalMeshComponent::Settings::RegisterWithRenderer)
    .Field("AutoPlayAnimations", &SkeletalMeshComponent::Settings::AutoPlayAnimations)
    .Field("LoopAnimations", &SkeletalMeshComponent::Settings::LoopAnimations)
    .Field("AnimationName", &SkeletalMeshComponent::Settings::AnimationName)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SkeletalMeshComponent, (TTypeBuilder<SkeletalMeshComponent>(SkeletalMeshComponent::kTypeName)
    .Field("Settings",
           &SkeletalMeshComponent::EditSettings,
           &SkeletalMeshComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Method("ReloadMesh", &SkeletalMeshComponent::ReloadMesh)
    .Method("PlayAnimation", &SkeletalMeshComponent::PlayAnimation)
    .Method("PlayAllAnimations", &SkeletalMeshComponent::PlayAllAnimations)
    .Method("StopAnimations", &SkeletalMeshComponent::StopAnimations)
    .Constructor<>()
    .Register()));

#endif // SNAPI_GF_ENABLE_RENDERER

void RegisterBuiltinTypes()
{
    TypeInfo VoidInfo;
    VoidInfo.Id = TypeIdFromName("void");
    VoidInfo.Name = "void";
    VoidInfo.Size = 0;
    VoidInfo.Align = 0;
    TypeRegistry::Instance().Register(std::move(VoidInfo));

    auto RegisterPlain = [](const char* Name, size_t Size, size_t Align) {
        TypeInfo Info;
        Info.Id = TypeIdFromName(Name);
        Info.Name = Name;
        Info.Size = Size;
        Info.Align = Align;
        (void)TypeRegistry::Instance().Register(std::move(Info));
    };
    auto RegisterEnum = [](const char* Name,
                           const size_t Size,
                           const size_t Align,
                           const bool IsSigned,
                           const std::initializer_list<EnumValueInfo> Values) {
        TypeInfo Info;
        Info.Id = TypeIdFromName(Name);
        Info.Name = Name;
        Info.Size = Size;
        Info.Align = Align;
        Info.IsEnum = true;
        Info.EnumIsSigned = IsSigned;
        Info.EnumValues.assign(Values.begin(), Values.end());
        (void)TypeRegistry::Instance().Register(std::move(Info));
    };

    RegisterPlain(TTypeNameV<bool>, sizeof(bool), alignof(bool));
    RegisterPlain(TTypeNameV<int>, sizeof(int), alignof(int));
    RegisterPlain(TTypeNameV<unsigned int>, sizeof(unsigned int), alignof(unsigned int));
    RegisterPlain(TTypeNameV<std::uint64_t>, sizeof(std::uint64_t), alignof(std::uint64_t));
    RegisterPlain(TTypeNameV<float>, sizeof(float), alignof(float));
    RegisterPlain(TTypeNameV<double>, sizeof(double), alignof(double));
    RegisterPlain(TTypeNameV<std::string>, sizeof(std::string), alignof(std::string));
    RegisterPlain(TTypeNameV<std::vector<uint8_t>>, sizeof(std::vector<uint8_t>), alignof(std::vector<uint8_t>));
    RegisterPlain(TTypeNameV<Uuid>, sizeof(Uuid), alignof(Uuid));
    RegisterPlain(TTypeNameV<Vec3>, sizeof(Vec3), alignof(Vec3));
    RegisterPlain(TTypeNameV<Quat>, sizeof(Quat), alignof(Quat));
    RegisterPlain(TTypeNameV<NodeHandle>, sizeof(NodeHandle), alignof(NodeHandle));
    RegisterPlain(TTypeNameV<ComponentHandle>, sizeof(ComponentHandle), alignof(ComponentHandle));
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    RegisterPlain(TTypeNameV<ECollisionFilterBits>, sizeof(ECollisionFilterBits), alignof(ECollisionFilterBits));
    RegisterPlain(TTypeNameV<CollisionFilterFlags>, sizeof(CollisionFilterFlags), alignof(CollisionFilterFlags));
    RegisterEnum(
        TTypeNameV<SnAPI::Physics::EBodyType>,
        sizeof(SnAPI::Physics::EBodyType),
        alignof(SnAPI::Physics::EBodyType),
        false,
        {
            EnumValueInfo{"Static", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Static)},
            EnumValueInfo{"Kinematic", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Kinematic)},
            EnumValueInfo{"Dynamic", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Dynamic)},
        });
    RegisterPlain(TTypeNameV<SnAPI::Physics::EShapeType>, sizeof(SnAPI::Physics::EShapeType), alignof(SnAPI::Physics::EShapeType));
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
    RegisterPlain(TTypeNameV<SnAPI::Input::EKey>, sizeof(SnAPI::Input::EKey), alignof(SnAPI::Input::EKey));
    RegisterPlain(TTypeNameV<SnAPI::Input::EGamepadAxis>, sizeof(SnAPI::Input::EGamepadAxis), alignof(SnAPI::Input::EGamepadAxis));
    RegisterPlain(TTypeNameV<SnAPI::Input::EGamepadButton>, sizeof(SnAPI::Input::EGamepadButton), alignof(SnAPI::Input::EGamepadButton));
    RegisterPlain(TTypeNameV<SnAPI::Input::DeviceId>, sizeof(SnAPI::Input::DeviceId), alignof(SnAPI::Input::DeviceId));
#endif

    RegisterSerializationDefaults();
#if defined(SNAPI_GF_ENABLE_PHYSICS) || defined(SNAPI_GF_ENABLE_INPUT)
    auto& ValueRegistry = ValueCodecRegistry::Instance();
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    ValueRegistry.Register<ECollisionFilterBits>();
    ValueRegistry.Register<CollisionFilterFlags>();
    ValueRegistry.Register<SnAPI::Physics::EBodyType>();
    ValueRegistry.Register<SnAPI::Physics::EShapeType>();
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
    ValueRegistry.Register<SnAPI::Input::EKey>();
    ValueRegistry.Register<SnAPI::Input::EGamepadAxis>();
    ValueRegistry.Register<SnAPI::Input::EGamepadButton>();
    ValueRegistry.Register<SnAPI::Input::DeviceId>();
#endif
}

} // namespace SnAPI::GameFramework
