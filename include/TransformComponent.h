#pragma once

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Basic transform component (position, quaternion rotation, scale).
 * @remarks Minimal spatial state component used by examples and built-in systems.
 */
class TransformComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TransformComponent";

    Vec3 Position{}; /**< @brief Local position. */
    Quat Rotation = Quat::Identity(); /**< @brief Local rotation as quaternion. */
    Vec3 Scale{1.0f, 1.0f, 1.0f}; /**< @brief Local scale. */
};

} // namespace SnAPI::GameFramework
