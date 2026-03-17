#include "TypeRegistration.h"

#include "IAsset.h"
#include "AssetRef.h"
#include "BaseNode.h"
#include "BuiltinTypes.h"
#include "Conduit/Asset.h"
#include "Conduit/ClassComponent.h"
#include "GameplayRpcGateway.h"
#include "Level.h"
#include "LocalPlayer.h"
#include "MultiplayerConfigNode.h"
#include "NodeAsset.h"
#include "PawnBase.h"
#include "PlayerStart.h"
#include "FollowTargetComponent.h"
#include "FrameGraphNode.h"
#include "Relevance.h"
#include "Serialization.h"
#include "ScriptComponent.h"
#include "SubClassOf.h"
#include "TransformComponent.h"
#include "InputIntentComponent.h"
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
#include "AtmosphereCompositeParamsNode.h"
#include "AtmosphereParamsNode.h"
#include "BloomParamsNode.h"
#include "CameraComponent.h"
#include "DirectionalLightComponent.h"
#include "EnvironmentCaptureComponent.h"
#include "EnvironmentProbeNode.h"
#include "HeightFogParamsNode.h"
#include "SSAOParamsNode.h"
#include "SSGIParamsNode.h"
#include "SSRParamsNode.h"
#include "TAAParamsNode.h"
#include "SkeletalMeshComponent.h"
#include "SprintArmComponent.h"
#include "StaticMeshComponent.h"
#include "ToneMapParamsNode.h"
#include "WorldRenderSettings.h"
#endif
#if defined(SNAPI_GF_ENABLE_UI)
#include <UILayout.h>
#endif
#include "TypeRegistry.h"
#include "World.h"

#include <array>
#include <initializer_list>
#include <type_traits>

#include "CameraComponent.h"

