#pragma once

#include <cstdint>
#include <string>

#include "INode.h"
#include "IComponent.h"
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

/**
 * @brief Built-in type name registrations for reflection.
 * @remarks These are used by TypeIdFromName and Variant conversions.
 */
SNAPI_DEFINE_TYPE_NAME(void, "void")
SNAPI_DEFINE_TYPE_NAME(bool, "bool")
SNAPI_DEFINE_TYPE_NAME(int, "int")
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
