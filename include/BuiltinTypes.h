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
class SSRParamsNode;
class BloomParamsNode;
class AtmosphereParamsNode;
class AtmosphereCompositeParamsNode;
class HeightFogParamsNode;
class ToneMapParamsNode;
class WorldRenderSettings;
#endif
using PawnBaseAssetRef = TAssetRef<PawnBase, void>;
using StaticMeshAssetRef = TAssetRef<StaticMeshAssetRuntime, void>;
using SkeletalMeshAssetRef = TAssetRef<SkeletalMeshAssetRuntime, void>;
using MaterialInstanceAssetRef = TAssetRef<MaterialInstanceAssetRuntime, void>;
#if defined(SNAPI_GF_ENABLE_RENDERER)
using SSAOParamsNodeAssetRef = TAssetRef<SSAOParamsNode, void>;
using SSRParamsNodeAssetRef = TAssetRef<SSRParamsNode, void>;
using BloomParamsNodeAssetRef = TAssetRef<BloomParamsNode, void>;
using AtmosphereParamsNodeAssetRef = TAssetRef<AtmosphereParamsNode, void>;
using AtmosphereCompositeParamsNodeAssetRef = TAssetRef<AtmosphereCompositeParamsNode, void>;
using HeightFogParamsNodeAssetRef = TAssetRef<HeightFogParamsNode, void>;
using ToneMapParamsNodeAssetRef = TAssetRef<ToneMapParamsNode, void>;
using WorldRenderSettingsAssetRef = TAssetRef<WorldRenderSettings, void>;
#endif

/**
 * @brief Built-in type name registrations for reflection.
 * @remarks These are used by TypeIdFromName and Variant conversions.
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
