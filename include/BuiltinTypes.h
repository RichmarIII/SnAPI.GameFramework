#pragma once

#include <cstdint>
#include <string>

#include "INode.h"
#include "IComponent.h"
#include "Math.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

SNAPI_DEFINE_TYPE_NAME(void, "void")
SNAPI_DEFINE_TYPE_NAME(bool, "bool")
SNAPI_DEFINE_TYPE_NAME(int, "int")
SNAPI_DEFINE_TYPE_NAME(unsigned int, "uint")
SNAPI_DEFINE_TYPE_NAME(std::uint64_t, "uint64")
SNAPI_DEFINE_TYPE_NAME(float, "float")
SNAPI_DEFINE_TYPE_NAME(double, "double")
SNAPI_DEFINE_TYPE_NAME(std::string, "std::string")
SNAPI_DEFINE_TYPE_NAME(Uuid, "SnAPI::GameFramework::Uuid")
SNAPI_DEFINE_TYPE_NAME(Vec3, "SnAPI::GameFramework::Vec3")
SNAPI_DEFINE_TYPE_NAME(NodeHandle, "SnAPI::GameFramework::NodeHandle")
SNAPI_DEFINE_TYPE_NAME(ComponentHandle, "SnAPI::GameFramework::ComponentHandle")

} // namespace SnAPI::GameFramework
