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
#include "RenderAssetRuntime.h"
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

SNAPI_REFLECT_TYPE(IAsset, (TTypeBuilder<IAsset>(IAsset::kTypeName)
    .AsInterface()
    .Register()));

SNAPI_REFLECT_TYPE(BaseNode, (TTypeBuilder<BaseNode>(BaseNode::kTypeName)
    .Field("Name", &BaseNode::Name, &BaseNode::Name)
    .Method("OnPossess", &BaseNode::OnPossess)
    .Method("OnUnpossess", &BaseNode::OnUnpossess)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Level, (TTypeBuilder<Level>(Level::kTypeName)
    .Base<BaseNode>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(World, (TTypeBuilder<World>(World::kTypeName)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(LocalPlayer, (TTypeBuilder<LocalPlayer>(LocalPlayer::kTypeName)
    .Base<BaseNode>()
    .Field("PlayerIndex",
           &LocalPlayer::EditPlayerIndex,
           &LocalPlayer::GetPlayerIndex,
           EFieldFlagBits::Replication)
    .Field("PossessedNode",
           &LocalPlayer::EditPossessedNode,
           &LocalPlayer::GetPossessedNode,
           EFieldFlagBits::Replication)
    .Field("AcceptInput",
           &LocalPlayer::EditAcceptInput,
           &LocalPlayer::GetAcceptInput,
           EFieldFlagBits::Replication)
    .Field("OwnerConnectionId",
           &LocalPlayer::EditOwnerConnectionId,
           &LocalPlayer::GetOwnerConnectionId,
           EFieldFlagBits::Replication)
#if defined(SNAPI_GF_ENABLE_INPUT)
    .Field("AssignedInputDevice",
           &LocalPlayer::EditAssignedInputDevice,
           &LocalPlayer::GetAssignedInputDevice)
    .Field("UseAssignedInputDevice",
           &LocalPlayer::EditUseAssignedInputDevice,
           &LocalPlayer::GetUseAssignedInputDevice)
#endif
    .Method("ServerRequestPossess",
            &LocalPlayer::ServerRequestPossess,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ServerRequestUnpossess",
            &LocalPlayer::ServerRequestUnpossess,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TSubClassOf<PawnBase>, (TTypeBuilder<TSubClassOf<PawnBase>>(TTypeNameV<TSubClassOf<PawnBase>>)
    .Field("TypeName",
           &TSubClassOf<PawnBase>::EditTypeName,
           &TSubClassOf<PawnBase>::GetTypeName)
    .Field("TypeId",
           &TSubClassOf<PawnBase>::EditTypeId,
           &TSubClassOf<PawnBase>::GetTypeId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<PawnBase>, (TTypeBuilder<TAssetRef<PawnBase>>(TTypeNameV<TAssetRef<PawnBase>>)
    .Field("AssetName",
           &TAssetRef<PawnBase>::EditAssetName,
           &TAssetRef<PawnBase>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<PawnBase>::EditAssetId,
           &TAssetRef<PawnBase>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::SlotId, (TTypeBuilder<Conduit::SlotId>(TTypeNameV<Conduit::SlotId>)
    .Field("Value", &Conduit::SlotId::Value)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::SerializedValue, (TTypeBuilder<Conduit::SerializedValue>(TTypeNameV<Conduit::SerializedValue>)
    .Field("Type", &Conduit::SerializedValue::Type)
    .Field("Bytes", &Conduit::SerializedValue::Bytes)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphViewportAsset, (TTypeBuilder<Conduit::GraphViewportAsset>(TTypeNameV<Conduit::GraphViewportAsset>)
    .Field("PanX", &Conduit::GraphViewportAsset::PanX)
    .Field("PanY", &Conduit::GraphViewportAsset::PanY)
    .Field("Zoom", &Conduit::GraphViewportAsset::Zoom)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphNodeEditorAsset, (TTypeBuilder<Conduit::GraphNodeEditorAsset>(TTypeNameV<Conduit::GraphNodeEditorAsset>)
    .Field("NodeId", &Conduit::GraphNodeEditorAsset::NodeId)
    .Field("X", &Conduit::GraphNodeEditorAsset::X)
    .Field("Y", &Conduit::GraphNodeEditorAsset::Y)
    .Field("Width", &Conduit::GraphNodeEditorAsset::Width)
    .Field("IsCollapsed", &Conduit::GraphNodeEditorAsset::IsCollapsed)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphCommentAsset, (TTypeBuilder<Conduit::GraphCommentAsset>(TTypeNameV<Conduit::GraphCommentAsset>)
    .Field("Id", &Conduit::GraphCommentAsset::Id)
    .Field("Title", &Conduit::GraphCommentAsset::Title)
    .Field("X", &Conduit::GraphCommentAsset::X)
    .Field("Y", &Conduit::GraphCommentAsset::Y)
    .Field("Width", &Conduit::GraphCommentAsset::Width)
    .Field("Height", &Conduit::GraphCommentAsset::Height)
    .Field("ColorRgba", &Conduit::GraphCommentAsset::ColorRgba)
    .Field("NodeIds", &Conduit::GraphCommentAsset::NodeIds)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphBookmarkAsset, (TTypeBuilder<Conduit::GraphBookmarkAsset>(TTypeNameV<Conduit::GraphBookmarkAsset>)
    .Field("Id", &Conduit::GraphBookmarkAsset::Id)
    .Field("Name", &Conduit::GraphBookmarkAsset::Name)
    .Field("PanX", &Conduit::GraphBookmarkAsset::PanX)
    .Field("PanY", &Conduit::GraphBookmarkAsset::PanY)
    .Field("Zoom", &Conduit::GraphBookmarkAsset::Zoom)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphEditorAssetState, (TTypeBuilder<Conduit::GraphEditorAssetState>(TTypeNameV<Conduit::GraphEditorAssetState>)
    .Field("Viewport", &Conduit::GraphEditorAssetState::Viewport)
    .Field("Nodes", &Conduit::GraphEditorAssetState::Nodes)
    .Field("Comments", &Conduit::GraphEditorAssetState::Comments)
    .Field("Bookmarks", &Conduit::GraphEditorAssetState::Bookmarks)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphSlotAsset, (TTypeBuilder<Conduit::GraphSlotAsset>(TTypeNameV<Conduit::GraphSlotAsset>)
    .Field("Name", &Conduit::GraphSlotAsset::Name)
    .Field("Type", &Conduit::GraphSlotAsset::Type)
    .Field("Kind", &Conduit::GraphSlotAsset::Kind)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphVariableAsset, (TTypeBuilder<Conduit::GraphVariableAsset>(TTypeNameV<Conduit::GraphVariableAsset>)
    .Field("Id", &Conduit::GraphVariableAsset::Id)
    .Field("Name", &Conduit::GraphVariableAsset::Name)
    .Field("Type", &Conduit::GraphVariableAsset::Type)
    .Field("DefaultValue", &Conduit::GraphVariableAsset::DefaultValue)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphNodeAsset, (TTypeBuilder<Conduit::GraphNodeAsset>(TTypeNameV<Conduit::GraphNodeAsset>)
    .Field("Id", &Conduit::GraphNodeAsset::Id)
    .Field("Kind", &Conduit::GraphNodeAsset::Kind)
    .Field("BuiltinEntryPoint", &Conduit::GraphNodeAsset::BuiltinEntryPoint)
    .Field("EntryPointName", &Conduit::GraphNodeAsset::EntryPointName)
    .Field("VariableId", &Conduit::GraphNodeAsset::VariableId)
    .Field("LabelName", &Conduit::GraphNodeAsset::LabelName)
    .Field("FalseLabelName", &Conduit::GraphNodeAsset::FalseLabelName)
    .Field("MemberName", &Conduit::GraphNodeAsset::MemberName)
    .Field("ConstantValue", &Conduit::GraphNodeAsset::ConstantValue)
    .Field("UnaryOp", &Conduit::GraphNodeAsset::UnaryOp)
    .Field("BinaryOp", &Conduit::GraphNodeAsset::BinaryOp)
    .Field("Input", &Conduit::GraphNodeAsset::Input)
    .Field("Left", &Conduit::GraphNodeAsset::Left)
    .Field("Right", &Conduit::GraphNodeAsset::Right)
    .Field("Output", &Conduit::GraphNodeAsset::Output)
    .Field("Condition", &Conduit::GraphNodeAsset::Condition)
    .Field("Instance", &Conduit::GraphNodeAsset::Instance)
    .Field("ReturnSlot", &Conduit::GraphNodeAsset::ReturnSlot)
    .Field("OwnerType", &Conduit::GraphNodeAsset::OwnerType)
    .Field("Inputs", &Conduit::GraphNodeAsset::Inputs)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::GraphAsset, (TTypeBuilder<Conduit::GraphAsset>(TTypeNameV<Conduit::GraphAsset>)
    .Interface<IAsset>()
    .Field("Name", &Conduit::GraphAsset::Name)
    .Field("SelfType", &Conduit::GraphAsset::SelfType)
    .Field("Slots", &Conduit::GraphAsset::Slots)
    .Field("Variables", &Conduit::GraphAsset::Variables)
    .Field("Nodes", &Conduit::GraphAsset::Nodes)
    .Field("EditorState", &Conduit::GraphAsset::EditorState)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Conduit::ClassAsset, (TTypeBuilder<Conduit::ClassAsset>(TTypeNameV<Conduit::ClassAsset>)
    .Interface<IAsset>()
    .Field("Name", &Conduit::ClassAsset::Name)
    .Field("HostType", &Conduit::ClassAsset::HostType)
    .Field("Graph", &Conduit::ClassAsset::Graph)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(NodeFieldAsset, (TTypeBuilder<NodeFieldAsset>(TTypeNameV<NodeFieldAsset>)
    .Field("Name", &NodeFieldAsset::Name)
    .Field("Value", &NodeFieldAsset::Value)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(NodeComponentAsset, (TTypeBuilder<NodeComponentAsset>(TTypeNameV<NodeComponentAsset>)
    .Field("Id", &NodeComponentAsset::Id)
    .Field("Type", &NodeComponentAsset::Type)
    .Field("Fields", &NodeComponentAsset::Fields)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(NodeObjectAsset, (TTypeBuilder<NodeObjectAsset>(TTypeNameV<NodeObjectAsset>)
    .Field("Id", &NodeObjectAsset::Id)
    .Field("Type", &NodeObjectAsset::Type)
    .Field("Name", &NodeObjectAsset::Name)
    .Field("Active", &NodeObjectAsset::Active)
    .Field("Fields", &NodeObjectAsset::Fields)
    .Field("Components", &NodeObjectAsset::Components)
    .Field("Children", &NodeObjectAsset::Children)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(NodeAsset, (TTypeBuilder<NodeAsset>(TTypeNameV<NodeAsset>)
    .Interface<IAsset>()
    .Field("Name", &NodeAsset::Name)
    .Field("Nodes", &NodeAsset::Nodes)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(LevelAsset, (TTypeBuilder<LevelAsset>(TTypeNameV<LevelAsset>)
    .Base<NodeAsset>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(WorldAsset, (TTypeBuilder<WorldAsset>(TTypeNameV<WorldAsset>)
    .Base<NodeAsset>()
    .Constructor<>()
    .Register()));

SNAPI_DEFINE_TYPE_NAME(TAssetRef<Conduit::GraphAsset>,
                       "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::Conduit::GraphAsset>")
SNAPI_REFLECT_TYPE(TAssetRef<Conduit::GraphAsset>, (TTypeBuilder<TAssetRef<Conduit::GraphAsset>>(TTypeNameV<TAssetRef<Conduit::GraphAsset>>)
    .Field("AssetName",
           &TAssetRef<Conduit::GraphAsset>::EditAssetName,
           &TAssetRef<Conduit::GraphAsset>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<Conduit::GraphAsset>::EditAssetId,
           &TAssetRef<Conduit::GraphAsset>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_DEFINE_TYPE_NAME(TAssetRef<Conduit::ClassAsset>,
                       "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::Conduit::ClassAsset>")
SNAPI_REFLECT_TYPE(TAssetRef<Conduit::ClassAsset>, (TTypeBuilder<TAssetRef<Conduit::ClassAsset>>(TTypeNameV<TAssetRef<Conduit::ClassAsset>>)
    .Field("AssetName",
           &TAssetRef<Conduit::ClassAsset>::EditAssetName,
           &TAssetRef<Conduit::ClassAsset>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<Conduit::ClassAsset>::EditAssetId,
           &TAssetRef<Conduit::ClassAsset>::GetAssetId)
    .Constructor<>()
    .Register()));

#if defined(SNAPI_GF_ENABLE_RENDERER)
SNAPI_REFLECT_TYPE(TAssetRef<StaticMeshAssetRuntime>, (TTypeBuilder<TAssetRef<StaticMeshAssetRuntime>>(TTypeNameV<TAssetRef<StaticMeshAssetRuntime>>)
    .Field("AssetName",
           &TAssetRef<StaticMeshAssetRuntime>::EditAssetName,
           &TAssetRef<StaticMeshAssetRuntime>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<StaticMeshAssetRuntime>::EditAssetId,
           &TAssetRef<StaticMeshAssetRuntime>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<SkeletalMeshAssetRuntime>, (TTypeBuilder<TAssetRef<SkeletalMeshAssetRuntime>>(TTypeNameV<TAssetRef<SkeletalMeshAssetRuntime>>)
    .Field("AssetName",
           &TAssetRef<SkeletalMeshAssetRuntime>::EditAssetName,
           &TAssetRef<SkeletalMeshAssetRuntime>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<SkeletalMeshAssetRuntime>::EditAssetId,
           &TAssetRef<SkeletalMeshAssetRuntime>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<SSAOParamsNode>, (TTypeBuilder<TAssetRef<SSAOParamsNode>>(TTypeNameV<TAssetRef<SSAOParamsNode>>)
    .Field("AssetName",
           &TAssetRef<SSAOParamsNode>::EditAssetName,
           &TAssetRef<SSAOParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<SSAOParamsNode>::EditAssetId,
           &TAssetRef<SSAOParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<SSGIParamsNode>, (TTypeBuilder<TAssetRef<SSGIParamsNode>>(TTypeNameV<TAssetRef<SSGIParamsNode>>)
    .Field("AssetName",
           &TAssetRef<SSGIParamsNode>::EditAssetName,
           &TAssetRef<SSGIParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<SSGIParamsNode>::EditAssetId,
           &TAssetRef<SSGIParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<SSRParamsNode>, (TTypeBuilder<TAssetRef<SSRParamsNode>>(TTypeNameV<TAssetRef<SSRParamsNode>>)
    .Field("AssetName",
           &TAssetRef<SSRParamsNode>::EditAssetName,
           &TAssetRef<SSRParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<SSRParamsNode>::EditAssetId,
           &TAssetRef<SSRParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<TAAParamsNode>, (TTypeBuilder<TAssetRef<TAAParamsNode>>(TTypeNameV<TAssetRef<TAAParamsNode>>)
    .Field("AssetName",
           &TAssetRef<TAAParamsNode>::EditAssetName,
           &TAssetRef<TAAParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<TAAParamsNode>::EditAssetId,
           &TAssetRef<TAAParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<BloomParamsNode>, (TTypeBuilder<TAssetRef<BloomParamsNode>>(TTypeNameV<TAssetRef<BloomParamsNode>>)
    .Field("AssetName",
           &TAssetRef<BloomParamsNode>::EditAssetName,
           &TAssetRef<BloomParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<BloomParamsNode>::EditAssetId,
           &TAssetRef<BloomParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<AtmosphereParamsNode>, (TTypeBuilder<TAssetRef<AtmosphereParamsNode>>(TTypeNameV<TAssetRef<AtmosphereParamsNode>>)
    .Field("AssetName",
           &TAssetRef<AtmosphereParamsNode>::EditAssetName,
           &TAssetRef<AtmosphereParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<AtmosphereParamsNode>::EditAssetId,
           &TAssetRef<AtmosphereParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<AtmosphereCompositeParamsNode>, (TTypeBuilder<TAssetRef<AtmosphereCompositeParamsNode>>(TTypeNameV<TAssetRef<AtmosphereCompositeParamsNode>>)
    .Field("AssetName",
           &TAssetRef<AtmosphereCompositeParamsNode>::EditAssetName,
           &TAssetRef<AtmosphereCompositeParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<AtmosphereCompositeParamsNode>::EditAssetId,
           &TAssetRef<AtmosphereCompositeParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<HeightFogParamsNode>, (TTypeBuilder<TAssetRef<HeightFogParamsNode>>(TTypeNameV<TAssetRef<HeightFogParamsNode>>)
    .Field("AssetName",
           &TAssetRef<HeightFogParamsNode>::EditAssetName,
           &TAssetRef<HeightFogParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<HeightFogParamsNode>::EditAssetId,
           &TAssetRef<HeightFogParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<ToneMapParamsNode>, (TTypeBuilder<TAssetRef<ToneMapParamsNode>>(TTypeNameV<TAssetRef<ToneMapParamsNode>>)
    .Field("AssetName",
           &TAssetRef<ToneMapParamsNode>::EditAssetName,
           &TAssetRef<ToneMapParamsNode>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<ToneMapParamsNode>::EditAssetId,
           &TAssetRef<ToneMapParamsNode>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAssetRef<WorldRenderSettings>, (TTypeBuilder<TAssetRef<WorldRenderSettings>>(TTypeNameV<TAssetRef<WorldRenderSettings>>)
    .Field("AssetName",
           &TAssetRef<WorldRenderSettings>::EditAssetName,
           &TAssetRef<WorldRenderSettings>::GetAssetName)
    .Field("AssetId",
           &TAssetRef<WorldRenderSettings>::EditAssetId,
           &TAssetRef<WorldRenderSettings>::GetAssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AssetRefPayload, (TTypeBuilder<AssetRefPayload>(TTypeNameV<AssetRefPayload>)
    .Field("AssetName", &AssetRefPayload::AssetName)
    .Field("AssetId", &AssetRefPayload::AssetId)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(MaterialScalarParamPayload, (TTypeBuilder<MaterialScalarParamPayload>(TTypeNameV<MaterialScalarParamPayload>)
    .Field("Name", &MaterialScalarParamPayload::Name)
    .Field("Value", &MaterialScalarParamPayload::Value)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(MaterialVectorParamPayload, (TTypeBuilder<MaterialVectorParamPayload>(TTypeNameV<MaterialVectorParamPayload>)
    .Field("Name", &MaterialVectorParamPayload::Name)
    .Field("Value", &MaterialVectorParamPayload::Value)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(MaterialTextureParamPayload, (TTypeBuilder<MaterialTextureParamPayload>(TTypeNameV<MaterialTextureParamPayload>)
    .Field("SlotName", &MaterialTextureParamPayload::SlotName)
    .Field("Texture", &MaterialTextureParamPayload::Texture)
    .Field("SRGB", &MaterialTextureParamPayload::SRGB)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(MaterialPayload, (TTypeBuilder<MaterialPayload>(TTypeNameV<MaterialPayload>)
    .Interface<IAsset>()
    .Field("ShaderModule", &MaterialPayload::ShaderModule)
    .Field("ShadingModel", &MaterialPayload::ShadingModel)
    .Field("FeatureAlbedoMap", &MaterialPayload::FeatureAlbedoMap)
    .Field("FeatureNormalMap", &MaterialPayload::FeatureNormalMap)
    .Field("FeatureRoughnessMap", &MaterialPayload::FeatureRoughnessMap)
    .Field("FeatureMetalnessMap", &MaterialPayload::FeatureMetalnessMap)
    .Field("FeatureOcclusionMap", &MaterialPayload::FeatureOcclusionMap)
    .Field("FeatureAlphaTest", &MaterialPayload::FeatureAlphaTest)
    .Field("FeatureAlphaBlend", &MaterialPayload::FeatureAlphaBlend)
    .Field("FeatureDoubleSided", &MaterialPayload::FeatureDoubleSided)
    .Field("FeatureInstancing", &MaterialPayload::FeatureInstancing)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(MaterialInstancePayload, (TTypeBuilder<MaterialInstancePayload>(TTypeNameV<MaterialInstancePayload>)
    .Interface<IAsset>()
    .Field("ParentMaterial", &MaterialInstancePayload::ParentMaterial)
    .Field("Scalars", &MaterialInstancePayload::Scalars)
    .Field("Vectors", &MaterialInstancePayload::Vectors)
    .Field("Textures", &MaterialInstancePayload::Textures)
    .Constructor<>()
    .Register()));
#endif

SNAPI_REFLECT_TYPE(PawnBase, (TTypeBuilder<PawnBase>(PawnBase::kTypeName)
    .Base<BaseNode>()
    .Method("OnPossess", &PawnBase::OnPossess)
    .Method("OnUnpossess", &PawnBase::OnUnpossess)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(PlayerStart, (TTypeBuilder<PlayerStart>(PlayerStart::kTypeName)
    .Base<BaseNode>()
    .Field("SpawnPawnAsset",
           &PlayerStart::EditSpawnPawnAsset,
           &PlayerStart::GetSpawnPawnAsset,
           EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

#if defined(SNAPI_GF_ENABLE_RENDERER)
SNAPI_REFLECT_TYPE(EnvironmentProbeNode, (TTypeBuilder<EnvironmentProbeNode>(EnvironmentProbeNode::kTypeName)
    .Base<BaseNode>()
    .Constructor<>()
    .Register()));
#endif

SNAPI_REFLECT_TYPE(MultiplayerConfigNode, (TTypeBuilder<MultiplayerConfigNode>(MultiplayerConfigNode::kTypeName)
    .Base<BaseNode>()
    .Field("LocalPlayerCount",
           &MultiplayerConfigNode::EditLocalPlayerCount,
           &MultiplayerConfigNode::GetLocalPlayerCount)
    .Field("Splitscreen",
           &MultiplayerConfigNode::EditSplitscreen,
           &MultiplayerConfigNode::GetSplitscreen)
    .Field("AutoJoinAdditionalLocalPlayers",
           &MultiplayerConfigNode::EditAutoJoinAdditionalLocalPlayers,
           &MultiplayerConfigNode::GetAutoJoinAdditionalLocalPlayers)
    .Field("RequireGamepadForAdditionalPlayers",
           &MultiplayerConfigNode::EditRequireGamepadForAdditionalPlayers,
           &MultiplayerConfigNode::GetRequireGamepadForAdditionalPlayers)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(GameplayRpcGateway, (TTypeBuilder<GameplayRpcGateway>(GameplayRpcGateway::kTypeName)
    .Base<BaseNode>()
    .Method("ServerRequestJoinPlayer",
            &GameplayRpcGateway::ServerRequestJoinPlayer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ServerRequestLeavePlayer",
            &GameplayRpcGateway::ServerRequestLeavePlayer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ServerRequestLoadLevel",
            &GameplayRpcGateway::ServerRequestLoadLevel,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("ServerRequestUnloadLevel",
            &GameplayRpcGateway::ServerRequestUnloadLevel,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Constructor<>()
    .Register()));

#if defined(SNAPI_GF_ENABLE_RENDERER)

SNAPI_REFLECT_TYPE(SSAOParamsNode, (TTypeBuilder<SSAOParamsNode>(SSAOParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &SSAOParamsNode::EditViewportID,
           &SSAOParamsNode::GetViewportID)
    .Field("Radius",
           &SSAOParamsNode::EditRadius,
           &SSAOParamsNode::GetRadius)
    .Field("Bias",
           &SSAOParamsNode::EditBias,
           &SSAOParamsNode::GetBias)
    .Field("Intensity",
           &SSAOParamsNode::EditIntensity,
           &SSAOParamsNode::GetIntensity)
    .Field("MaxDistance",
           &SSAOParamsNode::EditMaxDistance,
           &SSAOParamsNode::GetMaxDistance)
    .Field("SliceCount",
           &SSAOParamsNode::EditSliceCount,
           &SSAOParamsNode::GetSliceCount)
    .Field("StepsPerSlice",
           &SSAOParamsNode::EditStepsPerSlice,
           &SSAOParamsNode::GetStepsPerSlice)
    .Field("FalloffStart",
           &SSAOParamsNode::EditFalloffStart,
           &SSAOParamsNode::GetFalloffStart)
    .Field("FalloffEnd",
           &SSAOParamsNode::EditFalloffEnd,
           &SSAOParamsNode::GetFalloffEnd)
    .Field("MaxPixelRadius",
           &SSAOParamsNode::EditMaxPixelRadius,
           &SSAOParamsNode::GetMaxPixelRadius)
    .Field("Thickness",
           &SSAOParamsNode::EditThickness,
           &SSAOParamsNode::GetThickness)
    .Field("DenoiseBlurBeta",
           &SSAOParamsNode::EditDenoiseBlurBeta,
           &SSAOParamsNode::GetDenoiseBlurBeta)
    .Field("TemporalBlendFactor",
           &SSAOParamsNode::EditTemporalBlendFactor,
           &SSAOParamsNode::GetTemporalBlendFactor)
    .Field("DisocclusionThreshold",
           &SSAOParamsNode::EditDisocclusionThreshold,
           &SSAOParamsNode::GetDisocclusionThreshold)
    .Field("VelocityWeight",
           &SSAOParamsNode::EditVelocityWeight,
           &SSAOParamsNode::GetVelocityWeight)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SSGIParamsNode, (TTypeBuilder<SSGIParamsNode>(SSGIParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &SSGIParamsNode::EditViewportID,
           &SSGIParamsNode::GetViewportID)
    .Field("Intensity",
           &SSGIParamsNode::EditIntensity,
           &SSGIParamsNode::GetIntensity)
    .Field("MaxDistance",
           &SSGIParamsNode::EditMaxDistance,
           &SSGIParamsNode::GetMaxDistance)
    .Field("Thickness",
           &SSGIParamsNode::EditThickness,
           &SSGIParamsNode::GetThickness)
    .Field("SurfaceBias",
           &SSGIParamsNode::EditSurfaceBias,
           &SSGIParamsNode::GetSurfaceBias)
    .Field("MaxSteps",
           &SSGIParamsNode::EditMaxSteps,
           &SSGIParamsNode::GetMaxSteps)
    .Field("RayCount",
           &SSGIParamsNode::EditRayCount,
           &SSGIParamsNode::GetRayCount)
    .Field("DepthSigma",
           &SSGIParamsNode::EditDepthSigma,
           &SSGIParamsNode::GetDepthSigma)
    .Field("NormalSigma",
           &SSGIParamsNode::EditNormalSigma,
           &SSGIParamsNode::GetNormalSigma)
    .Field("RadianceClamp",
           &SSGIParamsNode::EditRadianceClamp,
           &SSGIParamsNode::GetRadianceClamp)
    .Field("MaxPixelRadius",
           &SSGIParamsNode::EditMaxPixelRadius,
           &SSGIParamsNode::GetMaxPixelRadius)
    .Field("StepExponent",
           &SSGIParamsNode::EditStepExponent,
           &SSGIParamsNode::GetStepExponent)
    .Field("TemporalBlendFactor",
           &SSGIParamsNode::EditTemporalBlendFactor,
           &SSGIParamsNode::GetTemporalBlendFactor)
    .Field("DisocclusionThreshold",
           &SSGIParamsNode::EditDisocclusionThreshold,
           &SSGIParamsNode::GetDisocclusionThreshold)
    .Field("ClampStrength",
           &SSGIParamsNode::EditClampStrength,
           &SSGIParamsNode::GetClampStrength)
    .Field("VelocityWeight",
           &SSGIParamsNode::EditVelocityWeight,
           &SSGIParamsNode::GetVelocityWeight)
    .Field("LowLumaBoost",
           &SSGIParamsNode::EditLowLumaBoost,
           &SSGIParamsNode::GetLowLumaBoost)
    .Field("TemporalDebugMode",
           &SSGIParamsNode::EditTemporalDebugMode,
           &SSGIParamsNode::GetTemporalDebugMode)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SSRParamsNode, (TTypeBuilder<SSRParamsNode>(SSRParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &SSRParamsNode::EditViewportID,
           &SSRParamsNode::GetViewportID)
    .Field("MaxDistance",
           &SSRParamsNode::EditMaxDistance,
           &SSRParamsNode::GetMaxDistance)
    .Field("Thickness",
           &SSRParamsNode::EditThickness,
           &SSRParamsNode::GetThickness)
    .Field("MaxRoughness",
           &SSRParamsNode::EditMaxRoughness,
           &SSRParamsNode::GetMaxRoughness)
    .Field("RoughnessThreshold",
           &SSRParamsNode::EditRoughnessThreshold,
           &SSRParamsNode::GetRoughnessThreshold)
    .Field("MaxSteps",
           &SSRParamsNode::EditMaxSteps,
           &SSRParamsNode::GetMaxSteps)
    .Field("MaxBinarySteps",
           &SSRParamsNode::EditMaxBinarySteps,
           &SSRParamsNode::GetMaxBinarySteps)
    .Field("ScreenEdgeFade",
           &SSRParamsNode::EditScreenEdgeFade,
           &SSRParamsNode::GetScreenEdgeFade)
    .Field("ReflectionFade",
           &SSRParamsNode::EditReflectionFade,
           &SSRParamsNode::GetReflectionFade)
    .Field("TemporalBlendFactor",
           &SSRParamsNode::EditTemporalBlendFactor,
           &SSRParamsNode::GetTemporalBlendFactor)
    .Field("ClampStrength",
           &SSRParamsNode::EditClampStrength,
           &SSRParamsNode::GetClampStrength)
    .Field("MotionHistoryReset",
           &SSRParamsNode::EditMotionHistoryReset,
           &SSRParamsNode::GetMotionHistoryReset)
    .Field("TemporalDebugMode",
           &SSRParamsNode::EditTemporalDebugMode,
           &SSRParamsNode::GetTemporalDebugMode)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TAAParamsNode, (TTypeBuilder<TAAParamsNode>(TAAParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &TAAParamsNode::EditViewportID,
           &TAAParamsNode::GetViewportID)
    .Field("BlendFactor",
           &TAAParamsNode::EditBlendFactor,
           &TAAParamsNode::GetBlendFactor)
    .Field("MotionBlendFactor",
           &TAAParamsNode::EditMotionBlendFactor,
           &TAAParamsNode::GetMotionBlendFactor)
    .Field("ClampStrength",
           &TAAParamsNode::EditClampStrength,
           &TAAParamsNode::GetClampStrength)
    .Field("Sharpen",
           &TAAParamsNode::EditSharpen,
           &TAAParamsNode::GetSharpen)
    .Field("JitterScale",
           &TAAParamsNode::EditJitterScale,
           &TAAParamsNode::GetJitterScale)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(BloomParamsNode, (TTypeBuilder<BloomParamsNode>(BloomParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &BloomParamsNode::EditViewportID,
           &BloomParamsNode::GetViewportID)
    .Field("Threshold",
           &BloomParamsNode::EditThreshold,
           &BloomParamsNode::GetThreshold)
    .Field("Knee",
           &BloomParamsNode::EditKnee,
           &BloomParamsNode::GetKnee)
    .Field("Intensity",
           &BloomParamsNode::EditIntensity,
           &BloomParamsNode::GetIntensity)
    .Field("Scatter",
           &BloomParamsNode::EditScatter,
           &BloomParamsNode::GetScatter)
    .Field("Clamp",
           &BloomParamsNode::EditClamp,
           &BloomParamsNode::GetClamp)
    .Field("MipCount",
           &BloomParamsNode::EditMipCount,
           &BloomParamsNode::GetMipCount)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AtmosphereParamsNode, (TTypeBuilder<AtmosphereParamsNode>(AtmosphereParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &AtmosphereParamsNode::EditViewportID,
           &AtmosphereParamsNode::GetViewportID)
    .Field("WorldMode",
           &AtmosphereParamsNode::EditWorldMode,
           &AtmosphereParamsNode::GetWorldMode)
    .Field("SunDirection",
           &AtmosphereParamsNode::EditSunDirection,
           &AtmosphereParamsNode::GetSunDirection)
    .Field("SunColor",
           &AtmosphereParamsNode::EditSunColor,
           &AtmosphereParamsNode::GetSunColor)
    .Field("Exposure",
           &AtmosphereParamsNode::EditExposure,
           &AtmosphereParamsNode::GetExposure)
    .Field("SunIntensity",
           &AtmosphereParamsNode::EditSunIntensity,
           &AtmosphereParamsNode::GetSunIntensity)
    .Field("RayleighScattering",
           &AtmosphereParamsNode::EditRayleighScattering,
           &AtmosphereParamsNode::GetRayleighScattering)
    .Field("RayleighScaleHeight",
           &AtmosphereParamsNode::EditRayleighScaleHeight,
           &AtmosphereParamsNode::GetRayleighScaleHeight)
    .Field("MieScattering",
           &AtmosphereParamsNode::EditMieScattering,
           &AtmosphereParamsNode::GetMieScattering)
    .Field("MieScaleHeight",
           &AtmosphereParamsNode::EditMieScaleHeight,
           &AtmosphereParamsNode::GetMieScaleHeight)
    .Field("MieAbsorption",
           &AtmosphereParamsNode::EditMieAbsorption,
           &AtmosphereParamsNode::GetMieAbsorption)
    .Field("MieAnisotropyG",
           &AtmosphereParamsNode::EditMieAnisotropyG,
           &AtmosphereParamsNode::GetMieAnisotropyG)
    .Field("PlanetRadiusMeters",
           &AtmosphereParamsNode::EditPlanetRadiusMeters,
           &AtmosphereParamsNode::GetPlanetRadiusMeters)
    .Field("AtmosphereRadiusMeters",
           &AtmosphereParamsNode::EditAtmosphereRadiusMeters,
           &AtmosphereParamsNode::GetAtmosphereRadiusMeters)
    .Field("CameraGroundOffsetMeters",
           &AtmosphereParamsNode::EditCameraGroundOffsetMeters,
           &AtmosphereParamsNode::GetCameraGroundOffsetMeters)
    .Field("MaxSunDistanceMeters",
           &AtmosphereParamsNode::EditMaxSunDistanceMeters,
           &AtmosphereParamsNode::GetMaxSunDistanceMeters)
    .Field("ViewSampleCount",
           &AtmosphereParamsNode::EditViewSampleCount,
           &AtmosphereParamsNode::GetViewSampleCount)
    .Field("SunSampleCount",
           &AtmosphereParamsNode::EditSunSampleCount,
           &AtmosphereParamsNode::GetSunSampleCount)
    .Field("MultiScatterStrength",
           &AtmosphereParamsNode::EditMultiScatterStrength,
           &AtmosphereParamsNode::GetMultiScatterStrength)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AtmosphereCompositeParamsNode, (TTypeBuilder<AtmosphereCompositeParamsNode>(AtmosphereCompositeParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &AtmosphereCompositeParamsNode::EditViewportID,
           &AtmosphereCompositeParamsNode::GetViewportID)
    .Field("DepthThreshold",
           &AtmosphereCompositeParamsNode::EditDepthThreshold,
           &AtmosphereCompositeParamsNode::GetDepthThreshold)
    .Field("BlendWhenGeometry",
           &AtmosphereCompositeParamsNode::EditBlendWhenGeometry,
           &AtmosphereCompositeParamsNode::GetBlendWhenGeometry)
    .Field("BlendWhenSky",
           &AtmosphereCompositeParamsNode::EditBlendWhenSky,
           &AtmosphereCompositeParamsNode::GetBlendWhenSky)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(ToneMapParamsNode, (TTypeBuilder<ToneMapParamsNode>(ToneMapParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &ToneMapParamsNode::EditViewportID,
           &ToneMapParamsNode::GetViewportID)
    .Field("Exposure",
           &ToneMapParamsNode::EditExposure,
           &ToneMapParamsNode::GetExposure)
    .Field("Gamma",
           &ToneMapParamsNode::EditGamma,
           &ToneMapParamsNode::GetGamma)
    .Field("DitherStrength",
           &ToneMapParamsNode::EditDitherStrength,
           &ToneMapParamsNode::GetDitherStrength)
    .Field("AgXExposureBiasStops",
           &ToneMapParamsNode::EditAgXExposureBiasStops,
           &ToneMapParamsNode::GetAgXExposureBiasStops)
    .Field("AgXSaturation",
           &ToneMapParamsNode::EditAgXSaturation,
           &ToneMapParamsNode::GetAgXSaturation)
    .Field("AgXContrast",
           &ToneMapParamsNode::EditAgXContrast,
           &ToneMapParamsNode::GetAgXContrast)
    .Field("AgXPivot",
           &ToneMapParamsNode::EditAgXPivot,
           &ToneMapParamsNode::GetAgXPivot)
    .Field("AgXGamutThreshold",
           &ToneMapParamsNode::EditAgXGamutThreshold,
           &ToneMapParamsNode::GetAgXGamutThreshold)
    .Field("AgXGamutKnee",
           &ToneMapParamsNode::EditAgXGamutKnee,
           &ToneMapParamsNode::GetAgXGamutKnee)
    .Field("AcesSaturation",
           &ToneMapParamsNode::EditAcesSaturation,
           &ToneMapParamsNode::GetAcesSaturation)
    .Field("AcesWhitePoint",
           &ToneMapParamsNode::EditAcesWhitePoint,
           &ToneMapParamsNode::GetAcesWhitePoint)
    .Field("EnableACES",
           &ToneMapParamsNode::EditEnableACES,
           &ToneMapParamsNode::GetEnableACES)
    .Field("EnableAgX",
           &ToneMapParamsNode::EditEnableAgX,
           &ToneMapParamsNode::GetEnableAgX)
    .Field("EnableCompare",
           &ToneMapParamsNode::EditEnableCompare,
           &ToneMapParamsNode::GetEnableCompare)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(HeightFogParamsNode, (TTypeBuilder<HeightFogParamsNode>(HeightFogParamsNode::kTypeName)
    .Base<BaseNode>()
    .Field("ViewportID",
           &HeightFogParamsNode::EditViewportID,
           &HeightFogParamsNode::GetViewportID)
    .Field("Density",
           &HeightFogParamsNode::EditDensity,
           &HeightFogParamsNode::GetDensity)
    .Field("HeightFalloff",
           &HeightFogParamsNode::EditHeightFalloff,
           &HeightFogParamsNode::GetHeightFalloff)
    .Field("UseAbsoluteHeight",
           &HeightFogParamsNode::EditUseAbsoluteHeight,
           &HeightFogParamsNode::GetUseAbsoluteHeight)
    .Field("HeightOffsetAbsoluteY",
           &HeightFogParamsNode::EditHeightOffsetAbsoluteY,
           &HeightFogParamsNode::GetHeightOffsetAbsoluteY)
    .Field("UseActiveCameraYAsRebaseOrigin",
           &HeightFogParamsNode::EditUseActiveCameraYAsRebaseOrigin,
           &HeightFogParamsNode::GetUseActiveCameraYAsRebaseOrigin)
    .Field("RebaseOriginAbsoluteY",
           &HeightFogParamsNode::EditRebaseOriginAbsoluteY,
           &HeightFogParamsNode::GetRebaseOriginAbsoluteY)
    .Field("HeightOffsetRebased",
           &HeightFogParamsNode::EditHeightOffsetRebased,
           &HeightFogParamsNode::GetHeightOffsetRebased)
    .Field("StartDistance",
           &HeightFogParamsNode::EditStartDistance,
           &HeightFogParamsNode::GetStartDistance)
    .Field("FogColor",
           &HeightFogParamsNode::EditFogColor,
           &HeightFogParamsNode::GetFogColor)
    .Field("HorizonColor",
           &HeightFogParamsNode::EditHorizonColor,
           &HeightFogParamsNode::GetHorizonColor)
    .Field("ZenithColor",
           &HeightFogParamsNode::EditZenithColor,
           &HeightFogParamsNode::GetZenithColor)
    .Field("SkyBlendStartDistance",
           &HeightFogParamsNode::EditSkyBlendStartDistance,
           &HeightFogParamsNode::GetSkyBlendStartDistance)
    .Field("SkyBlendEndDistance",
           &HeightFogParamsNode::EditSkyBlendEndDistance,
           &HeightFogParamsNode::GetSkyBlendEndDistance)
    .Field("SkyBlendStrength",
           &HeightFogParamsNode::EditSkyBlendStrength,
           &HeightFogParamsNode::GetSkyBlendStrength)
    .Field("TauDitherAmplitude",
           &HeightFogParamsNode::EditTauDitherAmplitude,
           &HeightFogParamsNode::GetTauDitherAmplitude)
    .Field("SunDirection",
           &HeightFogParamsNode::EditSunDirection,
           &HeightFogParamsNode::GetSunDirection)
    .Field("SunAnisotropyG",
           &HeightFogParamsNode::EditSunAnisotropyG,
           &HeightFogParamsNode::GetSunAnisotropyG)
    .Field("SunColor",
           &HeightFogParamsNode::EditSunColor,
           &HeightFogParamsNode::GetSunColor)
    .Field("SunInscatterIntensity",
           &HeightFogParamsNode::EditSunInscatterIntensity,
           &HeightFogParamsNode::GetSunInscatterIntensity)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(WorldRenderSettings, (TTypeBuilder<WorldRenderSettings>(WorldRenderSettings::kTypeName)
    .Base<BaseNode>()
    .Field("SSAOParams",
           &WorldRenderSettings::EditSSAOParams,
           &WorldRenderSettings::GetSSAOParams)
    .Field("SSGIParams",
           &WorldRenderSettings::EditSSGIParams,
           &WorldRenderSettings::GetSSGIParams)
    .Field("SSRParams",
           &WorldRenderSettings::EditSSRParams,
           &WorldRenderSettings::GetSSRParams)
    .Field("TAAParams",
           &WorldRenderSettings::EditTAAParams,
           &WorldRenderSettings::GetTAAParams)
    .Field("BloomParams",
           &WorldRenderSettings::EditBloomParams,
           &WorldRenderSettings::GetBloomParams)
    .Field("AtmosphereParams",
           &WorldRenderSettings::EditAtmosphereParams,
           &WorldRenderSettings::GetAtmosphereParams)
    .Field("AtmosphereCompositeParams",
           &WorldRenderSettings::EditAtmosphereCompositeParams,
           &WorldRenderSettings::GetAtmosphereCompositeParams)
    .Field("HeightFogParams",
           &WorldRenderSettings::EditHeightFogParams,
           &WorldRenderSettings::GetHeightFogParams)
    .Field("ToneMapParams",
           &WorldRenderSettings::EditToneMapParams,
           &WorldRenderSettings::GetToneMapParams)
    .Constructor<>()
    .Register()));

#endif // SNAPI_GF_ENABLE_RENDERER

SNAPI_REFLECT_TYPE(TransformComponent, (TTypeBuilder<TransformComponent>(TransformComponent::kTypeName)
    .Field("Position", &TransformComponent::Position, EFieldFlagBits::Replication)
    .Field("Rotation", &TransformComponent::Rotation, EFieldFlagBits::Replication)
    .Field("Scale", &TransformComponent::Scale, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(InputIntentComponent, (TTypeBuilder<InputIntentComponent>(InputIntentComponent::kTypeName)
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

SNAPI_REFLECT_TYPE(Conduit::ClassComponent, (TTypeBuilder<Conduit::ClassComponent>(Conduit::ClassComponent::kTypeName)
    .Field("Class", &Conduit::ClassComponent::Class)
    .Field("Bound", &Conduit::ClassComponent::IsBound)
    .Field("LastError", &Conduit::ClassComponent::LastError)
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
    .Field("KeepUpright", &CharacterMovementController::Settings::KeepUpright)
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
    .Field("LookEnabled", &InputComponent::Settings::LookEnabled)
    .Field("MouseLookEnabled", &InputComponent::Settings::MouseLookEnabled)
    .Field("GamepadLookEnabled", &InputComponent::Settings::GamepadLookEnabled)
    .Field("RequireRightMouseButtonForLook", &InputComponent::Settings::RequireRightMouseButtonForLook)
    .Field("MoveScale", &InputComponent::Settings::MoveScale)
    .Field("GamepadDeadzone", &InputComponent::Settings::GamepadDeadzone)
    .Field("InvertGamepadY", &InputComponent::Settings::InvertGamepadY)
    .Field("MouseLookSensitivity", &InputComponent::Settings::MouseLookSensitivity)
    .Field("InvertMouseY", &InputComponent::Settings::InvertMouseY)
    .Field("GamepadLookSensitivity", &InputComponent::Settings::GamepadLookSensitivity)
    .Field("InvertGamepadLookY", &InputComponent::Settings::InvertGamepadLookY)
    .Field("MoveForwardKey", &InputComponent::Settings::MoveForwardKey)
    .Field("MoveBackwardKey", &InputComponent::Settings::MoveBackwardKey)
    .Field("MoveLeftKey", &InputComponent::Settings::MoveLeftKey)
    .Field("MoveRightKey", &InputComponent::Settings::MoveRightKey)
    .Field("JumpKey", &InputComponent::Settings::JumpKey)
    .Field("MoveGamepadXAxis", &InputComponent::Settings::MoveGamepadXAxis)
    .Field("MoveGamepadYAxis", &InputComponent::Settings::MoveGamepadYAxis)
    .Field("LookGamepadXAxis", &InputComponent::Settings::LookGamepadXAxis)
    .Field("LookGamepadYAxis", &InputComponent::Settings::LookGamepadYAxis)
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
    .Field("LocalPositionOffset", &CameraComponent::Settings::LocalPositionOffset)
    .Field("LocalRotationOffsetEuler", &CameraComponent::Settings::LocalRotationOffsetEuler)
    .Field("AutoActivateForPlayer", &CameraComponent::Settings::AutoActivateForPlayer)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SprintArmComponent::Settings, (TTypeBuilder<SprintArmComponent::Settings>(SprintArmComponent::Settings::kTypeName)
    .Field("Enabled", &SprintArmComponent::Settings::Enabled)
    .Field("DriveOwnerYaw", &SprintArmComponent::Settings::DriveOwnerYaw)
    .Field("ArmLength", &SprintArmComponent::Settings::ArmLength)
    .Field("SocketOffset", &SprintArmComponent::Settings::SocketOffset)
    .Field("YawDegrees", &SprintArmComponent::Settings::YawDegrees)
    .Field("PitchDegrees", &SprintArmComponent::Settings::PitchDegrees)
    .Field("MinPitchDegrees", &SprintArmComponent::Settings::MinPitchDegrees)
    .Field("MaxPitchDegrees", &SprintArmComponent::Settings::MaxPitchDegrees)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(SprintArmComponent, (TTypeBuilder<SprintArmComponent>(SprintArmComponent::kTypeName)
    .Field("Settings",
           &SprintArmComponent::EditSettings,
           &SprintArmComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Method("AddLookInput", &SprintArmComponent::AddLookInput)
    .Method("SetViewAngles", &SprintArmComponent::SetViewAngles)
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

SNAPI_REFLECT_TYPE(DirectionalLightComponent::Settings, (TTypeBuilder<DirectionalLightComponent::Settings>(DirectionalLightComponent::Settings::kTypeName)
    .Field("Enabled", &DirectionalLightComponent::Settings::Enabled, EFieldFlagBits::Replication)
    .Field("Direction", &DirectionalLightComponent::Settings::Direction, EFieldFlagBits::Replication)
    .Field("Color", &DirectionalLightComponent::Settings::Color, EFieldFlagBits::Replication)
    .Field("Intensity", &DirectionalLightComponent::Settings::Intensity, EFieldFlagBits::Replication)
    .Field("CastShadows", &DirectionalLightComponent::Settings::CastShadows, EFieldFlagBits::Replication)
    .Field("CascadeCount", &DirectionalLightComponent::Settings::CascadeCount, EFieldFlagBits::Replication)
    .Field("ShadowMapSize", &DirectionalLightComponent::Settings::ShadowMapSize, EFieldFlagBits::Replication)
    .Field("ShadowBias", &DirectionalLightComponent::Settings::ShadowBias, EFieldFlagBits::Replication)
    .Field("ShadowFarDistance", &DirectionalLightComponent::Settings::ShadowFarDistance, EFieldFlagBits::Replication)
    .Field("SoftnessFactor", &DirectionalLightComponent::Settings::SoftnessFactor, EFieldFlagBits::Replication)
    .Field("SoftShadows", &DirectionalLightComponent::Settings::SoftShadows, EFieldFlagBits::Replication)
    .Field("ContactHardening", &DirectionalLightComponent::Settings::ContactHardening, EFieldFlagBits::Replication)
    .Field("CascadeBlending", &DirectionalLightComponent::Settings::CascadeBlending, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(DirectionalLightComponent, (TTypeBuilder<DirectionalLightComponent>(DirectionalLightComponent::kTypeName)
    .Field("Settings",
           &DirectionalLightComponent::EditSettings,
           &DirectionalLightComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(EnvironmentCaptureComponent::Settings, (TTypeBuilder<EnvironmentCaptureComponent::Settings>(EnvironmentCaptureComponent::Settings::kTypeName)
    .Field("ViewportID", &EnvironmentCaptureComponent::Settings::ViewportID)
    .Field("FaceSize", &EnvironmentCaptureComponent::Settings::FaceSize)
    .Field("FacesPerFrame", &EnvironmentCaptureComponent::Settings::FacesPerFrame)
    .Field("Realtime", &EnvironmentCaptureComponent::Settings::Realtime)
    .Field("CaptureResourceNameOverride", &EnvironmentCaptureComponent::Settings::CaptureResourceNameOverride)
    .Field("ProjectionExtents", &EnvironmentCaptureComponent::Settings::ProjectionExtents)
    .Field("InfluenceExtents", &EnvironmentCaptureComponent::Settings::InfluenceExtents)
    .Field("Priority", &EnvironmentCaptureComponent::Settings::Priority)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(EnvironmentCaptureComponent, (TTypeBuilder<EnvironmentCaptureComponent>(EnvironmentCaptureComponent::kTypeName)
    .Field("Settings",
           &EnvironmentCaptureComponent::EditSettings,
           &EnvironmentCaptureComponent::GetSettings)
    .Field("CaptureState", &EnvironmentCaptureComponent::GetCaptureStateText)
    .Field("CapturedFaces", &EnvironmentCaptureComponent::GetCapturedFaces)
    .Method("Bake", &EnvironmentCaptureComponent::Bake, EMethodFlagBits::EditorAction)
    .Method("CancelCapture", &EnvironmentCaptureComponent::CancelCapture, EMethodFlagBits::EditorAction)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(StaticMeshComponent::Settings, (TTypeBuilder<StaticMeshComponent::Settings>(StaticMeshComponent::Settings::kTypeName)
    .Field("MeshPath", &StaticMeshComponent::Settings::MeshPath, EFieldFlagBits::Replication)
    .Field("Visible", &StaticMeshComponent::Settings::Visible, EFieldFlagBits::Replication)
    .Field("CastShadows", &StaticMeshComponent::Settings::CastShadows, EFieldFlagBits::Replication)
    .Field("SyncFromTransform", &StaticMeshComponent::Settings::SyncFromTransform)
    .Field("RegisterWithRenderer", &StaticMeshComponent::Settings::RegisterWithRenderer)
    .Field("MeshAsset", &StaticMeshComponent::Settings::MeshAsset, EFieldFlagBits::Replication)
    .Field("MaterialInstanceOverrides", &StaticMeshComponent::Settings::MaterialInstanceOverrides, EFieldFlagBits::Replication)
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
    .Field("MeshAsset", &SkeletalMeshComponent::Settings::MeshAsset, EFieldFlagBits::Replication)
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

    auto RegisterPlain = []<typename T>(const char* Name) {
        TypeInfo Info;
        Info.Id = TypeIdFromName(Name);
        Info.Name = Name;
        Info.Size = sizeof(T);
        Info.Align = alignof(T);
        Info.RuntimeOps = &GetTypeRuntimeOps<T>();
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
    RegisterPlain.operator()<Vec2>(TTypeNameV<Vec2>);
    RegisterPlain.operator()<Vec3>(TTypeNameV<Vec3>);
    RegisterPlain.operator()<Vec4>(TTypeNameV<Vec4>);
    RegisterPlain.operator()<Quat>(TTypeNameV<Quat>);
    RegisterPlain.operator()<NodeHandle>(TTypeNameV<NodeHandle>);
    RegisterPlain.operator()<ComponentHandle>(TTypeNameV<ComponentHandle>);
    RegisterEnum.operator()<Conduit::EBuiltinEntryPoint>(
        TTypeNameV<Conduit::EBuiltinEntryPoint>,
        false,
        {
            EnumValueInfo{"None", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::None)},
            EnumValueInfo{"OnCreate", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::OnCreate)},
            EnumValueInfo{"PreTick", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::PreTick)},
            EnumValueInfo{"Tick", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::Tick)},
            EnumValueInfo{"FixedTick", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::FixedTick)},
            EnumValueInfo{"LateTick", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::LateTick)},
            EnumValueInfo{"PostTick", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::PostTick)},
            EnumValueInfo{"OnDestroy", static_cast<std::uint64_t>(Conduit::EBuiltinEntryPoint::OnDestroy)},
        });
    RegisterEnum.operator()<Conduit::ESlotKind>(
        TTypeNameV<Conduit::ESlotKind>,
        false,
        {
            EnumValueInfo{"Value", static_cast<std::uint64_t>(Conduit::ESlotKind::Value)},
            EnumValueInfo{"Handle", static_cast<std::uint64_t>(Conduit::ESlotKind::Handle)},
        });
    RegisterEnum.operator()<Conduit::EGraphAssetNodeKind>(
        TTypeNameV<Conduit::EGraphAssetNodeKind>,
        false,
        {
            EnumValueInfo{"EntryPoint", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::EntryPoint)},
            EnumValueInfo{"Label", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::Label)},
            EnumValueInfo{"Constant", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::Constant)},
            EnumValueInfo{"VariableGet", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::VariableGet)},
            EnumValueInfo{"VariableSet", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::VariableSet)},
            EnumValueInfo{"UnaryIntrinsic", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::UnaryIntrinsic)},
            EnumValueInfo{"BinaryIntrinsic", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::BinaryIntrinsic)},
            EnumValueInfo{"Jump", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::Jump)},
            EnumValueInfo{"Branch", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::Branch)},
            EnumValueInfo{"SelfFieldRead", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::SelfFieldRead)},
            EnumValueInfo{"SelfFieldWrite", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::SelfFieldWrite)},
            EnumValueInfo{"SelfMethodCall", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::SelfMethodCall)},
            EnumValueInfo{"InstanceFieldRead", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::InstanceFieldRead)},
            EnumValueInfo{"InstanceFieldWrite", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::InstanceFieldWrite)},
            EnumValueInfo{"InstanceMethodCall", static_cast<std::uint64_t>(Conduit::EGraphAssetNodeKind::InstanceMethodCall)},
        });
    RegisterEnum.operator()<Conduit::EUnaryIntrinsicOp>(
        TTypeNameV<Conduit::EUnaryIntrinsicOp>,
        false,
        {
            EnumValueInfo{"LogicalNot", static_cast<std::uint64_t>(Conduit::EUnaryIntrinsicOp::LogicalNot)},
            EnumValueInfo{"Negate", static_cast<std::uint64_t>(Conduit::EUnaryIntrinsicOp::Negate)},
        });
    RegisterEnum.operator()<Conduit::EBinaryIntrinsicOp>(
        TTypeNameV<Conduit::EBinaryIntrinsicOp>,
        false,
        {
            EnumValueInfo{"Add", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Add)},
            EnumValueInfo{"Subtract", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Subtract)},
            EnumValueInfo{"Multiply", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Multiply)},
            EnumValueInfo{"Divide", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Divide)},
            EnumValueInfo{"Equal", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Equal)},
            EnumValueInfo{"NotEqual", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::NotEqual)},
            EnumValueInfo{"Less", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Less)},
            EnumValueInfo{"LessEqual", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::LessEqual)},
            EnumValueInfo{"Greater", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::Greater)},
            EnumValueInfo{"GreaterEqual", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::GreaterEqual)},
            EnumValueInfo{"LogicalAnd", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::LogicalAnd)},
            EnumValueInfo{"LogicalOr", static_cast<std::uint64_t>(Conduit::EBinaryIntrinsicOp::LogicalOr)},
        });
#if defined(SNAPI_GF_ENABLE_UI)
    RegisterPlain.operator()<SnAPI::UI::Color>(TTypeNameV<SnAPI::UI::Color>);
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    RegisterPlain.operator()<ECollisionFilterBits>(TTypeNameV<ECollisionFilterBits>);
    RegisterPlain.operator()<CollisionFilterFlags>(TTypeNameV<CollisionFilterFlags>);
    RegisterEnum.operator()<SnAPI::Physics::EBodyType>(
        TTypeNameV<SnAPI::Physics::EBodyType>,
        false,
        {
            EnumValueInfo{"Static", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Static)},
            EnumValueInfo{"Kinematic", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Kinematic)},
            EnumValueInfo{"Dynamic", static_cast<std::uint64_t>(SnAPI::Physics::EBodyType::Dynamic)},
        });
    RegisterPlain.operator()<SnAPI::Physics::EShapeType>(TTypeNameV<SnAPI::Physics::EShapeType>);
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
    RegisterPlain.operator()<SnAPI::Input::EKey>(TTypeNameV<SnAPI::Input::EKey>);
    RegisterPlain.operator()<SnAPI::Input::EGamepadAxis>(TTypeNameV<SnAPI::Input::EGamepadAxis>);
    RegisterPlain.operator()<SnAPI::Input::EGamepadButton>(TTypeNameV<SnAPI::Input::EGamepadButton>);
    RegisterPlain.operator()<SnAPI::Input::DeviceId>(TTypeNameV<SnAPI::Input::DeviceId>);
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
