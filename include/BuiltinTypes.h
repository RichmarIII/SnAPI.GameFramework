#pragma once

#include <cstdint>
#include <string>

#include "BaseNode.h"
#include "BaseComponent.h"
#include "Math.h"
#include "CollisionFilters.h"
#include "TypeName.h"
#include "Uuid.h"
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include <Physics.h>
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif
#if defined(SNAPI_GF_ENABLE_UI)
#include <UILayout.h>
#endif

namespace SnAPI::GameFramework
{

template<typename TBase>
class TSubClassOf;
template<typename TBase, typename TNameTag>
class TAssetRef;
class PawnBase;
struct StaticMeshAssetRuntime;
struct SkeletalMeshAssetRuntime;
struct MaterialInstanceAssetRuntime;
#if defined(SNAPI_GF_ENABLE_RENDERER)
class SSAOParamsNode;
class SSGIParamsNode;
class SSRParamsNode;
class BloomParamsNode;
class AtmosphereParamsNode;
class AtmosphereCompositeParamsNode;
class HeightFogParamsNode;
class ToneMapParamsNode;
class WorldRenderSettings;
#endif
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for pawn-class gameplay assets.
 */
using PawnBaseAssetRef = TAssetRef<PawnBase, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for runtime static-mesh assets.
 */
using StaticMeshAssetRef = TAssetRef<StaticMeshAssetRuntime, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for runtime skeletal-mesh assets.
 */
using SkeletalMeshAssetRef = TAssetRef<SkeletalMeshAssetRuntime, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for runtime material-instance assets.
 */
using MaterialInstanceAssetRef = TAssetRef<MaterialInstanceAssetRuntime, void>;
#if defined(SNAPI_GF_ENABLE_RENDERER)
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for SSAO parameter nodes.
 */
using SSAOParamsNodeAssetRef = TAssetRef<SSAOParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for SSGI parameter nodes.
 */
using SSGIParamsNodeAssetRef = TAssetRef<SSGIParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for SSR parameter nodes.
 */
using SSRParamsNodeAssetRef = TAssetRef<SSRParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for bloom parameter nodes.
 */
using BloomParamsNodeAssetRef = TAssetRef<BloomParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for atmosphere parameter nodes.
 */
using AtmosphereParamsNodeAssetRef = TAssetRef<AtmosphereParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for atmosphere-composite parameter nodes.
 */
using AtmosphereCompositeParamsNodeAssetRef = TAssetRef<AtmosphereCompositeParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for height-fog parameter nodes.
 */
using HeightFogParamsNodeAssetRef = TAssetRef<HeightFogParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for tone-map parameter nodes.
 */
using ToneMapParamsNodeAssetRef = TAssetRef<ToneMapParamsNode, void>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Asset-reference alias for world render-settings assets.
 */
using WorldRenderSettingsAssetRef = TAssetRef<WorldRenderSettings, void>;
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief Built-in reflection type-name registrations.
 *
 * The `SNAPI_DEFINE_TYPE_NAME(...)` entries below provide stable textual names for the engine's
 * built-in primitive, math, handle, asset-reference, and optional subsystem-specific types.
 *
 * These names participate in:
 * - reflected `TypeId` lookup
 * - `Variant` conversions and diagnostics
 * - serialized or UI-facing type presentation
 *
 * The literal strings are part of the public reflection contract and should be treated as stable.
 */
SNAPI_DEFINE_TYPE_NAME(void, "void")
SNAPI_DEFINE_TYPE_NAME(bool, "bool")
SNAPI_DEFINE_TYPE_NAME(int, "int")
SNAPI_DEFINE_TYPE_NAME(std::int64_t, "int64")
SNAPI_DEFINE_TYPE_NAME(unsigned int, "uint")
SNAPI_DEFINE_TYPE_NAME(std::uint64_t, "uint64")
SNAPI_DEFINE_TYPE_NAME(float, "float")
SNAPI_DEFINE_TYPE_NAME(double, "double")
SNAPI_DEFINE_TYPE_NAME(std::string, "std::string")
SNAPI_DEFINE_TYPE_NAME(std::vector<uint8_t>, "std::vector<uint8_t>")
SNAPI_DEFINE_TYPE_NAME(Uuid, "SnAPI::GameFramework::Uuid")
SNAPI_DEFINE_TYPE_NAME(Vec2, "SnAPI::GameFramework::Vec2")
SNAPI_DEFINE_TYPE_NAME(Vec3, "SnAPI::GameFramework::Vec3")
SNAPI_DEFINE_TYPE_NAME(Vec4, "SnAPI::GameFramework::Vec4")
SNAPI_DEFINE_TYPE_NAME(Quat, "SnAPI::GameFramework::Quat")
SNAPI_DEFINE_TYPE_NAME(TSubClassOf<PawnBase>, "SnAPI::GameFramework::TSubClassOf<SnAPI::GameFramework::PawnBase>")
SNAPI_DEFINE_TYPE_NAME(PawnBaseAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::PawnBase>")
SNAPI_DEFINE_TYPE_NAME(StaticMeshAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::StaticMeshAssetRuntime>")
SNAPI_DEFINE_TYPE_NAME(SkeletalMeshAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::SkeletalMeshAssetRuntime>")
SNAPI_DEFINE_TYPE_NAME(MaterialInstanceAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::MaterialInstanceAssetRuntime>")
SNAPI_DEFINE_TYPE_NAME(std::vector<MaterialInstanceAssetRef>, "std::vector<SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::MaterialInstanceAssetRuntime>>")
#if defined(SNAPI_GF_ENABLE_RENDERER)
SNAPI_DEFINE_TYPE_NAME(SSAOParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::SSAOParamsNode>")
SNAPI_DEFINE_TYPE_NAME(SSGIParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::SSGIParamsNode>")
SNAPI_DEFINE_TYPE_NAME(SSRParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::SSRParamsNode>")
SNAPI_DEFINE_TYPE_NAME(BloomParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::BloomParamsNode>")
SNAPI_DEFINE_TYPE_NAME(AtmosphereParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::AtmosphereParamsNode>")
SNAPI_DEFINE_TYPE_NAME(AtmosphereCompositeParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::AtmosphereCompositeParamsNode>")
SNAPI_DEFINE_TYPE_NAME(HeightFogParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::HeightFogParamsNode>")
SNAPI_DEFINE_TYPE_NAME(ToneMapParamsNodeAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::ToneMapParamsNode>")
SNAPI_DEFINE_TYPE_NAME(WorldRenderSettingsAssetRef, "SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::WorldRenderSettings>")
#endif
SNAPI_DEFINE_TYPE_NAME(NodeHandle, "SnAPI::GameFramework::NodeHandle")
SNAPI_DEFINE_TYPE_NAME(ComponentHandle, "SnAPI::GameFramework::ComponentHandle")
#if defined(SNAPI_GF_ENABLE_UI)
SNAPI_DEFINE_TYPE_NAME(SnAPI::UI::Color, "SnAPI::UI::Color")
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
SNAPI_DEFINE_TYPE_NAME(ECollisionFilterBits, "SnAPI::GameFramework::ECollisionFilterBits")
SNAPI_DEFINE_TYPE_NAME(CollisionFilterFlags, "SnAPI::GameFramework::CollisionFilterFlags")
SNAPI_DEFINE_TYPE_NAME(SnAPI::Physics::EBodyType, "SnAPI::Physics::EBodyType")
SNAPI_DEFINE_TYPE_NAME(SnAPI::Physics::EShapeType, "SnAPI::Physics::EShapeType")
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
SNAPI_DEFINE_TYPE_NAME(SnAPI::Input::EKey, "SnAPI::Input::EKey")
SNAPI_DEFINE_TYPE_NAME(SnAPI::Input::EGamepadAxis, "SnAPI::Input::EGamepadAxis")
SNAPI_DEFINE_TYPE_NAME(SnAPI::Input::EGamepadButton, "SnAPI::Input::EGamepadButton")
SNAPI_DEFINE_TYPE_NAME(SnAPI::Input::DeviceId, "SnAPI::Input::DeviceId")
#endif

} // namespace SnAPI::GameFramework
