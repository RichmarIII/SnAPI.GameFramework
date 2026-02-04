#pragma once

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

class TransformComponent : public IComponent
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TransformComponent";

    Vec3 Position{};
    Vec3 Rotation{};
    Vec3 Scale{1.0f, 1.0f, 1.0f};
};

} // namespace SnAPI::GameFramework