namespace SnAPI::GameFramework
{

static_assert(!std::is_polymorphic_v<TransformComponent>, "TransformComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<InputIntentComponent>, "InputIntentComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<FollowTargetComponent>, "FollowTargetComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<RelevanceComponent>, "RelevanceComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<ScriptComponent>, "ScriptComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<Conduit::ClassComponent>, "Conduit::ClassComponent must be non-polymorphic runtime type");
#if defined(SNAPI_GF_ENABLE_AUDIO)
static_assert(!std::is_polymorphic_v<AudioSourceComponent>, "AudioSourceComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<AudioListenerComponent>, "AudioListenerComponent must be non-polymorphic runtime type");
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
static_assert(!std::is_polymorphic_v<ColliderComponent>, "ColliderComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<RigidBodyComponent>, "RigidBodyComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<CharacterMovementController>,
              "CharacterMovementController must be non-polymorphic runtime type");
#if defined(SNAPI_GF_ENABLE_INPUT)
static_assert(!std::is_polymorphic_v<InputComponent>, "InputComponent must be non-polymorphic runtime type");
#endif
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
static_assert(!std::is_polymorphic_v<CameraComponent>, "CameraComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<DirectionalLightComponent>, "DirectionalLightComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<EnvironmentCaptureComponent>, "EnvironmentCaptureComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<SprintArmComponent>, "SprintArmComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<StaticMeshComponent>, "StaticMeshComponent must be non-polymorphic runtime type");
static_assert(!std::is_polymorphic_v<SkeletalMeshComponent>, "SkeletalMeshComponent must be non-polymorphic runtime type");
#endif

#if defined(SNAPI_GF_ENABLE_INPUT)

#endif

#if defined(SNAPI_GF_ENABLE_UI)

#endif

#if defined(SNAPI_GF_ENABLE_AUDIO)

#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)

#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)

#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)

#endif

#if defined(SNAPI_GF_ENABLE_UI)

#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)

#endif // SNAPI_GF_ENABLE_RENDERER

#if defined(SNAPI_GF_ENABLE_AUDIO)

#endif // SNAPI_GF_ENABLE_AUDIO

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#if defined(SNAPI_GF_ENABLE_INPUT)

#endif // SNAPI_GF_ENABLE_INPUT

#endif // SNAPI_GF_ENABLE_PHYSICS

#if defined(SNAPI_GF_ENABLE_RENDERER)

#endif // SNAPI_GF_ENABLE_RENDERER

void RegisterBuiltinTypes()
{
    TypeInfo VoidInfo;
    VoidInfo.Id = TypeIdFromName("void");
    VoidInfo.Name = "void";
    VoidInfo.Size = 0;
    VoidInfo.Align = 0;
    TypeRegistry::Instance().Register(std::move(VoidInfo));

    auto RegisterPlain = []<typename T>(const char* Name) {
        TypeInfo Info;
        Info.Id = TypeIdFromName(Name);
        Info.Name = Name;
        Info.Size = sizeof(T);
        Info.Align = alignof(T);
        Info.RuntimeOps = &GetTypeRuntimeOps<T>();
        ApplyEditorValueFamilyMetadata<T>(Info);
        (void)TypeRegistry::Instance().Register(std::move(Info));
    };
    auto RegisterEnum = []<typename T>(const char* Name,
                           const bool IsSigned,
                           const std::initializer_list<EnumValueInfo> Values) {
        TypeInfo Info;
        Info.Id = TypeIdFromName(Name);
        Info.Name = Name;
        Info.Size = sizeof(T);
        Info.Align = alignof(T);
        Info.RuntimeOps = &GetTypeRuntimeOps<T>();
        Info.IsEnum = true;
        Info.EnumIsSigned = IsSigned;
        Info.EnumValues.assign(Values.begin(), Values.end());
        (void)TypeRegistry::Instance().Register(std::move(Info));
    };

    RegisterPlain.operator()<bool>(TTypeNameV<bool>);
    RegisterPlain.operator()<int>(TTypeNameV<int>);
    RegisterPlain.operator()<std::int64_t>(TTypeNameV<std::int64_t>);
    RegisterPlain.operator()<unsigned int>(TTypeNameV<unsigned int>);
    RegisterPlain.operator()<std::uint64_t>(TTypeNameV<std::uint64_t>);
    RegisterPlain.operator()<float>(TTypeNameV<float>);
    RegisterPlain.operator()<double>(TTypeNameV<double>);
    RegisterPlain.operator()<std::string>(TTypeNameV<std::string>);
    RegisterPlain.operator()<std::vector<uint8_t>>(TTypeNameV<std::vector<uint8_t>>);
    RegisterPlain.operator()<Uuid>(TTypeNameV<Uuid>);
    RegisterPlain.operator()<TypeId>(TTypeNameV<TypeId>);
    RegisterPlain.operator()<Vec2>(TTypeNameV<Vec2>);
    RegisterPlain.operator()<Vec3>(TTypeNameV<Vec3>);
    RegisterPlain.operator()<Vec4>(TTypeNameV<Vec4>);
    RegisterPlain.operator()<Quat>(TTypeNameV<Quat>);
    RegisterPlain.operator()<NodeHandle>(TTypeNameV<NodeHandle>);
    RegisterPlain.operator()<ComponentHandle>(TTypeNameV<ComponentHandle>);
    
    
    RegisterPlain.operator()<FieldFlags>(TTypeNameV<FieldFlags>);
    RegisterPlain.operator()<FieldEditorFlags>(TTypeNameV<FieldEditorFlags>);
    
    RegisterPlain.operator()<MethodFlags>(TTypeNameV<MethodFlags>);
    
    
    
    
    
#if defined(SNAPI_GF_ENABLE_UI)
    RegisterPlain.operator()<SnAPI::UI::Color>(TTypeNameV<SnAPI::UI::Color>);
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    RegisterEnum.operator()<EMeshStreamSemantic>(
        TTypeNameV<EMeshStreamSemantic>,
        false,
        {
            EnumValueInfo{"Position", 0u},
            EnumValueInfo{"Normal", 1u},
            EnumValueInfo{"Tangent", 2u},
            EnumValueInfo{"UV0", 3u},
            EnumValueInfo{"UV1", 4u},
            EnumValueInfo{"Color", 5u},
            EnumValueInfo{"BoneIndices", 6u},
            EnumValueInfo{"BoneWeights", 7u},
            EnumValueInfo{"Index", 8u},
        });
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    
    RegisterPlain.operator()<CollisionFilterFlags>(TTypeNameV<CollisionFilterFlags>);
    RegisterEnum.operator()<SnAPI::Physics::EBodyType>(
        TTypeNameV<SnAPI::Physics::EBodyType>,
        false,
        {
            EnumValueInfo{"Static", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Static)},
            EnumValueInfo{"Kinematic", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Kinematic)},
            EnumValueInfo{"Dynamic", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Dynamic)},
        });
    RegisterEnum.operator()<SnAPI::Physics::EShapeType>(
        TTypeNameV<SnAPI::Physics::EShapeType>,
        false,
        {
            EnumValueInfo{"Sphere", static_cast<std::uint64_t>(SnAPI::Physics::EShapeType::Sphere)},
            EnumValueInfo{"Box", static_cast<std::uint64_t>(SnAPI::Physics::EShapeType::Box)},
            EnumValueInfo{"Capsule", static_cast<std::uint64_t>(SnAPI::Physics::EShapeType::Capsule)},
            EnumValueInfo{"ConvexHull", static_cast<std::uint64_t>(SnAPI::Physics::EShapeType::ConvexHull)},
            EnumValueInfo{"TriangleMesh", static_cast<std::uint64_t>(SnAPI::Physics::EShapeType::TriangleMesh)},
            EnumValueInfo{"HeightField", static_cast<std::uint64_t>(SnAPI::Physics::EShapeType::HeightField)},
        });
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
    RegisterEnum.operator()<SnAPI::Input::EInputBackend>(
        TTypeNameV<SnAPI::Input::EInputBackend>,
        false,
        {
            EnumValueInfo{"Invalid", static_cast<std::uint64_t>(SnAPI::Input::EInputBackend::Invalid)},
            EnumValueInfo{"SDL3", static_cast<std::uint64_t>(SnAPI::Input::EInputBackend::SDL3)},
            EnumValueInfo{"HIDAPI", static_cast<std::uint64_t>(SnAPI::Input::EInputBackend::HIDAPI)},
            EnumValueInfo{"LIBUSB", static_cast<std::uint64_t>(SnAPI::Input::EInputBackend::LIBUSB)},
            EnumValueInfo{"Custom0", static_cast<std::uint64_t>(SnAPI::Input::EInputBackend::Custom0)},
        });
    RegisterPlain.operator()<SnAPI::Input::EKey>(TTypeNameV<SnAPI::Input::EKey>);
    RegisterPlain.operator()<SnAPI::Input::EGamepadAxis>(TTypeNameV<SnAPI::Input::EGamepadAxis>);
    RegisterPlain.operator()<SnAPI::Input::EGamepadButton>(TTypeNameV<SnAPI::Input::EGamepadButton>);
    RegisterPlain.operator()<SnAPI::Input::DeviceId>(TTypeNameV<SnAPI::Input::DeviceId>);
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    RegisterEnum.operator()<SnAPI::Networking::ESessionRole>(
        TTypeNameV<SnAPI::Networking::ESessionRole>,
        false,
        {
            EnumValueInfo{"Client", static_cast<std::uint64_t>(SnAPI::Networking::ESessionRole::Client)},
            EnumValueInfo{"Server", static_cast<std::uint64_t>(SnAPI::Networking::ESessionRole::Server)},
            EnumValueInfo{"ServerAndClient", static_cast<std::uint64_t>(SnAPI::Networking::ESessionRole::ServerAndClient)},
        });
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    
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
