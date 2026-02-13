#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <Physics.h>

#include "CollisionFilters.h"
#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Collider definition component used by physics-backed nodes.
 */
class ColliderComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ColliderComponent";

    /**
     * @brief Shape/material/filter configuration for collider creation.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::ColliderComponent::Settings";

        SnAPI::Physics::EShapeType Shape = SnAPI::Physics::EShapeType::Box; /**< @brief Collider shape type. */

        Vec3 HalfExtent{0.5f, 0.5f, 0.5f}; /**< @brief Box half extents (Shape=Box). */
        float Radius = 0.5f; /**< @brief Sphere/capsule radius. */
        float HalfHeight = 0.5f; /**< @brief Capsule half height (without hemispheres). */

        Vec3 LocalPosition{}; /**< @brief Local shape offset from owning node transform. */
        Vec3 LocalRotation{}; /**< @brief Local rotation in radians (XYZ euler). */

        float Density = 1.0f; /**< @brief Density override used by backend body setup. */
        float Friction = 0.5f; /**< @brief Contact friction coefficient. */
        float Restitution = 0.1f; /**< @brief Contact restitution/bounciness. */

        CollisionLayerFlags Layer = CollisionLayerFlags(ECollisionFilterBits::WorldDynamic); /**< @brief Single selected layer channel. */
        CollisionMaskFlags Mask = kCollisionMaskAll; /**< @brief Collision mask channels. */
        bool IsTrigger = false; /**< @brief Sensor-only overlap mode. */
    };

    /** @brief Access settings (const). */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /** @brief Access settings for mutation. */
    Settings& EditSettings()
    {
        return m_settings;
    }

protected:
    Settings m_settings{}; /**< @brief Collider settings consumed by RigidBodyComponent build path. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
