#pragma once

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Basic transform component (position, rotation, scale).
 * @remarks
 * Minimal spatial state component used by examples and built-in systems.
 * Rotation interpretation is intentionally engine-policy dependent (Euler degrees/radians,
 * local/world space) and should be treated as raw reflected data unless a system defines it.
 */
class TransformComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TransformComponent";

    Vec3 Position{}; /**< @brief Local position. */
    Vec3 Rotation{}; /**< @brief Local rotation (implementation-defined units). */
    Vec3 Scale{1.0f, 1.0f, 1.0f}; /**< @brief Local scale. */
};

} // namespace SnAPI::GameFramework
