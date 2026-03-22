#pragma once

/**
 * @file GameFramework.hpp
 * @brief Umbrella include and Doxygen entry point for the SnAPI.GameFramework module.
 *
 * @defgroup SnAPI_GameFramework SnAPI.GameFramework
 * @brief World/node/component gameplay framework with optional subsystem and editor integration.
 *
 * `SnAPI.GameFramework` is the high-level runtime layer used to assemble games from:
 * - a `GameRuntime` application host
 * - one `World` that owns graph storage and subsystem state
 * - `BaseNode`-derived objects that define hierarchy and identity
 * - `BaseComponent`-derived objects that attach behavior and data to nodes
 * - optional subsystem adapters for input, UI, audio, networking, physics, rendering, and scripting
 *
 * Mental model:
 * - `GameRuntime` owns startup/shutdown and per-frame orchestration.
 * - `World` is the authoritative owner of node/component storage and subsystem lifetimes.
 * - `NodeHandle` / `ComponentHandle` are the stable public identity boundary.
 * - Borrowed pointers are transient convenience views, not ownership.
 *
 * Threading:
 * - Most graph mutation APIs are main-thread only unless a specific type states otherwise.
 * - Handle copies are cheap value types, but borrowed pointers returned from handles or world lookups must not be cached.
 *
 * Include this header when you want the complete public API surface instead of selecting individual headers.
 */
#include "Assert.h"
#include "AuthoredAssetLoading.h"
#include "AuthoredAssetRegistry.h"
#include "AssetRef.h"
#include "BuiltinTypes.h"
#include "AudioListenerComponent.h"
#include "AudioSourceComponent.h"
#include "AudioSystem.h"
#include "CharacterMovementController.h"
#include "CameraComponent.h"
#include "CollisionFilters.h"
#include "ColliderComponent.h"
#include "ComponentTypeRegistry.h"
#include "Conduit.h"
#include "DirectionalLightComponent.h"
#include "EnvironmentCaptureComponent.h"
#include "EnvironmentProbeNode.h"
#include "DeferredShadingParamsNode.h"
#include "SprintArmComponent.h"
#include "Expected.h"
#include "FollowTargetComponent.h"
#include "FrameGraphNode.h"
#include "GameplayHost.h"
#include "GameplayRpcGateway.h"
#include "GameProjectRuntime.h"
#include "GameRuntime.h"
#include "IAsset.h"
#include "Handle.h"
#include "BaseComponent.h"
#include "IGame.h"
#include "IGameMode.h"
#include "IGameService.h"
#include "InputComponent.h"
#include "InputIntentComponent.h"
#include "InputSystem.h"
#include "UISystem.h"
#include "UIPropertyPanel.h"
#include "UIRenderViewport.h"
#include "IWorld.h"
#include "JobSystem.h"
#include "Level.h"
#include "LocalPlayer.h"
#include "LocalPlayerService.h"
#include "Math.h"
#include "Level.h"
#include "MultiplayerConfigNode.h"
#include "NodeAsset.h"
#include "PawnBase.h"
#include "PathResolver.h"
#include "PlayerStart.h"
#include "NetReplication.h"
#include "NetRpc.h"
#include "NetworkSystem.h"
#include "PhysicsSystem.h"
#include "RendererSystem.h"
#include "Relevance.h"
#include "Reflection.h"
#include "RigidBodyComponent.h"
#include "SSAOParamsNode.h"
#include "SSGIParamsNode.h"
#include "SSRParamsNode.h"
#include "TAAParamsNode.h"
#include "BloomParamsNode.h"
#include "AtmosphereParamsNode.h"
#include "AtmosphereCompositeParamsNode.h"
#include "HeightFogParamsNode.h"
#include "ToneMapParamsNode.h"
#include "WorldRenderSettings.h"
#include "SkeletalMeshComponent.h"
#include "StaticMeshComponent.h"
#include "SubClassOf.h"
#include "Serialization.h"
#include "ScriptABI.h"
#include "ScriptBindings.h"
#include "ScriptComponent.h"
#include "ScriptRuntime.h"
#include "TransformComponent.h"
#include "TypeAutoRegistration.h"
#include "TypeRegistration.h"
#include "Uuid.h"
#include "World.h"
#include "WorldEcsRuntime.h"

#include "AssetPipelineFactories.h"
#include "AssetPipelineIds.h"
#include "AssetPipelineSerializers.h"
