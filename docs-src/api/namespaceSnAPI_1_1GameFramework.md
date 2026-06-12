# SnAPI::GameFramework

## Contents

- **Namespace:** SnAPI::GameFramework::detail
- **Namespace:** SnAPI::GameFramework::Editor
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{AssetRef.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{BaseComponent.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{BaseNode.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{FollowTargetComponent.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{GameplayHost.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{GameRuntime.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{InputIntentComponent.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{LocalPlayer.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{LocalPlayerService.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{PathResolver.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{RenderAssetPayloads.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePayloads.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{ScriptComponent.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{ScriptRuntime.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{World.cpp}
- **Type:** SnAPI::GameFramework::NodeAssetLoadParams
- **Type:** SnAPI::GameFramework::LevelAssetLoadParams
- **Type:** SnAPI::GameFramework::WorldAssetLoadParams
- **Type:** SnAPI::GameFramework::THasAssetRefDefaultName
- **Type:** SnAPI::GameFramework::THasAssetRefDefaultName< TTag, std::void_t< decltype(TTag::Value)> >
- **Type:** SnAPI::GameFramework::TAssetRef
- **Type:** SnAPI::GameFramework::BaseComponent
- **Type:** SnAPI::GameFramework::BaseNode
- **Type:** SnAPI::GameFramework::TSubClassOf
- **Type:** SnAPI::GameFramework::ComponentTypeRegistry
- **Type:** SnAPI::GameFramework::ComponentStorageView
- **Type:** SnAPI::GameFramework::TComponentStorage
- **Type:** SnAPI::GameFramework::Error
- **Type:** SnAPI::GameFramework::TExpectedRef
- **Type:** SnAPI::GameFramework::TFlags
- **Type:** SnAPI::GameFramework::EnableFlags
- **Type:** SnAPI::GameFramework::FollowTargetComponent
- **Type:** SnAPI::GameFramework::GameRuntimeGameplaySettings
- **Type:** SnAPI::GameFramework::GameplayHost
- **Type:** SnAPI::GameFramework::GameplayRpcGateway
- **Type:** SnAPI::GameFramework::GameRuntimeTickSettings
- **Type:** SnAPI::GameFramework::GameRuntimeSettings
- **Type:** SnAPI::GameFramework::GameRuntime
- **Type:** SnAPI::GameFramework::ITaskDispatcher
- **Type:** SnAPI::GameFramework::TaskDispatcherScope
- **Type:** SnAPI::GameFramework::TaskHandle
- **Type:** SnAPI::GameFramework::GameMutex
- **Type:** SnAPI::GameFramework::TSystemTaskQueue
- **Type:** SnAPI::GameFramework::THandle
- **Type:** SnAPI::GameFramework::HandleHash
- **Type:** SnAPI::GameFramework::IGame
- **Type:** SnAPI::GameFramework::IGameMode
- **Type:** SnAPI::GameFramework::IGameService
- **Type:** SnAPI::GameFramework::InputIntentComponent
- **Type:** SnAPI::GameFramework::TObjectPool
- **Type:** SnAPI::GameFramework::TDenseRuntimeHandle
- **Type:** SnAPI::GameFramework::IWorld
- **Type:** SnAPI::GameFramework::JobSystem
- **Type:** SnAPI::GameFramework::Level
- **Type:** SnAPI::GameFramework::LocalPlayer
- **Type:** SnAPI::GameFramework::LocalPlayerService
- **Type:** SnAPI::GameFramework::MultiplayerConfigNode
- **Type:** SnAPI::GameFramework::ObjectRegistry
- **Type:** SnAPI::GameFramework::SPathResolver
- **Type:** SnAPI::GameFramework::PawnBase
- **Type:** SnAPI::GameFramework::PlayerStart
- **Type:** SnAPI::GameFramework::RelevanceContext
- **Type:** SnAPI::GameFramework::RelevancePolicyRegistry
- **Type:** SnAPI::GameFramework::RelevanceComponent
- **Type:** SnAPI::GameFramework::AssimpImporterSettings
- **Type:** SnAPI::GameFramework::AssetRefPayload
- **Type:** SnAPI::GameFramework::MeshStreamChunkRef
- **Type:** SnAPI::GameFramework::StaticSubMeshPayload
- **Type:** SnAPI::GameFramework::StaticMeshPayload
- **Type:** SnAPI::GameFramework::SkeletalBonePayload
- **Type:** SnAPI::GameFramework::SkeletonPayload
- **Type:** SnAPI::GameFramework::AnimationKeyFramePayload
- **Type:** SnAPI::GameFramework::AnimationTrackPayload
- **Type:** SnAPI::GameFramework::AnimationPayload
- **Type:** SnAPI::GameFramework::SkeletalMeshPayload
- **Type:** SnAPI::GameFramework::MaterialPayload
- **Type:** SnAPI::GameFramework::MaterialScalarParamPayload
- **Type:** SnAPI::GameFramework::MaterialVectorParamPayload
- **Type:** SnAPI::GameFramework::MaterialTextureParamPayload
- **Type:** SnAPI::GameFramework::MaterialInstancePayload
- **Type:** SnAPI::GameFramework::TextureAssetRuntime
- **Type:** SnAPI::GameFramework::MaterialAssetRuntime
- **Type:** SnAPI::GameFramework::MaterialInstanceAssetRuntime
- **Type:** SnAPI::GameFramework::StaticMeshAssetRuntime
- **Type:** SnAPI::GameFramework::SkeletonAssetRuntime
- **Type:** SnAPI::GameFramework::AnimationAssetRuntime
- **Type:** SnAPI::GameFramework::SkeletalMeshAssetRuntime
- **Type:** SnAPI::GameFramework::MeshVertexStreamPayload
- **Type:** SnAPI::GameFramework::MeshImportSettingsPayload
- **Type:** SnAPI::GameFramework::StaticMeshSourcePayload
- **Type:** SnAPI::GameFramework::SkeletalMeshSourcePayload
- **Type:** SnAPI::GameFramework::ScriptBindings
- **Type:** SnAPI::GameFramework::ScriptComponent
- **Type:** SnAPI::GameFramework::ScriptInstanceContext
- **Type:** SnAPI::GameFramework::ScriptCreateInfo
- **Type:** SnAPI::GameFramework::IScript
- **Type:** SnAPI::GameFramework::IScriptEngineBackend
- **Type:** SnAPI::GameFramework::ScriptRuntimeService
- **Type:** SnAPI::GameFramework::TSerializationContext
- **Type:** SnAPI::GameFramework::TDeserializeOptions
- **Type:** SnAPI::GameFramework::TValueCodec
- **Type:** SnAPI::GameFramework::TValueCodec< TAssetRef< TBase, TNameTag > >
- **Type:** SnAPI::GameFramework::TValueCodec< std::vector< T > >
- **Type:** SnAPI::GameFramework::ValueCodecRegistry
- **Type:** SnAPI::GameFramework::ComponentSerializationRegistry
- **Type:** SnAPI::GameFramework::NodeComponentPayload
- **Type:** SnAPI::GameFramework::NodePayload
- **Type:** SnAPI::GameFramework::LevelPayload
- **Type:** SnAPI::GameFramework::WorldPayload
- **Type:** SnAPI::GameFramework::NodeSerializer
- **Type:** SnAPI::GameFramework::LevelSerializer
- **Type:** SnAPI::GameFramework::WorldSerializer
- **Type:** SnAPI::GameFramework::NodeTransform
- **Type:** SnAPI::GameFramework::TransformComponent
- **Type:** SnAPI::GameFramework::TTypeRegistrar
- **Type:** SnAPI::GameFramework::TypeAutoRegistry
- **Type:** SnAPI::GameFramework::TTypeBuilder
- **Type:** SnAPI::GameFramework::TTypeName
- **Type:** SnAPI::GameFramework::TransparentStringHash
- **Type:** SnAPI::GameFramework::TransparentStringEqual
- **Type:** SnAPI::GameFramework::EnableFlags< EFieldFlagBits >
- **Type:** SnAPI::GameFramework::EnableFlags< EMethodFlagBits >
- **Type:** SnAPI::GameFramework::FieldInfo
- **Type:** SnAPI::GameFramework::MethodInfo
- **Type:** SnAPI::GameFramework::ConstructorInfo
- **Type:** SnAPI::GameFramework::EnumValueInfo
- **Type:** SnAPI::GameFramework::TypeInfo
- **Type:** SnAPI::GameFramework::ReflectedFieldRef
- **Type:** SnAPI::GameFramework::ReflectedMethodRef
- **Type:** SnAPI::GameFramework::TypeRegistry
- **Type:** SnAPI::GameFramework::UuidParts
- **Type:** SnAPI::GameFramework::UuidHash
- **Type:** SnAPI::GameFramework::Variant
- **Type:** SnAPI::GameFramework::VariantView
- **Type:** SnAPI::GameFramework::WorldExecutionProfile
- **Type:** SnAPI::GameFramework::World
- **Type:** SnAPI::GameFramework::ScopedComponentOnCreateSuppression
- **Type:** SnAPI::GameFramework::TRuntimeTickCRTP
- **Type:** SnAPI::GameFramework::NodeCRTP
- **Type:** SnAPI::GameFramework::ComponentCRTP
- **Type:** SnAPI::GameFramework::TDenseRuntimeStorage
- **Type:** SnAPI::GameFramework::RuntimeNodeRecord
- **Type:** SnAPI::GameFramework::RuntimeComponentRecord
- **Type:** SnAPI::GameFramework::RuntimeNodeTransform
- **Type:** SnAPI::GameFramework::WorldNodeRuntime
- **Type:** SnAPI::GameFramework::WorldEcsRuntime

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TAssetManagerResolver = std::function<::SnAPI::AssetPipeline::AssetManager*()>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::RuntimeTextureAsset = TextureAssetRuntime`
</div>

## Variables

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindLevelName`

Asset kind name for Level assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindNodeName`

Asset kind name for Node assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindWorldName`

Asset kind name for World assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindStaticMeshName`

Asset kind name for StaticMesh assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindSkeletalMeshName`

Asset kind name for SkeletalMesh assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindMaterialName`

Asset kind name for Material assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindMaterialInstanceName`

Asset kind name for MaterialInstance assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindSkeletonName`

Asset kind name for Skeleton assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindAnimationName`

Asset kind name for Animation assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadNodeName`

Payload type name for Node cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadLevelName`

Payload type name for Level cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadWorldName`

Payload type name for World cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadStaticMeshName`

Payload type name for StaticMesh cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadSkeletalMeshName`

Payload type name for SkeletalMesh cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadMaterialName`

Payload type name for Material cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadMaterialInstanceName`

Payload type name for MaterialInstance cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadSkeletonName`

Payload type name for Skeleton cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadAnimationName`

Payload type name for Animation cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadStaticMeshSourceName`

Payload type name for StaticMesh source-intermediate data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadSkeletalMeshSourceName`

Payload type name for SkeletalMesh source-intermediate data.
</div>
<div class="snapi-api-card" markdown="1">
### `& SnAPI::GameFramework::TSubClassOf< PawnBase >::EditTypeId`
</div>
<div class="snapi-api-card" markdown="1">
### `& SnAPI::GameFramework::TAssetRef< PawnBase >::EditAssetId`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindLevel()`

Get the AssetPipeline TypeId for Level assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindNode()`

Get the AssetPipeline TypeId for Node assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindWorld()`

Get the AssetPipeline TypeId for World assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindStaticMesh()`

Get the AssetPipeline TypeId for StaticMesh assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindSkeletalMesh()`

Get the AssetPipeline TypeId for SkeletalMesh assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindMaterial()`

Get the AssetPipeline TypeId for Material assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindMaterialInstance()`

Get the AssetPipeline TypeId for MaterialInstance assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindSkeleton()`

Get the AssetPipeline TypeId for Skeleton assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindAnimation()`

Get the AssetPipeline TypeId for Animation assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadNode()`

Get the payload TypeId for Node payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadLevel()`

Get the payload TypeId for Level payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadWorld()`

Get the payload TypeId for World payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadStaticMesh()`

Get the payload TypeId for StaticMesh payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadSkeletalMesh()`

Get the payload TypeId for SkeletalMesh payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadMaterial()`

Get the payload TypeId for Material payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadMaterialInstance()`

Get the payload TypeId for MaterialInstance payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadSkeleton()`

Get the payload TypeId for Skeleton payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadAnimation()`

Get the payload TypeId for Animation payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadStaticMeshSource()`

Get the payload TypeId for StaticMesh source-intermediate payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadSkeletalMeshSource()`

Get the payload TypeId for SkeletalMesh source-intermediate payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::SNAPI_DEFINE_TYPE_NAME(Editor::ETextureCompressionTarget, "SnAPI.GameFramework.Editor.ETextureCompressionTarget")`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::SNAPI_DEFINE_TYPE_NAME(Editor::ETextureCompressionFormat, "SnAPI.GameFramework.Editor.ETextureCompressionFormat")`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Ok()`

Construct a success Result.

**Returns:** Result with no error.

**Notes**

- Use for functions returning Result.
</div>
<div class="snapi-api-card" markdown="1">
### `Error SnAPI::GameFramework::MakeError(EErrorCode Code, std::string Message)`

Construct an Error value.

**Parameters**

- `Code`: Error category.
- `Message`: Diagnostic message.

**Returns:** Error instance with the provided data.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter > SnAPI::GameFramework::CreateRenderAssetJsonImporter()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter > SnAPI::GameFramework::CreateRenderAssetAssimpImporter()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > SnAPI::GameFramework::CreateRenderMaterialCooker()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > SnAPI::GameFramework::CreateRenderMaterialInstanceCooker()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > SnAPI::GameFramework::CreateRenderSkeletonCooker()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > SnAPI::GameFramework::CreateRenderAnimationCooker()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > SnAPI::GameFramework::CreateRenderStaticMeshCooker()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > SnAPI::GameFramework::CreateRenderSkeletalMeshCooker()`
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::RegisterAssetPipelinePlugin(::SnAPI::AssetPipeline::IPluginRegistrar &Registrar)`

Register the GameFramework AssetPipeline plugin.

**Parameters**

- `Registrar`: AssetPipeline plugin registrar.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("Name", &BaseNode::Name, &BaseNode::Name) .Method("OnPossess"`
</div>
<div class="snapi-api-card" markdown="1">
### `&BaseNode::OnPossess SnAPI::GameFramework::Method("OnUnpossess", &BaseNode::OnUnpossess) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Base< BaseNode >() .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `&PerfComponentB::m_value Constructor() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `EFieldFlagBits::Replication SnAPI::GameFramework::Field("PossessedNode", &LocalPlayer::EditPossessedNode, &LocalPlayer::GetPossessedNode, EFieldFlagBits::Replication) .Field("AcceptInput"`
</div>
<div class="snapi-api-card" markdown="1">
### `EFieldFlagBits::Replication EFieldFlagBits::Replication SnAPI::GameFramework::Field("OwnerConnectionId", &LocalPlayer::EditOwnerConnectionId, &LocalPlayer::GetOwnerConnectionId, EFieldFlagBits::Replication) .Method("ServerRequestPossess"`
</div>
<div class="snapi-api-card" markdown="1">
### `EFieldFlagBits::Replication EFieldFlagBits::Replication EMethodFlagBits::RpcReliable EMethodFlagBits::RpcNetServer SnAPI::GameFramework::Method("ServerRequestUnpossess", &LocalPlayer::ServerRequestUnpossess, EMethodFlagBits::RpcReliable|EMethodFlagBits::RpcNetServer) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("TypeName", &TSubClassOf< PawnBase >::EditTypeName, &TSubClassOf< PawnBase >::GetTypeName) .Field("TypeId"`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("AssetName", &TAssetRef< PawnBase >::EditAssetName, &TAssetRef< PawnBase >::GetAssetName) .Field("AssetId"`
</div>
<div class="snapi-api-card" markdown="1">
### `&PawnBase::OnPossess SnAPI::GameFramework::Method("OnUnpossess", &PawnBase::OnUnpossess) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `&MultiplayerConfigNode::GetLocalPlayerCount SnAPI::GameFramework::Field("Splitscreen", &MultiplayerConfigNode::EditSplitscreen, &MultiplayerConfigNode::GetSplitscreen) .Field("AutoJoinAdditionalLocalPlayers"`
</div>
<div class="snapi-api-card" markdown="1">
### `&MultiplayerConfigNode::GetLocalPlayerCount &MultiplayerConfigNode::GetAutoJoinAdditionalLocalPlayers SnAPI::GameFramework::Field("RequireGamepadForAdditionalPlayers", &MultiplayerConfigNode::EditRequireGamepadForAdditionalPlayers, &MultiplayerConfigNode::GetRequireGamepadForAdditionalPlayers) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `EMethodFlagBits::RpcReliable EMethodFlagBits::RpcNetServer SnAPI::GameFramework::Method("ServerRequestLeavePlayer", &GameplayRpcGateway::ServerRequestLeavePlayer, EMethodFlagBits::RpcReliable|EMethodFlagBits::RpcNetServer) .Method("ServerRequestLoadLevel"`
</div>
<div class="snapi-api-card" markdown="1">
### `EMethodFlagBits::RpcReliable EMethodFlagBits::RpcNetServer EMethodFlagBits::RpcReliable EMethodFlagBits::RpcNetServer SnAPI::GameFramework::Method("ServerRequestUnloadLevel", &GameplayRpcGateway::ServerRequestUnloadLevel, EMethodFlagBits::RpcReliable|EMethodFlagBits::RpcNetServer) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("Position", &TransformComponent::Position, EFieldFlagBits::Replication) .Field("Rotation"`
</div>
<div class="snapi-api-card" markdown="1">
### `EFieldFlagBits::Replication SnAPI::GameFramework::Field("Scale", &TransformComponent::Scale, EFieldFlagBits::Replication) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("Target", &FollowTargetComponent::Settings::Target) .Field("PositionOffset"`
</div>
<div class="snapi-api-card" markdown="1">
### `&FollowTargetComponent::Settings::PositionOffset SnAPI::GameFramework::Field("SyncPosition", &FollowTargetComponent::Settings::SyncPosition) .Field("SyncRotation"`
</div>
<div class="snapi-api-card" markdown="1">
### `&FollowTargetComponent::Settings::PositionOffset &FollowTargetComponent::Settings::SyncRotation SnAPI::GameFramework::Field("RotationOffset", &FollowTargetComponent::Settings::RotationOffset) .Field("PositionSmoothingHz"`
</div>
<div class="snapi-api-card" markdown="1">
### `&FollowTargetComponent::Settings::PositionOffset &FollowTargetComponent::Settings::SyncRotation &FollowTargetComponent::Settings::PositionSmoothingHz SnAPI::GameFramework::Field("RotationSmoothingHz", &FollowTargetComponent::Settings::RotationSmoothingHz) .Field("ResolveTargetByUuidFallback"`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("Settings", &FollowTargetComponent::EditSettings, &FollowTargetComponent::GetSettings, EFieldFlagBits::Replication) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("ScriptModule", &ScriptComponent::ScriptModule) .Field("ScriptType"`
</div>
<div class="snapi-api-card" markdown="1">
### `&ScriptComponent::ScriptType SnAPI::GameFramework::Field("Instance", &ScriptComponent::Instance) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, AssetRefPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MeshStreamChunkRef &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, StaticSubMeshPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, StaticMeshPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, SkeletalBonePayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, SkeletonPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, AnimationKeyFramePayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, AnimationTrackPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, AnimationPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, SkeletalMeshPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MaterialPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MaterialScalarParamPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MaterialVectorParamPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MaterialTextureParamPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MaterialInstancePayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MeshVertexStreamPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, MeshImportSettingsPayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, StaticMeshSourcePayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &Ar, SkeletalMeshSourcePayload &Value)`

**Parameters**

- `Ar`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, NodeComponentPayload &Value)`

cereal serialize for NodeComponentPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, NodePayload &Value)`

cereal serialize for NodePayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, LevelPayload &Value)`

cereal serialize for LevelPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, WorldPayload &Value)`

cereal serialize for WorldPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
