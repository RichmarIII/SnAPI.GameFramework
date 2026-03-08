#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <Physics.h>
#include <string_view>

#include "CollisionFilters.h"
#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-only collider definition consumed by `RigidBodyComponent`.
 *
 * `ColliderComponent` stores shape, material, and filter configuration for nodes that participate
 * in physics. The component does not create backend shapes by itself. Instead, sibling systems such
 * as `RigidBodyComponent` read its settings during body creation or recreation.
 *
 * Core semantics:
 * - The component is passive runtime data; it has no tick behavior.
 * - Changing settings does not automatically update an existing backend body at runtime.
 * - The editor-only property-change path does trigger sibling rigid-body recreation when possible.
 *
 * Ownership and lifetime:
 * - The component owns only serialized collider settings.
 * - Backend collider resources are owned by the physics body/scene created from those settings.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning `Settings::Layer` is consumed as a single selected collision layer. If multiple bits are
 * set, downstream code uses the least-significant set bit as the effective layer.
 *
 * @see RigidBodyComponent
 * @see CollisionLayerToIndex()
 * @see CollisionMaskFlags
 */
class ColliderComponent : public BaseComponent, public ComponentCRTP<ColliderComponent>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ColliderComponent";

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shape, material, and filter configuration used for collider creation.
     *
     * `RigidBodyComponent` reads these values when building a backend collider description. Mutating
     * them affects only future body creation/recreation.
     */
    struct Settings
    {
        /** @brief Stable reflected type name used for serialization registration. */
        static constexpr const char* kTypeName = "SnAPI::GameFramework::ColliderComponent::Settings";

        SnAPI::Physics::EShapeType Shape = SnAPI::Physics::EShapeType::Box; /**< @brief Backend shape type interpreted by the rigid-body build path. */

        Vec3 HalfExtent{0.5f, 0.5f, 0.5f}; /**< @brief Box half extents in local-space world units when `Shape == Box`. */
        float Radius = 0.5f; /**< @brief Sphere or capsule radius in local-space world units. */
        float HalfHeight = 0.5f; /**< @brief Capsule half-height in local-space world units, excluding the hemispherical end caps. */

        Vec3 LocalPosition{}; /**< @brief Local-space shape offset from the owning node origin, expressed in world units. */
        Vec3 LocalRotation{}; /**< @brief Local-space Euler rotation in radians, interpreted in XYZ order by the build path. */

        float Density = 1.0f; /**< @brief Density forwarded into backend collider/body setup. */
        float Friction = 0.5f; /**< @brief Contact friction coefficient forwarded to the physics backend. */
        float Restitution = 0.1f; /**< @brief Contact restitution/bounciness forwarded to the physics backend. */

        CollisionLayerFlags Layer = CollisionLayerFlags(ECollisionFilterBits::WorldDynamic); /**< @brief Logical collision layer. Downstream code expects one effective bit. */
        CollisionMaskFlags Mask = kCollisionMaskAll; /**< @brief Collision mask describing which layers this collider can query/collide against. */
        bool IsTrigger = false; /**< @brief Request trigger/sensor behavior instead of solid collision response. */
    };

    /**
     * @brief Read the current collider settings.
     * @return Borrowed reference to the stored settings object.
     */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /**
     * @brief Mutate the current collider settings.
     * @return Borrowed reference to the stored settings object.
     * @remarks Runtime backend changes occur only when a sibling rigid body is recreated.
     */
    Settings& EditSettings()
    {
        return m_settings;
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief React to editor-side collider edits.
     * @param Name Name of the changed reflected property.
     * @remarks Relevant property changes trigger sibling `RigidBodyComponent` recreation when present.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

protected:
    Settings m_settings{}; /**< @brief Collider settings consumed by RigidBodyComponent build path. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
